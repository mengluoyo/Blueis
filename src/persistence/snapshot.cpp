#include "snapshot.h"
#include "json.h"
#include "../storage/engine.h"
#include <fstream>
#include <iostream>

namespace blueis {

bool Snapshot::save(const std::string& path) {
    auto& store = StorageEngine::instance();
    json::Writer w;

    {
        json::ObjectScope root(w);

        // strings
        w.key("strings");
        {
            json::ObjectScope obj(w);
            auto keys = store.keys("*");
            for (const auto& k : keys) {
                auto v = store.get(k);
                w.key(k);
                w.value(v.value_or(""));
            }
        }

        // hashes
        w.key("hashes");
        {
            json::ObjectScope obj(w);
            auto hkeys = store.all_hash_names();
            for (const auto& hk : hkeys) {
                w.key(hk);
                json::ObjectScope hobj(w);
                auto all = store.hgetall(hk);
                for (const auto& [field, val] : all) {
                    w.key(field);
                    w.value(val);
                }
            }
        }

        // tables
        w.key("tables");
        {
            json::ArrayScope arr(w);
            auto names = store.show_tables();
            for (const auto& tname : names) {
                const auto* meta = store.get_table(tname);
                if (!meta) continue;
                json::ObjectScope tobj(w);
                w.key("name"); w.value(meta->name);
                w.key("primary_key"); w.value(meta->primary_key);
                w.key("columns");
                {
                    json::ArrayScope carr(w);
                    for (const auto& cd : meta->columns) {
                        json::ObjectScope cobj(w);
                        w.key("name"); w.value(cd.name);
                        w.key("type"); w.value(cd.type);
                        w.key("size"); w.value(cd.size);
                        w.key("is_primary"); w.value(cd.is_primary);
                    }
                }
                // 行数据
                w.key("rows");
                {
                    json::ArrayScope rarr(w);
                    auto rows = store.get_all_rows(tname);
                    for (const auto& row : rows) {
                        json::ObjectScope robj(w);
                        for (const auto& cd : meta->columns) {
                            w.key(cd.name);
                            w.value(StorageEngine::row_value_str(row, cd.name));
                        }
                    }
                }
            }
        }
    }

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;
    ofs << w.str();
    ofs.close();
    std::cout << "[INFO] Snapshot saved to " << path << std::endl;
    return true;
}

bool Snapshot::load(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;

    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    ifs.close();

    json::Reader reader;
    json::Value root;
    try {
        root = reader.parse(content);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to parse snapshot: " << e.what() << std::endl;
        return false;
    }

    auto& store = StorageEngine::instance();

    // strings
    const auto& strings = root["strings"];
    for (const auto& [k, v] : strings.obj_val) {
        store.set(k, v.as_str());
    }

    // hashes
    const auto& hashes = root["hashes"];
    for (const auto& [hk, hv] : hashes.obj_val) {
        for (const auto& [field, val] : hv.obj_val) {
            store.hset(hk, field, val.as_str());
        }
    }

    // tables
    const auto& tables = root["tables"];
    for (size_t i = 0; i < tables.size(); ++i) {
        const auto& t = tables[i];
        TableMeta meta;
        meta.name = t["name"].as_str();
        meta.primary_key = t["primary_key"].as_str();
        const auto& cols = t["columns"];
        for (size_t j = 0; j < cols.size(); ++j) {
            ColumnDef cd;
            cd.name = cols[j]["name"].as_str();
            cd.type = cols[j]["type"].as_str();
            cd.size = static_cast<int>(cols[j]["size"].as_int());
            cd.is_primary = cols[j]["is_primary"].as_int() != 0;
            meta.columns.push_back(cd);
        }
        store.create_table(meta);
        // 行数据
        const auto& rows = t["rows"];
        for (size_t j = 0; j < rows.size(); ++j) {
            DataRow row;
            for (const auto& cd : meta.columns) {
                row[cd.name] = rows[j][cd.name].as_str();
            }
            store.insert_row(meta.name, row, meta.primary_key);
        }
    }

    std::cout << "[INFO] Snapshot loaded from " << path << std::endl;
    return true;
}

} // namespace blueis
