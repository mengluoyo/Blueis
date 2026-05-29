#include <iostream>
#include "server.h"
#include "../protocol/command_parser.h"
#include "../protocol/resp_parser.h"
#include "../protocol/resp_writer.h"
#include "../sql/lexer.h"
#include "../sql/parser.h"
#include "../sql/executor.h"
#include "../sql/formatter.h"
#include "../storage/engine.h"
#include "../persistence/snapshot.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <winsock2.h>

using namespace std;

// 终端文字重绘
void redraw(SOCKET& socket, std::string& linebuf, int& index){
    send(socket, "\r\x1b[K", 4, 0);
    send(socket, linebuf.c_str(), linebuf.size(), 0);
    // 回到行首
    send(socket, "\r", 1, 0);
    moveCursorToEnd(socket, index);
    // std::string movePos = "\x1b[" + std::to_string(index) + "C";
    // send(socket, movePos.c_str(), movePos.size(), 0);
}

// 左移移动光标
void moveCursorToEnd(SOCKET& socket, int distance){
    // 回到行首
    send(socket, "\r", 1, 0);
    string movePos = "\x1b[" + to_string(distance) + "C";   // 右移
    send(socket, movePos.c_str(), movePos.size(), 0);
}

// 右移移动光标
void moveCursorToHome(SOCKET& socket, int distance){
    // 回到行首
    send(socket, "\r", 1, 0);
    string movePos = "\x1b[" + to_string(distance) + "D";   // 右移
    send(socket, movePos.c_str(), movePos.size(), 0);
}

namespace blueis {

// ============================================================
// 构造 / 析构
// ============================================================

TcpServer::TcpServer(uint16_t port)
    : m_port(port) {}

TcpServer::~TcpServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

// ============================================================
// 启动
// ============================================================

bool TcpServer::start() {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "[ERROR] WSAStartup failed" << std::endl;
        return false;
    }
#endif

