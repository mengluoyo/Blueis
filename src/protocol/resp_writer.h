#ifndef BLUEIS_PROTOCOL_RESP_WRITER_H
#define BLUEIS_PROTOCOL_RESP_WRITER_H

#include <string>

namespace blueis {

class RespWriter {
public:
    // 将 execute_redis / execute_sql 的返回字符串转成标准 RESP 格式
    std::string to_resp(const std::string& result);
};

} // namespace blueis

#endif // BLUEIS_PROTOCOL_RESP_WRITER_H
