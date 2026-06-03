#ifndef BLUEIS_PERSISTENCE_RDB_H
#define BLUEIS_PERSISTENCE_RDB_H

#include <string>
#include <cstdint>

namespace blueis {

class StorageEngine;

class Rdb {
public:
    static Rdb& instance();

    // 同步保存（阻塞）
    void save(const std::string& path);

    // 后台保存（不阻塞）
    void bgsave(const std::string& path);

    // 加载 RDB 文件
    bool load(const std::string& path);

    // 关闭
    void close();

private:
    Rdb() = default;

    // 写入辅助方法
    void write_magic(std::ofstream& fs);
    void write_version(std::ofstream& fs);
    void write_string(std::ofstream& fs, const std::string& s);
    void write_string_pair(std::ofstream& fs, const std::string& k, const std::string& v);
    void write_hash(std::ofstream& fs, const std::string& key);
    void write_tables(std::ofstream& fs, StorageEngine& store);
    void write_expires(std::ofstream& fs, StorageEngine& store);
    void write_checksum(std::ofstream& fs);

    // 读取辅助方法
    bool read_magic(std::ifstream& fs);
    bool read_version(std::ifstream& fs);
    std::string read_string(std::ifstream& fs);
    void read_string_pair(std::ifstream& fs);
    void read_hash(std::ifstream& fs);
    void read_tables(std::ifstream& fs);
    void read_expires(std::ifstream& fs);

    static constexpr const char* MAGIC = "BLUEISRDB";
    static constexpr uint32_t VERSION = 1;
};

} // namespace blueis

#endif // BLUEIS_PERSISTENCE_RDB_H
