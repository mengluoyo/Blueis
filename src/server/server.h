#ifndef BLUEIS_SERVER_SERVER_H
#define BLUEIS_SERVER_SERVER_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#endif

// 终端文字重绘
void redraw(SOCKET& socket, std::string& linebuf, int& index);
// 右移移动光标
void moveCursorToEnd(SOCKET& socket, int distance);
// 左移移动光标
void moveCursorToHome(SOCKET& socket, int distance);

namespace blueis {

class TcpServer {
public:
    explicit TcpServer(uint16_t port = 6380);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    bool start();
    void run();
    void stop();

private:
    void handle_client(SOCKET client_socket);
    std::string process_command(const std::string& raw, bool record_aof = true);
    std::string execute_redis(const class Command& cmd);
    std::string execute_sql(const class Command& cmd);

    void cleanup_threads();

    // 自动保存
    void start_auto_save();
    void stop_auto_save();
    void auto_save_loop();
    void trigger_auto_save();
    void record_write() { m_write_count++; }
    
    // SQL,Redis 写命令记录
    bool is_write_command(const std::string& keyword);

    uint16_t m_port;
    SOCKET m_listen_socket = INVALID_SOCKET;
    std::atomic<bool> m_running{false};
    std::mutex m_threads_mutex;
    std::vector<std::thread> m_threads;

    // 自动保存配置
    bool m_auto_save_enabled = true;     // 默认开启自动保存
    int m_auto_save_interval = 60;        // 自动保存间隔（秒），默认60秒
    std::atomic<int> m_write_count{0};    // 写命令计数
    std::thread m_auto_save_thread;
};

} // namespace blueis

#endif // BLUEIS_SERVER_SERVER_H
