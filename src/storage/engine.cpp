#include "engine.h"
#include <any>
#include <chrono>
#include <cstdint>
#include <mutex>

namespace blueis {

StorageEngine& StorageEngine::instance() {
    static StorageEngine inst;
    return inst;
}

// ============================================================
// String 操作
// ============================================================

void StorageEngine::set(const std::string& key, const std::string& value) {
    std::unique_lock lock(m_mutex);
    m_strings[key] = value;
}

std::optional<std::string> StorageEngine::get(const std::string& key) const {
    std::shared_lock lock(m_mutex);
    // 惰性删除：如果 key 已过期，直接返回不存在
    if (check_expired(key)) return std::nullopt;
    auto it = m_strings.find(key);
    if (it == m_strings.end()) return std::nullopt;
    return it->second;
}

bool StorageEngine::del(const std::string& key) {
    std::unique_lock lock(m_mutex);
    m_expires.erase(key);  // 同步清理过期记录
    return m_strings.erase(key) > 0 || m_hashes.erase(key) > 0;
}

bool StorageEngine::exists(const std::string& key) const {
    std::shared_lock lock(m_mutex);
    if (check_expired(key)) return false;
    return m_strings.find(key) != m_strings.end() ||
           m_hashes.find(key) != m_hashes.end();
}

std::vector<std::string> StorageEngine::keys(const std::string& pattern) const {
    std::shared_lock lock(m_mutex);
    std::vector<std::string> result;
    for (const auto& [k, v] : m_strings) {
        if (check_expired(k)) continue;
        if (match_pattern(k, pattern)) result.push_back(k);
    }
    return result;
}

// ============================================================
// 表管理
// ============================================================

void StorageEngine::create_table(const TableMeta& meta) {
    std::unique_lock lock(m_mutex);
    m_tables[meta.name] = meta;
}

void StorageEngine::drop_table(const std::string& name) {
    std::unique_lock lock(m_mutex);
    m_tables.erase(name);
    m_rows.erase(name);
}

const TableMeta* StorageEngine::get_table(const std::string& name) const {
    std::shared_lock lock(m_mutex);
    auto it = m_tables.find(name);
    return it == m_tables.end() ? nullptr : &it->second;
}

std::vector<std::string> StorageEngine::show_tables() const {
    std::shared_lock lock(m_mutex);
    std::vector<std::string> names;
    for (const auto& [k, v] : m_tables) names.push_back(k);
    return names;
}

// ============================================================
// 行操作
// ============================================================

std::string StorageEngine::make_row_key(const std::string& table, const std::string& pk) const {
    return table + ":" + pk;
}

void StorageEngine::insert_row(const std::string& table, const DataRow& row, const std::string& pk_col) {
    std::unique_lock lock(m_mutex);
    auto pk_it = row.find(pk_col);
    if (pk_it == row.end()) {
        return; // 主键不存在
    }
    std::string pk;
    if (pk_it->second.type() == typeid(std::string)) {
        pk = std::any_cast<std::string>(pk_it->second);
    } else if (pk_it->second.type() == typeid(int64_t)) {
        pk = std::to_string(std::any_cast<int64_t>(pk_it->second));
    } else if (pk_it->second.type() == typeid(int)) {
        pk = std::to_string(std::any_cast<int>(pk_it->second));
    }
    std::string key = table + ":" + pk;
    m_rows[table][key] = row;
}

std::vector<DataRow> StorageEngine::get_all_rows(const std::string& table) const {
    std::shared_lock lock(m_mutex);
    auto tit = m_rows.find(table);
    if (tit == m_rows.end()) return {};
    std::vector<DataRow> result;
    for (const auto& [k, v] : tit->second) {
        result.push_back(v);
    }
    return result;
}

void StorageEngine::delete_row(const std::string& table, const std::string& pk) {
    std::unique_lock lock(m_mutex);
    std::string key = table + ":" + pk;
    auto tit = m_rows.find(table);
    if (tit != m_rows.end()) {
        tit->second.erase(key);
    }
}

int StorageEngine::delete_all_rows(const std::string& table) {
    std::unique_lock lock(m_mutex);
    auto tit = m_rows.find(table);
    if (tit == m_rows.end()) return 0;
    int n = static_cast<int>(tit->second.size());
    tit->second.clear();
    return n;
}

// ============================================================
// Hash 操作
// ============================================================

/**
 * @brief 设置哈希表中的指定字段的值
 * @param key   哈希表的名称
 * @param field 字段名
 * @param value 字段对应的值
 */
void StorageEngine::hset(const std::string& key, const std::string& field, const std::string& value) {
    std::unique_lock lock(m_mutex);
    m_hashes[key][field] = value;
}

std::optional<std::string> StorageEngine::hget(const std::string& key, const std::string& field) const {
    std::shared_lock lock(m_mutex);
    if (check_expired(key)) return std::nullopt;
    auto it = m_hashes.find(key);
    if (it == m_hashes.end()) return std::nullopt;
    auto fit = it->second.find(field);
    if (fit == it->second.end()) return std::nullopt;
    return fit->second;
}

bool StorageEngine::hdel(const std::string& key, const std::string& field) {
    std::unique_lock lock(m_mutex);
    auto it = m_hashes.find(key);
    if (it == m_hashes.end()) return false;
    return it->second.erase(field) > 0;
}

bool StorageEngine::hexists(const std::string& key, const std::string& field) const {
    std::shared_lock lock(m_mutex);
    if (check_expired(key)) return false;
    auto it = m_hashes.find(key);
    if (it == m_hashes.end()) return false;
    return it->second.find(field) != it->second.end();
}

std::vector<std::pair<std::string, std::string>> StorageEngine::hgetall(const std::string& key) const {
    std::shared_lock lock(m_mutex);
    if (check_expired(key)) return {};
    auto it = m_hashes.find(key);
    if (it == m_hashes.end()) return {};
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto& [k, v] : it->second) {
        result.emplace_back(k, v);
    }
    return result;
}

