#include "rdb.h"
#include "../storage/engine.h"
#include <fstream>
#include <iostream>
#include <chrono>

namespace blueis {

Rdb& Rdb::instance() {
    static Rdb inst;
    return inst;
}

// ============================================================
// 写入相关
// ============================================================

void Rdb::save(const std::string& path) {
    std::ofstream fs(path, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!fs) {
        std::cerr << "[WARN] RDB: cannot open " << path << std::endl;
        return;
    }

    auto& store = StorageEngine::instance();

    // 1. Magic
    write_magic(fs);

    // 2. Version
    write_version(fs);

    // 3. String 数据
    auto string_keys = store.keys("*");
    for (const auto& k : string_keys) {
        auto v = store.get(k);
        if (v.has_value() && !v.value().empty()) {
            write_string_pair(fs, k, v.value());
        }
    }

    // 4. Hash 数据
    auto hash_names = store.all_hash_names();
    for (const auto& hk : hash_names) {
        write_hash(fs, hk);
    }

    // 5. Table 数据（按数据库分组）
    auto db_names = store.show_databases();
    for (const auto& db_name : db_names) {
        write_database_header(fs, db_name);
        store.use_database(db_name);
        write_tables(fs, store);
    }

    // 6. Expire 数据
    write_expires(fs, store);

    // 7. Checksum (预留)
    write_checksum(fs);

    fs.close();
    std::cout << "[INFO] RDB saved: " << path << std::endl;
}

void Rdb::bgsave(const std::string& path) {
    std::thread([this, path]() {
        save(path);
    }).detach();
    std::cout << "[INFO] RDB background save started" << std::endl;
}

void Rdb::write_magic(std::ofstream& fs) {
    fs.write(MAGIC, 9);
}

void Rdb::write_version(std::ofstream& fs) {
    uint32_t v = VERSION;
    fs.write(reinterpret_cast<const char*>(&v), 4);
}

void Rdb::write_string(std::ofstream& fs, const std::string& s) {
    // 长度前缀 (2 bytes) + 内容
    uint16_t len = static_cast<uint16_t>(s.size());
    fs.write(reinterpret_cast<const char*>(&len), 2);
    fs.write(s.data(), s.size());
}

void Rdb::write_string_pair(std::ofstream& fs, const std::string& k, const std::string& v) {
    // 类型标记: 0x01 = String
    uint8_t type = 0x01;
    fs.write(reinterpret_cast<const char*>(&type), 1);
    write_string(fs, k);
    write_string(fs, v);
}

void Rdb::write_hash(std::ofstream& fs, const std::string& key) {
    auto& store = StorageEngine::instance();
    auto all = store.hgetall(key);
    if (all.empty()) return;

    // 类型标记: 0x02 = Hash
    uint8_t type = 0x02;
    fs.write(reinterpret_cast<const char*>(&type), 1);

    // 写入 key
    write_string(fs, key);

    // 写入 field-count
    uint16_t count = static_cast<uint16_t>(all.size());
    fs.write(reinterpret_cast<const char*>(&count), 2);

    // 写入 field-value 对
    for (const auto& [field, val] : all) {
        write_string(fs, field);
        write_string(fs, val);
    }
}

void Rdb::write_tables(std::ofstream& fs, StorageEngine& store) {
    auto table_names = store.show_tables();

    for (const auto& tname : table_names) {
        const auto* meta = store.get_table(tname);
        if (!meta) continue;

        // 类型标记: 0x03 = Table
        uint8_t type = 0x03;
        fs.write(reinterpret_cast<const char*>(&type), 1);

        // 写入表名
        write_string(fs, tname);

        // 写入列数
        uint16_t col_count = static_cast<uint16_t>(meta->columns.size());
        fs.write(reinterpret_cast<const char*>(&col_count), 2);

        // 写入列定义
        for (const auto& col : meta->columns) {
            write_string(fs, col.name);
            write_string(fs, col.type);
            fs.write(reinterpret_cast<const char*>(&col.size), 4);
            fs.write(reinterpret_cast<const char*>(&col.is_primary), 1);
        }

        // 写入行数据
        auto rows = store.get_all_rows(tname);
        uint32_t row_count = static_cast<uint32_t>(rows.size());
        fs.write(reinterpret_cast<const char*>(&row_count), 4);

        for (const auto& row : rows) {
            for (const auto& col : meta->columns) {
                std::string val = StorageEngine::row_value_str(row, col.name);
                write_string(fs, val);
            }
        }
    }
}

void Rdb::write_database_header(std::ofstream& fs, const std::string& db_name) {
    // 类型标记: 0x05 = Database
    uint8_t type = 0x05;
    fs.write(reinterpret_cast<const char*>(&type), 1);
    write_string(fs, db_name);
}

void Rdb::write_checksum(std::ofstream& fs) {
    // 预留 CRC64 (8 bytes)，暂时写 0
    uint64_t crc = 0;
    fs.write(reinterpret_cast<const char*>(&crc), 8);
}

void Rdb::write_expires(std::ofstream& fs, StorageEngine& store) {
    // 遍历所有 string key，写入有过期时间的
    auto string_keys = store.keys("*");
    for (const auto& k : string_keys) {
        auto exp = store.get_expire(k);
        if (exp.has_value()) {
            // 类型标记: 0x04 = Expire
            uint8_t type = 0x04;
            fs.write(reinterpret_cast<const char*>(&type), 1);
            write_string(fs, k);
            int64_t ts = exp.value();
            fs.write(reinterpret_cast<const char*>(&ts), 8);
        }
    }

    // 遍历所有 hash key，写入有过期时间的
    auto hash_names = store.all_hash_names();
    for (const auto& hk : hash_names) {
        auto exp = store.get_expire(hk);
        if (exp.has_value()) {
            uint8_t type = 0x04;
            fs.write(reinterpret_cast<const char*>(&type), 1);
            write_string(fs, hk);
            int64_t ts = exp.value();
            fs.write(reinterpret_cast<const char*>(&ts), 8);
        }
    }
}

// ============================================================
// 读取相关
// ============================================================

bool Rdb::load(const std::string& path) {
    std::ifstream fs(path, std::ios::binary);
    if (!fs) {
        std::cout << "[INFO] RDB: no existing RDB file, skip load" << std::endl;
        return false;
    }

    // 1. Magic
    if (!read_magic(fs)) {
        std::cerr << "[WARN] RDB: invalid magic" << std::endl;
        return false;
    }

    // 2. Version
    if (!read_version(fs)) {
        std::cerr << "[WARN] RDB: invalid version" << std::endl;
        return false;
    }

    auto& store = StorageEngine::instance();

    // 3. 循环读取各个数据类型
    while (fs.peek() != EOF) {
        uint8_t type;
        fs.read(reinterpret_cast<char*>(&type), 1);
        if (fs.eof()) break;

        switch (type) {
            case 0x01: // String
                read_string_pair(fs);
                break;
            case 0x02: // Hash
                read_hash(fs);
                break;
            case 0x03: // Table
                read_tables(fs);
                break;
            case 0x04: // Expire
                read_expires(fs);
                break;
            case 0x05: // Database header
                {
                    std::string db_name = read_database_header(fs);
                    store.create_database(db_name);
                    store.use_database(db_name);
                }
                break;
            default:
                std::cerr << "[WARN] RDB: unknown type 0x"
                          << std::hex << static_cast<int>(type) << std::dec << std::endl;
                return false;
        }
    }

    fs.close();
    std::cout << "[INFO] RDB loaded: " << path << std::endl;
    return true;
}

bool Rdb::read_magic(std::ifstream& fs) {
    char magic[9];
    fs.read(magic, 9);
    return std::string(magic, 9) == MAGIC;
}

bool Rdb::read_version(std::ifstream& fs) {
    uint32_t v;
    fs.read(reinterpret_cast<char*>(&v), 4);
    return v == VERSION;
}

std::string Rdb::read_string(std::ifstream& fs) {
    uint16_t len;
    fs.read(reinterpret_cast<char*>(&len), 2);
    std::string s(len, '\0');
    fs.read(&s[0], len);
    return s;
}

void Rdb::read_string_pair(std::ifstream& fs) {
    std::string key = read_string(fs);
    std::string val = read_string(fs);
    StorageEngine::instance().set(key, val);
}

void Rdb::read_hash(std::ifstream& fs) {
    std::string key = read_string(fs);
    uint16_t count;
    fs.read(reinterpret_cast<char*>(&count), 2);

    auto& store = StorageEngine::instance();
    for (int i = 0; i < count; ++i) {
        std::string field = read_string(fs);
        std::string val = read_string(fs);
        store.hset(key, field, val);
    }
}

void Rdb::read_tables(std::ifstream& fs) {
    std::string table_name = read_string(fs);

    uint16_t col_count;
    fs.read(reinterpret_cast<char*>(&col_count), 2);

    // 构建列定义
    std::vector<ColumnDef> cols;
    for (int i = 0; i < col_count; ++i) {
        ColumnDef col;
        col.name = read_string(fs);
        col.type = read_string(fs);
        fs.read(reinterpret_cast<char*>(&col.size), 4);
        fs.read(reinterpret_cast<char*>(&col.is_primary), 1);
        cols.push_back(col);
    }

    // 创建表
    TableMeta meta;
    meta.name = table_name;
    meta.columns = cols;
    // 找主键
    for (const auto& c : cols) {
        if (c.is_primary) {
            meta.primary_key = c.name;
            break;
        }
    }
    StorageEngine::instance().create_table(meta);

    // 读取行数据
    uint32_t row_count;
    fs.read(reinterpret_cast<char*>(&row_count), 4);

    for (int i = 0; i < row_count; ++i) {
        DataRow row;
        for (const auto& col : cols) {
            std::string val = read_string(fs);
            // 根据类型解析值
            if (col.type == "INT") {
                try {
                    int64_t iv = std::stoll(val);
                    row[col.name] = iv;
                } catch (...) {
                    row[col.name] = static_cast<int64_t>(0);
                }
            } else {
                row[col.name] = val;
            }
        }
        StorageEngine::instance().insert_row(table_name, row, meta.primary_key);
    }
}

void Rdb::close() {
    // RDB 无需关闭（每次 save 都是新建文件）
}

void Rdb::read_expires(std::ifstream& fs) {
    std::string key = read_string(fs);
    int64_t ts;
    fs.read(reinterpret_cast<char*>(&ts), 8);
    // 只有过期时间在未来才恢复，否则跳过（已过期的不需要了）
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (ts > now) {
        StorageEngine::instance().expireat(key, ts);
    }
}

std::string Rdb::read_database_header(std::ifstream& fs) {
    return read_string(fs);
}

} // namespace blueis