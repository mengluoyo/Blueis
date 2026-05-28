#ifndef BLUEIS_STORAGE_ENGINE_H
#define BLUEIS_STORAGE_ENGINE_H

#include "common.h"
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace blueis {

class StorageEngine {
public:
    static StorageEngine& instance();

    StorageEngine(const StorageEngine&) = delete;
    StorageEngine& operator=(const StorageEngine&) = delete;

    // === String 操作 (Phase 1) ===
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);
    bool exists(const std::string& key) const;
    std::vector<std::string> keys(const std::string& pattern) const;

    // === 表管理 ===
    void create_table(const TableMeta& meta);
    void drop_table(const std::string& name);
    const TableMeta* get_table(const std::string& name) const;
    std::vector<std::string> show_tables() const;

    // === 行操作 ===
    std::string make_row_key(const std::string& table, const std::string& pk) const;
    /**
     * @brief 插入一行数据到指定表
     * @param table   表名
     * @param row     行数据（DataRow 类型）
     * @param pk_col  主键字段名
     */
    void insert_row(const std::string& table, const DataRow& row, const std::string& pk_col);
    std::vector<DataRow> get_all_rows(const std::string& table) const;
    void delete_row(const std::string& table, const std::string& pk);
    int delete_all_rows(const std::string& table);

    // === Hash 操作 (Phase 3) ===
    void hset(const std::string& key, const std::string& field, const std::string& value);
    std::optional<std::string> hget(const std::string& key, const std::string& field) const;
    bool hdel(const std::string& key, const std::string& field);
    bool hexists(const std::string& key, const std::string& field) const;
    std::vector<std::pair<std::string, std::string>> hgetall(const std::string& key) const;
    std::vector<std::string> hkeys(const std::string& key) const;
    std::vector<std::string> hash_keys() const;

    // === 序列化辅助 ===
    std::vector<std::string> all_table_names() const;
    std::vector<std::string> all_hash_names() const;

    // 获取 DataRow 中的值（类型转换）
    static std::string row_value_str(const DataRow& row, const std::string& col);
    static int64_t row_value_int(const DataRow& row, const std::string& col);

private:
    StorageEngine() = default;

    bool match_pattern(const std::string& key, const std::string& pattern) const;

    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, std::string> m_strings;     // Redis
    std::unordered_map<std::string, TableMeta> m_tables;        // SQL
    // key: "table:pkValue" → DataRow
    std::unordered_map<std::string, std::unordered_map<std::string, DataRow>> m_rows;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_hashes;
};

} // namespace blueis

#endif // BLUEIS_STORAGE_ENGINE_H
