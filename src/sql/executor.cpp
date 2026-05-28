#include "executor.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace blueis {
namespace sql {

Executor::Executor(StorageEngine& store) : m_store(store) {}

Executor::Executor::Result Executor::execute(const Stmt& stmt) {
    if (std::holds_alternative<CreateTableStmt>(stmt))
        return exec_create(std::get<CreateTableStmt>(stmt));
    if (std::holds_alternative<InsertStmt>(stmt))
        return exec_insert(std::get<InsertStmt>(stmt));
    if (std::holds_alternative<SelectStmt>(stmt))
        return exec_select(std::get<SelectStmt>(stmt));
    if (std::holds_alternative<UpdateStmt>(stmt))
        return exec_update(std::get<UpdateStmt>(stmt));
    if (std::holds_alternative<DeleteStmt>(stmt))
        return exec_delete(std::get<DeleteStmt>(stmt));
    if (std::holds_alternative<DropTableStmt>(stmt))
        return exec_drop(std::get<DropTableStmt>(stmt));
    return exec_show(std::get<ShowTablesStmt>(stmt));
}

// ============================================================
// CREATE TABLE
// ============================================================

Executor::Result Executor::exec_create(const CreateTableStmt& s) {
    if (m_store.get_table(s.table)) {
        throw std::runtime_error("Table '" + s.table + "' already exists");
    }
    TableMeta meta;
    meta.name = s.table;
    for (const auto& c : s.columns) {
        ColumnDef cd;
        cd.name = c.name;
        cd.type = c.type;
        cd.size = c.size;
        cd.is_primary = c.is_primary;
        if (cd.is_primary) meta.primary_key = cd.name;
        meta.columns.push_back(cd);
    }
    m_store.create_table(meta);
    return {{}, {}, 0};
}

// ============================================================
// INSERT INTO
// ============================================================

Executor::Result Executor::exec_insert(const InsertStmt& s) {
    const TableMeta* meta = m_store.get_table(s.table);
    if (!meta) throw std::runtime_error("Table '" + s.table + "' does not exist");

    std::string pk_col = meta->primary_key;

    for (const auto& row_vals : s.values) {
        DataRow row;
        for (size_t i = 0; i < s.columns.size(); ++i) {
            const std::string& col = s.columns[i];
            const std::string& val = row_vals[i];

            // 根据列类型存储值
            const ColumnDef* col_def = nullptr;
            for (const auto& cd : meta->columns) {
                if (cd.name == col) { col_def = &cd; break; }
            }

            if (col_def && col_def->type == "int") {
                row[col] = static_cast<int64_t>(std::stoll(val));
            } else if (col_def && col_def->type == "float") {
                row[col] = std::stod(val);
            } else {
                row[col] = val;
            }
        }
        m_store.insert_row(s.table, row, pk_col);
    }
    return {{}, {}, static_cast<int>(s.values.size())};
}

// ============================================================
// SELECT
// ============================================================

Executor::Result Executor::exec_select(const SelectStmt& s) {
    const TableMeta* meta = m_store.get_table(s.table);
    if (!meta) throw std::runtime_error("Table '" + s.table + "' does not exist");

    auto all = m_store.get_all_rows(s.table);
    std::vector<DataRow> filtered;

    for (const auto& row : all) {
        if (s.where) {
            if (eval_expr(s.where, row)) filtered.push_back(row);
        } else {
            filtered.push_back(row);
        }
    }

    // 确定输出列
    std::vector<std::string> out_cols;
    if (s.columns.size() == 1 && s.columns[0] == "*") {
        for (const auto& cd : meta->columns) out_cols.push_back(cd.name);
    } else {
        out_cols = s.columns;
    }

    std::vector<std::vector<std::string>> out_rows;
    for (const auto& row : filtered) {
        std::vector<std::string> r;
        for (const auto& col : out_cols) {
            r.push_back(StorageEngine::row_value_str(row, col));
        }
        out_rows.push_back(std::move(r));
    }

    return {out_cols, out_rows, static_cast<int>(out_rows.size())};
}

// ============================================================
// UPDATE
// ============================================================

Executor::Result Executor::exec_update(const UpdateStmt& s) {
    const TableMeta* meta = m_store.get_table(s.table);
    if (!meta) throw std::runtime_error("Table '" + s.table + "' does not exist");

    auto all = m_store.get_all_rows(s.table);
    int count = 0;

    for (auto& row : all) {
        if (s.where && !eval_expr(s.where, row)) continue;

        for (const auto& set : s.sets) {
            const ColumnDef* col_def = nullptr;
            for (const auto& cd : meta->columns) {
                if (cd.name == set.column) { col_def = &cd; break; }
            }
            if (col_def && col_def->type == "int") {
                row[set.column] = static_cast<int64_t>(std::stoll(set.value));
            } else {
                row[set.column] = set.value;
            }
        }
        ++count;
    }

    if (count > 0) {
        // 重新插入所有修改后的行
        for (auto& row : all) {
            m_store.insert_row(s.table, row, meta->primary_key);
        }
    }

    return {{}, {}, count};
}

// ============================================================
// DELETE
// ============================================================

Executor::Result Executor::exec_delete(const DeleteStmt& s) {
    const TableMeta* meta = m_store.get_table(s.table);
    if (!meta) throw std::runtime_error("Table '" + s.table + "' does not exist");

    if (!s.where) {
        int n = m_store.delete_all_rows(s.table);
        return {{}, {}, n};
    }

    auto all = m_store.get_all_rows(s.table);
    int count = 0;

    for (const auto& row : all) {
        if (eval_expr(s.where, row)) {
            std::string pk = StorageEngine::row_value_str(row, meta->primary_key);
            m_store.delete_row(s.table, pk);
            ++count;
        }
    }

    return {{}, {}, count};
}

// ============================================================
// DROP TABLE
// ============================================================

Executor::Result Executor::exec_drop(const DropTableStmt& s) {
    m_store.drop_table(s.table);
    return {{}, {}, 0};
}

// ============================================================
// SHOW TABLES
// ============================================================

Executor::Result Executor::exec_show(const ShowTablesStmt&) {
    auto names = m_store.show_tables();
    std::vector<std::vector<std::string>> rows;
    for (const auto& n : names) {
        rows.push_back({n});
    }
    return {{"Tables_in_blueis"}, rows, static_cast<int>(rows.size())};
}

// ============================================================
// WHERE 表达式求值
// ============================================================

bool Executor::eval_expr(const std::unique_ptr<Expr>& e, const DataRow& row) const {
    if (!e) return true;

    switch (e->kind) {
        case Expr::Kind::Ident: {
            return !StorageEngine::row_value_str(row, e->ident_name).empty();
        }
        case Expr::Kind::Literal: {
            return true; // 字面量永远为 true
        }
        case Expr::Kind::Binary: {
            std::string op = e->op;
            std::transform(op.begin(), op.end(), op.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            // AND / OR
            if (op == "and") {
                return eval_expr(e->left, row) && eval_expr(e->right, row);
            }
            if (op == "or") {
                return eval_expr(e->left, row) || eval_expr(e->right, row);
            }

            // 比较——左边必须是列名 (Identifier), 右边是字面量
            std::string col;
            if (e->left->kind == Expr::Kind::Ident) {
                col = e->left->ident_name;
            } else {
                return false;
            }

            std::string left_val = StorageEngine::row_value_str(row, col);
            std::string right_val;
            if (e->right->kind == Expr::Kind::Literal) {
                if (std::holds_alternative<std::string>(e->right->literal_val)) {
                    right_val = std::get<std::string>(e->right->literal_val);
                } else if (std::holds_alternative<int64_t>(e->right->literal_val)) {
                    right_val = std::to_string(std::get<int64_t>(e->right->literal_val));
                }
            } else if (e->right->kind == Expr::Kind::Ident) {
                right_val = StorageEngine::row_value_str(row, e->right->ident_name);
            }

            // LIKE
            if (op == "like") {
                std::string pat = right_val;
                // % → .* 的简化 glob 匹配
                bool match = false;
                if (pat.size() >= 2 && pat.front() == '%' && pat.back() == '%') {
                    std::string mid = pat.substr(1, pat.size() - 2);
                    match = left_val.find(mid) != std::string::npos;
                } else if (!pat.empty() && pat.front() == '%') {
                    std::string suffix = pat.substr(1);
                    match = left_val.size() >= suffix.size() &&
                            left_val.compare(left_val.size() - suffix.size(), suffix.size(), suffix) == 0;
                } else if (!pat.empty() && pat.back() == '%') {
                    std::string prefix = pat.substr(0, pat.size() - 1);
                    match = left_val.compare(0, prefix.size(), prefix) == 0;
                } else {
                    match = (left_val == pat);
                }
                return match;
            }

            // 数值比较
            int64_t lv = 0, rv = 0;
            bool num_cmp = true;
            try {
                lv = std::stoll(left_val);
                rv = std::stoll(right_val);
            } catch (...) {
                num_cmp = false;
            }

            if (num_cmp) {
                if (op == "=")  return lv == rv;
                if (op == "!=") return lv != rv;
                if (op == ">")  return lv > rv;
                if (op == "<")  return lv < rv;
                if (op == ">=") return lv >= rv;
                if (op == "<=") return lv <= rv;
            }

            // 字符串比较
            if (op == "=")  return left_val == right_val;
            if (op == "!=") return left_val != right_val;
            if (op == ">")  return left_val > right_val;
            if (op == "<")  return left_val < right_val;
            if (op == ">=") return left_val >= right_val;
            if (op == "<=") return left_val <= right_val;

            return false;
        }
    }
    return false;
}

} // namespace sql
} // namespace blueis
