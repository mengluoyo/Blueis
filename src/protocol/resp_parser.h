#ifndef BLUEIS_PROTOCOL_RESP_PARSER_H
#define BLUEIS_PROTOCOL_RESP_PARSER_H

#include "common.h"
#include <string>
#include <vector>

namespace blueis{

class RespParser{
public:
    Command parse(const char *data, int len);

private:
    const char *m_ptr;  // 指向当前读取字符
    const char *m_end;  // 指向缓冲字符末尾

    void saveToken(std::vector<std::string> &tokens);
    std::vector<std::string> tokenize(const char *data);
};

}  // namespace blueis

#endif  // BLUEIS_PROTOCOL_RESP_PARSER_H