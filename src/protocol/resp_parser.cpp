#include "resp_parser.h"
#include <string>
#include <vector>

using namespace std;

namespace blueis {

Command RespParser::parse(const char *data, int len){
    Command cmd;
    cmd.raw = string(data, len);

    m_ptr = data;   //  指向首地址
    m_end = data + len;     // 指向最后一个字符的后面

    cmd.tokens = tokenize(data);

    if (cmd.tokens.empty()) {
        cmd.type = CommandType::Unknown;
        return cmd;
    }

    // 支持RESP协议，在Redis中使用
    cmd.type = CommandType::Redis;
    return cmd;
}

void RespParser::saveToken(vector<string> &tokens){
    string token;
    while(*m_ptr != '\r' and m_ptr < m_end){
        char c = *m_ptr++;
        token += c;
    }
    tokens.emplace_back(token);
    // \r\n
    m_ptr += 2;
}

std::vector<std::string> RespParser::tokenize(const char *data){
    vector<string> tokens;
    // 先过滤掉*数组字符
    if(*m_ptr != '*'){
        return tokens;
    }

    while(*m_ptr != '\n')
        m_ptr++;
    ++m_ptr;    // 指向\n的下一个字符

    while(m_ptr < m_end){
        char c = *m_ptr;
        if(c == '$'){
            // 过滤字符
            while(*m_ptr != '\n')
                m_ptr++;
            ++m_ptr;    // 过滤\n
        }else if(c == '+' or c == '-' or c == ':'){
            ++m_ptr;
        }else {
            break;
        }
        saveToken(tokens);
    }
    return tokens;
}

}