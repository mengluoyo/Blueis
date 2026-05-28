#ifndef BLUEIS_COMMON_H
#define BLUEIS_COMMON_H

#include <any>
#include <string>
#include <unordered_map>
#include <vector>

namespace blueis {

// 数据类型枚举
enum class DataType {
    String,
    Hash,
    List,    // v0.2
    Set,     // v0.2
    ZSet,    // v0.2
};

// 列定义
struct ColumnDef {
    std::string name;
    std::string type;   // INT, VARCHAR, BOOL, FLOAT
    int size = 0;
    bool is_primary = false;
};

// 表元数据
struct TableMeta {
    std::string name;
    std::vector<ColumnDef> columns;
    std::string primary_key;
};

// 数据行
using DataRow = std::unordered_map<std::string, std::any>;

// 命令类型（协议解析后分发给不同引擎）
enum class CommandType {
    SQL,            // SQL 语句
    Redis,          // Redis 风格命令
    Unknown,
};

// 解析后的命令
struct Command {
    CommandType type = CommandType::Unknown;
    std::string raw;            // 原始文本
    std::vector<std::string> tokens;  // token 化后的命令

    // Redis 命令快捷访问
    const std::string& cmd() const { return tokens.empty() ? empty : tokens[0]; }
    const std::string& arg(size_t i) const { return i < tokens.size() ? tokens[i] : empty; }
    size_t arg_size() const { return tokens.size(); }

private:
    inline static const std::string empty;
};

// 响应状态
enum class Status {
    OK,
    Err,
    Empty,
};

struct Response {
    Status status = Status::OK;
    std::string message;
};

} // namespace blueis

#endif // BLUEIS_COMMON_H