std::vector<std::string> StorageEngine::hkeys(const std::string& key) const {
    std::shared_lock lock(m_mutex);
    if (check_expired(key)) return {};
    auto it = m_hashes.find(key);
    if (it == m_hashes.end()) return {};
    std::vector<std::string> result;
    for (const auto& [k, v] : it->second) {
        result.push_back(k);
    }
    return result;
}

std::vector<std::string> StorageEngine::hash_keys() const {
    std::shared_lock lock(m_mutex);
    std::vector<std::string> result;
    for (const auto& [k, v] : m_hashes) result.push_back(k);
    return result;
}

std::vector<std::string> StorageEngine::all_table_names() const {
    return show_tables();
}

std::vector<std::string> StorageEngine::all_hash_names() const {
    return hash_keys();
}

// ============================================================
// DataRow 值提取
// ============================================================

std::string StorageEngine::row_value_str(const DataRow& row, const std::string& col) {
    auto it = row.find(col);
    if (it == row.end()) return "";
    try {
        if (it->second.type() == typeid(std::string)) {
            return std::any_cast<std::string>(it->second);
        }
        if (it->second.type() == typeid(int64_t)) {
            return std::to_string(std::any_cast<int64_t>(it->second));
        }
        if (it->second.type() == typeid(int)) {
            return std::to_string(std::any_cast<int>(it->second));
        }
        if (it->second.type() == typeid(double)) {
            return std::to_string(std::any_cast<double>(it->second));
        }
    } catch (...) {}
    return "";
}

int64_t StorageEngine::row_value_int(const DataRow& row, const std::string& col) {
    auto it = row.find(col);
    if (it == row.end()) return 0;
    try {
        if (it->second.type() == typeid(int64_t)) {
            return std::any_cast<int64_t>(it->second);
        }
        if (it->second.type() == typeid(int)) {
            return std::any_cast<int>(it->second);
        }
        if (it->second.type() == typeid(std::string)) {
            return std::stoll(std::any_cast<std::string>(it->second));
        }
    } catch (...) {}
    return 0;
}

// ============================================================
// 模式匹配
// ============================================================

