#ifndef BLUEIS_PERSISTENCE_AOF_H
#define BLUEIS_PERSISTENCE_AOF_H

#include <functional>
#include <fstream>
#include <mutex>
#include <string>

namespace blueis {

class StorageEngine;

class Aof {
public:
    static Aof& instance();

    // 打开 AOF 文件（追加模式）
    void open(const std::string& path);

    // 追加一条写命令，自动 flush
    void append(const std::string& command);

    // 加载 AOF 文件，逐条回调给 server 执行
    void load(const std::string& path,
              const std::function<void(const std::string&)>& execute);

    // 用当前内存数据重写 AOF（压缩）
    void rewrite(StorageEngine& store, const std::string& path);

    // 关闭 AOF 文件
    void close();

private:
    Aof() = default;

    std::mutex m_mutex;
    std::ofstream m_file;
    std::string m_path;
};

} // namespace blueis

#endif // BLUEIS_PERSISTENCE_AOF_H