    m_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen_socket == INVALID_SOCKET) {
        std::cerr << "[ERROR] socket() failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return false;
    }

    // 允许端口复用，方便重启
    int opt = 1;
    // 设置端口复用选项，允许服务器程序重启时端口能快速复用
    // 参数含义：
    // - m_listen_socket: 监听的 socket 描述符
    // - SOL_SOCKET: 套接字层级（操作的是套接字本身的选项）
    // - SO_REUSEADDR: 要设置的选项（允许端口复用）
    // - opt: 设置的具体值（1，启用复用）
    // - sizeof(opt): 参数长度（以字节计）
    setsockopt(m_listen_socket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
       

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);

    if (bind(m_listen_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[ERROR] bind() failed: " << WSAGetLastError() << std::endl;
        closesocket(m_listen_socket);
        WSACleanup();
        return false;
    }

    if (listen(m_listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[ERROR] listen() failed: " << WSAGetLastError() << std::endl;
        closesocket(m_listen_socket);
        WSACleanup();
        return false;
    }

    m_running = true;

    // 启动时自动加载数据
    Snapshot::load("data/blueis.json");

    std::cout << "[INFO] Blueis listening on port " << m_port << std::endl;
    return true;
}

// ============================================================
// 主循环
// ============================================================

void TcpServer::run() {
    while (m_running) {
        sockaddr_in client_addr{};
        int addr_len = sizeof(client_addr);
        SOCKET client = accept(m_listen_socket,
                               reinterpret_cast<sockaddr*>(&client_addr),
                               &addr_len);
        if (client == INVALID_SOCKET) {
            if (m_running) {
                std::cerr << "[WARN] accept() failed: " << WSAGetLastError() << std::endl;
            }
            continue;
        }

        char* ip_str = inet_ntoa(client_addr.sin_addr);
        std::cout << "[INFO] client connected: " << ip_str << std::endl;

        // 存线程（可调试），同时清理已结束的线程
        {
            // 使用 std::lock_guard 实现自动加锁和解锁，确保 m_threads_mutex 在作用域内是独占的，防止多线程竞争访问 m_threads。
            // lock_guard 是一种轻量级的 RAII 机制锁，只要 lock_guard 对象存在，对应的互斥量(m_threads_mutex)就被锁住，可以防止数据竞争和不一致。
            std::lock_guard lock(m_threads_mutex);

            // 清理已结束的线程（joinable 为 false 的线程）
            m_threads.erase(
                std::remove_if(m_threads.begin(), m_threads.end(),
                    [](std::thread& t) {
                        if (!t.joinable()) return true;
                        return false;
                    }),
                m_threads.end());

            // 新建并保存当前连接的客户端线程
            m_threads.emplace_back(&TcpServer::handle_client, this, client);
        }
    }
    cleanup_threads();
}

void TcpServer::stop() {
    m_running = false;
    if (m_listen_socket != INVALID_SOCKET) {
        closesocket(m_listen_socket);
        m_listen_socket = INVALID_SOCKET;
    }
    cleanup_threads();
    Snapshot::save("data/blueis.json");
}

void TcpServer::cleanup_threads() {
    std::lock_guard lock(m_threads_mutex);
    for (auto& t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    m_threads.clear();
}

// ============================================================
// 客户端处理
// ============================================================

// 过滤 telnet 协商序列，返回普通数据长度
static int filter_telnet(SOCKET sock, char* buf, int len) {
    int w = 0;
    for (int r = 0; r < len; ++r) {
        unsigned char c = static_cast<unsigned char>(buf[r]);
        if (c == 0xFF) { // IAC — telnet 协商开始
            if (r + 1 >= len) break;
            unsigned char cmd = static_cast<unsigned char>(buf[r + 1]);
            if (cmd == 0xFF) {
                // 转义的 0xFF 数据字节
                buf[w++] = buf[r];
                r += 1;
            } else if (cmd >= 251 && cmd <= 254) {
                // IAC WILL/WONT/DO/DONT <opt> → 回复拒绝
                if (r + 2 < len) {
                    unsigned char opt = static_cast<unsigned char>(buf[r + 2]);
                    unsigned char reply[3] = {0xFF, 0, 0};
                    if (cmd == 251) reply[1] = 254;       // WILL → DONT
                    else if (cmd == 252) reply[1] = 253;  // WONT → DO
                    else if (cmd == 253) reply[1] = 252;  // DO   → WONT
                    else if (cmd == 254) reply[1] = 251;  // DONT → WILL
                    reply[2] = opt;
                    send(sock, reinterpret_cast<char*>(reply), 3, 0);
                    r += 2;
                }
            } else {
                // 不认识的 IAC 序列，跳过命令字节
                r += 1;
            }
        } else {
            buf[w++] = buf[r];
        }
    }
    return w;
}

void TcpServer::handle_client(SOCKET client_socket) {
    char buffer[4096];
    std::string sql_buf;    // 完整的sql语句
    std::string line_buf;   // 每行字符
    int buf_index = 0;      // 光标指向位置
    int buf_row = 0;        // 光标指向行数
    std::vector<std::string> vLineStr;  // 存储各行字符
    std::string last_line;  // 最后一行暂未被存储的字符
    bool in_esc = false;    // 处于方向键编辑状态
    bool welcomed = false;  // 是否已发送欢迎信息
    string esc_buf;         // 存储方向键字符

    while (m_running) {
        int received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            break;
        }

        // RESP 协议检测：首字节为 '*' 表示 redis-cli 发来的命令
        if (buffer[0] == '*') {
            RespParser resp_parser;
            Command cmd = resp_parser.parse(buffer, received);

            // 从 tokens 重建原始命令字符串（如 "set key value"）
            std::string raw;
            for (size_t i = 0; i < cmd.tokens.size(); ++i) {
                if (i > 0) raw += " ";
                raw += cmd.tokens[i];
            }

            std::string response = process_command(raw);
            RespWriter resp_writer;
            std::string resp_data = resp_writer.to_resp(response);
            send(client_socket, resp_data.c_str(),
                 static_cast<int>(resp_data.size()), 0);
            continue;
        }

        // 过滤 telnet 协商字节
        received = filter_telnet(client_socket, buffer, received);
        if (received <= 0) continue;

        // telnet 模式：首次连接发送欢迎信息
        if (!welcomed) {
            const char* welcome = "Blueis v1.0.0\r\nType your command:\r\n";
            send(client_socket, welcome, static_cast<int>(strlen(welcome)), 0);
            welcomed = true;
        }

        std::string data(buffer, received);

        for (char c : data) {
            if (c == '\b' || c == 0x7F) {
                if (!line_buf.empty()) {
                    if(buf_index == line_buf.size()){
                        line_buf.pop_back();
                        buf_index--;
                        // 回显退格效果
                        const char *bs = " \b";
                        send(client_socket, bs, 2, 0);    
                    }else{      // 中间字符删除
                        line_buf.erase(line_buf.begin() + buf_index - 1);
                        redraw(client_socket, line_buf, --buf_index);
                    }
                }
            } 
            else if (c == '\x1b' or in_esc == true) {
                if(!in_esc){
                    esc_buf.clear();
                }
                esc_buf += c;
                in_esc = true;
                if(esc_buf == "\x1b[D"){  // 左方向键
                    if(buf_index > 0){
                        send(client_socket, esc_buf.c_str(), esc_buf.size(), 0);
                        buf_index--;
                    }
                    // cout << "buf index: " << buf_index << "buf size: " << sql_buf.size() << endl;
                    in_esc = false;
                }else if (esc_buf == "\x1b[C") {    // 右方向键
                    if(buf_row < vLineStr.size()){
                        if(buf_index < vLineStr[buf_row].size()){
                            send(client_socket, esc_buf.c_str(), esc_buf.size(), 0);
                            buf_index++;
                        }
                    }else if(buf_index < line_buf.size()){
                        send(client_socket, esc_buf.c_str(), esc_buf.size(), 0);
                        buf_index++;
                    }
                    // cout << "buf index: " << buf_index << "buf size: " << sql_buf.size() << endl;
                    in_esc = false;
                }else if (esc_buf == "\x1b[A") {     // 处理上箭头
                    if(buf_row > 0){
                        send(client_socket, esc_buf.c_str(), esc_buf.size(), 0);
                        if(buf_row == vLineStr.size()){     // 当前光标指向最后一行
                            last_line = line_buf;       // 保存
                        }
                        line_buf.clear();
                        line_buf = vLineStr[--buf_row];
                        moveCursorToEnd(client_socket, line_buf.size());
                        // cout << "now line: " << buf_row << " buf: " << line_buf << endl;
                    }
                    in_esc = false;
                }else if(esc_buf == "\x1b[B"){       // 处理下箭头
                    if(buf_row < vLineStr.size()){
                        send(client_socket, esc_buf.c_str(), esc_buf.size(), 0);
                        vLineStr[buf_row++] = line_buf;
                        line_buf.clear();
                        // 替换可编辑栏
                        if(buf_row == vLineStr.size()){     // 光标指向最后一行
                            line_buf = last_line;
                        }else{      // 从vlinestr中读取
                            line_buf = vLineStr[buf_row];
                        }
                        moveCursorToEnd(client_socket, line_buf.size());
                        // cout << "now line: " << buf_row << " buf: " << line_buf << endl;
                    }
                    in_esc = false;
                }
            }
            else if (c == '\n') {
                if (!line_buf.empty() && line_buf.back() == '\r') {
                    line_buf.pop_back();
                    sql_buf.append(line_buf);
                    vLineStr.emplace_back(line_buf);
                    line_buf.clear();
                    buf_row++;              // 光标指向下一行
                    buf_index--;
                }

                if(sql_buf.empty() or sql_buf.back() != ';') {
                    buf_index = 0;
                    continue;
                }
                sql_buf.pop_back();         // 以;结尾，开始执行

                if(sql_buf == "exit"){
                    // 自动执行操作保存
                    process_command("save");
                    string bye = "bye";
                    send(client_socket, bye.c_str(), bye.size(), 0);
                    closesocket(client_socket);
                }
                
                if (!sql_buf.empty()) {
                    // cout << "command: " << sql_buf << "sizes: " << sql_buf.size() << endl;
                    std::string response = process_command(sql_buf);
                    response += "\r\n";
                    send(client_socket, response.c_str(),
                         static_cast<int>(response.size()), 0);
                }
                sql_buf.clear();
                vLineStr.clear();
                buf_row = 0;
                buf_index = 0;
            } else if (c == '\t') {
                line_buf += "    ";
                buf_index++;
            }
            else if (c >= ' ' || c == '\r') {
                if(c == '\r'){      // 按下回车输入\r\n字符，默认语句完整，将光标移至语句末尾
                    if(buf_row != vLineStr.size()){
                        line_buf.clear();
                        line_buf = last_line;   // 还原最后一行数据
                    }
                    buf_index = line_buf.size();
                }

                if(buf_index == line_buf.size()){
                    line_buf.insert(line_buf.end(), c);
                    buf_index++;
                }else if(buf_index < line_buf.size()){
                    line_buf.insert(line_buf.begin() + buf_index, c);
                    // 触发重绘
                    redraw(client_socket, line_buf, ++buf_index);
                }
            }
        }
    }

    closesocket(client_socket);
    std::cout << "[INFO] client disconnected" << std::endl;
}

// ============================================================
// 命令处理
// ============================================================

std::string TcpServer::process_command(const std::string& raw) {
    ProtocolParser parser;
    Command cmd = parser.parse(raw);

    if (cmd.type == CommandType::Unknown) {
        return "(empty command)";
    }
    if (cmd.type == CommandType::SQL) {
        return execute_sql(cmd);
    }
    return execute_redis(cmd);
}

// ============================================================
// Redis 命令执行
// ============================================================

std::string TcpServer::execute_redis(const Command& cmd) {
    const std::string& name = cmd.cmd();
    auto& store = StorageEngine::instance();

    if (name == "set") {
        if (cmd.arg_size() < 3) {
            return "-ERR wrong number of arguments for 'set' command";
        }
        store.set(cmd.arg(1), cmd.arg(2));
        return "+OK";
    }

    if (name == "get") {
        if (cmd.arg_size() < 2) {
            return "-ERR wrong number of arguments for 'get' command";
        }
        auto result = store.get(cmd.arg(1));
        if (result.has_value()) {
            return "$" + result.value();
        }
        return "$-1";
    }

    if (name == "del") {
        if (cmd.arg_size() < 2) {
            return "-ERR wrong number of arguments for 'del' command";
        }
        bool ok = store.del(cmd.arg(1));
        return ok ? ":1" : ":0";
    }

    if (name == "exists") {
        if (cmd.arg_size() < 2) {
            return "-ERR wrong number of arguments for 'exists' command";
        }
        bool ok = store.exists(cmd.arg(1));
        return ok ? ":1" : ":0";
    }

    if (name == "keys") {
        std::string pattern = (cmd.arg_size() >= 2) ? cmd.arg(1) : "*";
        auto result = store.keys(pattern);
        if (result.empty()) {
            return "(empty)";
        }
        std::ostringstream oss;
        for (size_t i = 0; i < result.size(); ++i) {
            if (i > 0) oss << "\r\n";
            oss << result[i];
        }
        return oss.str();
    }

    // === Hash 命令 ===

    if (name == "hset") {
        if (cmd.arg_size() < 4) {
            return "-ERR wrong number of arguments for 'hset' command";
        }
        store.hset(cmd.arg(1), cmd.arg(2), cmd.arg(3));
        return "+OK";
    }

    if (name == "hget") {
        if (cmd.arg_size() < 3) {
            return "-ERR wrong number of arguments for 'hget' command";
        }
        auto result = store.hget(cmd.arg(1), cmd.arg(2));
        if (result.has_value()) {
            return "$" + result.value();
        }
        return "$-1";
    }

    if (name == "hdel") {
        if (cmd.arg_size() < 3) {
            return "-ERR wrong number of arguments for 'hdel' command";
        }
        bool ok = store.hdel(cmd.arg(1), cmd.arg(2));
        return ok ? ":1" : ":0";
    }

    if (name == "hexists") {
        if (cmd.arg_size() < 3) {
            return "-ERR wrong number of arguments for 'hexists' command";
        }
        bool ok = store.hexists(cmd.arg(1), cmd.arg(2));
        return ok ? ":1" : ":0";
    }

    if (name == "hgetall") {
        if (cmd.arg_size() < 2) {
            return "-ERR wrong number of arguments for 'hgetall' command";
        }
        auto result = store.hgetall(cmd.arg(1));
        if (result.empty()) return "(empty)";
        std::ostringstream oss;
        for (size_t i = 0; i < result.size(); ++i) {
            if (i > 0) oss << "\r\n";
            oss << result[i].first << ": " << result[i].second;
        }
        return oss.str();
    }

    if (name == "hkeys") {
        if (cmd.arg_size() < 2) {
            return "-ERR wrong number of arguments for 'hkeys' command";
        }
        auto result = store.hkeys(cmd.arg(1));
        if (result.empty()) return "(empty)";
        std::ostringstream oss;
        for (size_t i = 0; i < result.size(); ++i) {
            if (i > 0) oss << "\r\n";
            oss << result[i];
        }
        return oss.str();
    }

    if (name == "save") {
        if (Snapshot::save("data/blueis.json")) {
            return "+OK";
        }
        return "-ERR save failed";
    }

    return "-ERR unknown command '" + name + "'";
}

// ============================================================
// SQL 命令（Phase 2 预留）
// ============================================================

std::string TcpServer::execute_sql(const Command& cmd) {
    try {
        sql::Lexer lexer(cmd.raw);
        sql::Parser parser(lexer);
        sql::Stmt stmt = parser.parse();
        sql::Executor exec(StorageEngine::instance());
        auto result = exec.execute(stmt);
        return sql::format_result(result);
    } catch (const std::exception& e) {
        return std::string("-ERR ") + e.what();
    }
}

} // namespace blueis