bool StorageEngine::match_pattern(const std::string& key, const std::string& pattern) const {
    if (pattern == "*") return true;
    if (!pattern.empty() && pattern.back() == '*' && pattern.find('*') == pattern.size() - 1) {
        std::string prefix = pattern.substr(0, pattern.size() - 1);
        return key.compare(0, prefix.size(), prefix) == 0;
    }
    if (!pattern.empty() && pattern.front() == '*' && pattern.find('*') == 0) {
        std::string suffix = pattern.substr(1);
        return key.size() >= suffix.size() &&
               key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0;
    }
    return key == pattern;
}

// ============================================================
// 过期机制 (Phase 7)
// ============================================================

int64_t StorageEngine::now_ms() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

bool StorageEngine::check_expired(const std::string& key) const {
    auto it = m_expires.find(key);
    if (it == m_expires.end()) return false;
    return it->second <= now_ms();
}

bool StorageEngine::expire(const std::string& key, int64_t seconds) {
    std::unique_lock lock(m_mutex);
    // key 必须存在（String 或 Hash）
    if (m_strings.find(key) == m_strings.end() &&
        m_hashes.find(key) == m_hashes.end()) {
        return false;
    }
    m_expires[key] = now_ms() + seconds * 1000;
    return true;
}

bool StorageEngine::expireat(const std::string& key, int64_t timestamp_ms) {
    std::unique_lock lock(m_mutex);
    if (m_strings.find(key) == m_strings.end() &&
        m_hashes.find(key) == m_hashes.end()) {
        return false;
    }
    m_expires[key] = timestamp_ms;
    return true;
}

int64_t StorageEngine::ttl(const std::string& key) const {
    std::shared_lock lock(m_mutex);
    // key 不存在
    if (m_strings.find(key) == m_strings.end() &&
        m_hashes.find(key) == m_hashes.end()) {
        return -2;
    }
    auto it = m_expires.find(key);
    if (it == m_expires.end()) return -1;  // 永久
    int64_t remaining = it->second - now_ms();
    if (remaining <= 0) return -2;  // 已过期，视为不存在
    return (remaining + 999) / 1000;  // 向上取整到秒
}

int64_t StorageEngine::pttl(const std::string& key) const {
    std::shared_lock lock(m_mutex);
    if (m_strings.find(key) == m_strings.end() &&
        m_hashes.find(key) == m_hashes.end()) {
        return -2;
    }
    auto it = m_expires.find(key);
    if (it == m_expires.end()) return -1;
    int64_t remaining = it->second - now_ms();
    if (remaining <= 0) return -2;
    return remaining;
}

bool StorageEngine::persist(const std::string& key) {
    std::unique_lock lock(m_mutex);
    return m_expires.erase(key) > 0;
}

std::optional<int64_t> StorageEngine::get_expire(const std::string& key) const {
    std::shared_lock lock(m_mutex);
    auto it = m_expires.find(key);
    if (it == m_expires.end()) return std::nullopt;
    return it->second;
}

// ============================================================
// 后台定期删除 (Phase 7)
// ============================================================

void StorageEngine::start_expire_loop() {
    m_expire_running = true;
    m_expire_thread = std::thread(&StorageEngine::expire_loop, this);
}

void StorageEngine::stop_expire_loop() {
    m_expire_running = false;
    if (m_expire_thread.joinable()) {
        m_expire_thread.join();
    }
}

void StorageEngine::expire_loop() {
    while (m_expire_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::unique_lock lock(m_mutex);
        if (m_expires.empty()) continue;

        int64_t now = now_ms();
        int checked = 0;
        int expired = 0;
        int max_check = std::min(20, static_cast<int>(m_expires.size()));

        auto it = m_expires.begin();
        while (checked < max_check && it != m_expires.end()) {
            if (it->second <= now) {
                // 删除过期 key 的数据和过期记录
                m_strings.erase(it->first);
                m_hashes.erase(it->first);
                it = m_expires.erase(it);
                ++expired;
            } else {
                ++it;
            }
            ++checked;
        }

        // 过期比例 > 25% 时不休眠，继续下一轮加速清理
        if (expired > max_check / 4) {
            lock.unlock();
            // 不 sleep，直接进入下一轮 while 循环
        }
    }
}

} // namespace blueis
