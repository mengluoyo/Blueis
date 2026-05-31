#include "aof.h"
#include "../storage/engine.h"
#include <fstream>
#include <iostream>

namespace blueis {

Aof& Aof::instance() {
    static Aof inst;
    return inst;
}

void Aof::open(const std::string& path) {
    std::lock_guard lock(m_mutex);
    m_path = path;
    m_file.open(path, std::ios::app | std::ios::out); // 追加写模式（append），不会覆盖原有内容，直接追加到文件末尾
    if (!m_file) {
        std::cerr << "[WARN] AOF: cannot open " << path << std::endl;
    }
}

void Aof::append(const std::string& command) {
    std::lock_guard lock(m_mutex);
    if (!m_file.is_open()) return;
    m_file << command << "\n";
    m_file.flush();   // 强制刷盘，保证数据不丢
}

void Aof::load(const std::string& path,
               const std::function<void(const std::string&)>& execute) {
    std::ifstream ifs(path);
    if (!ifs) {
        std::cout << "[INFO] AOF: no existing AOF file, skip load" << std::endl;
        return;
    }

    int count = 0;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        execute(line);
        ++count;
    }
    ifs.close();
    std::cout << "[INFO] AOF loaded: " << count << " commands replayed" << std::endl;
}

void Aof::rewrite(StorageEngine& store, const std::string& path) {
    std::string tmp = path + ".rewrite";
    std::ofstream ofs(tmp, std::ios::out | std::ios::trunc);
    if (!ofs) {
        std::cerr << "[WARN] AOF: cannot rewrite to " << tmp << std::endl;
        return;
    }

    int count = 0;

    // String 数据
    auto string_keys = store.keys("*");
    for (const auto& k : string_keys) {
        auto v = store.get(k);
        if (v.has_value() && !v.value().empty()) {
            // 用引号保护含空格的值
            ofs << "SET " << k << " " << v.value() << "\n";
            ++count;
        }
        // 持久化过期信息 (Phase 7)
        auto exp = store.get_expire(k);
        if (exp.has_value()) {
            ofs << "EXPIREAT " << k << " " << (exp.value() / 1000) << "\n";
            ++count;
        }
    }

    // Hash 数据
    auto hash_keys = store.all_hash_names();
    for (const auto& hk : hash_keys) {
        auto all = store.hgetall(hk);
        for (const auto& [field, val] : all) {
            ofs << "HSET " << hk << " " << field << " " << val << "\n";
            ++count;
        }
        // 持久化过期信息 (Phase 7)
        auto exp = store.get_expire(hk);
        if (exp.has_value()) {
            ofs << "EXPIREAT " << hk << " " << (exp.value() / 1000) << "\n";
            ++count;
        }
    }

    // 表数据
    auto table_names = store.show_tables();
    for (const auto& tname : table_names) {
        const auto* meta = store.get_table(tname);
        if (!meta) continue;

        // CREATE TABLE
        ofs << "CREATE TABLE " << tname << " (";
        for (size_t i = 0; i < meta->columns.size(); ++i) {
            if (i > 0) ofs << ", ";
            const auto& cd = meta->columns[i];
            ofs << cd.name << " " << cd.type;
            if (cd.size > 0) ofs << "(" << cd.size << ")";
            if (cd.is_primary) ofs << " PRIMARY KEY";
        }
        ofs << ")\n";
        ++count;

        // INSERT 行数据
        auto rows = store.get_all_rows(tname);
        for (const auto& row : rows) {
            ofs << "INSERT INTO " << tname << " (";
            for (size_t i = 0; i < meta->columns.size(); ++i) {
                if (i > 0) ofs << ", ";
                ofs << meta->columns[i].name;
            }
            ofs << ") VALUES (";
            for (size_t i = 0; i < meta->columns.size(); ++i) {
                if (i > 0) ofs << ", ";
                ofs << StorageEngine::row_value_str(row, meta->columns[i].name);
            }
            ofs << ")\n";
            ++count;
        }
    }

    ofs.close();

    // Windows 下需先关闭已打开的文件句柄才能 rename
    {
        std::lock_guard lock(m_mutex);
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    // 原子替换：删旧文件，重命名新文件，重新打开
    std::remove(path.c_str());
    if (std::rename(tmp.c_str(), path.c_str()) == 0) {
        std::cout << "[INFO] AOF rewrite: " << count
                  << " commands written to " << path << std::endl;
    } else {
        std::cerr << "[WARN] AOF rewrite: rename failed" << std::endl;
    }

    // 重新以追加模式打开
    m_file.open(path, std::ios::app | std::ios::out);
}

void Aof::close() {
    std::lock_guard lock(m_mutex);
    if (m_file.is_open()) {
        m_file.close();
    }
}

} // namespace blueis
