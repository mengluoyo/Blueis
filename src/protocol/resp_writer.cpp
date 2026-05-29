#include "resp_writer.h"
#include <string>

namespace blueis {

std::string RespWriter::to_resp(const std::string& result) {
    if (result.empty()) {
        return "$-1\r\n";
    }

    char prefix = result[0];

    // 简单字符串: +OK → +OK\r\n
    if (prefix == '+') {
        return result + "\r\n";
    }

    // 错误: -ERR ... → -ERR ...\r\n
    if (prefix == '-') {
        return result + "\r\n";
    }

    // 整数: :1 → :1\r\n
    if (prefix == ':') {
        return result + "\r\n";
    }

    // 批量字符串: $hello → $5\r\nhello\r\n, $-1 → $-1\r\n
    if (prefix == '$') {
        if (result == "$-1") {
            return "$-1\r\n";
        }
        // 取 $ 后面的内容
        std::string content = result.substr(1);
        return "$" + std::to_string(content.size()) + "\r\n"
               + content + "\r\n";
    }

    // (empty) → 空数组
    if (result == "(empty)") {
        return "*0\r\n";
    }

    // 多行文本（KEYS / HGETALL / SQL 表格等）→ 整体包成批量字符串
    return "$" + std::to_string(result.size()) + "\r\n"
           + result + "\r\n";
}

} // namespace blueis
