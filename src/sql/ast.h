#ifndef BLUEIS_SQL_AST_H
#define BLUEIS_SQL_AST_H

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace blueis {
namespace sql {

// 列定义 (SQL 侧)
struct ColDef {
    std::string name;
    std::string type;  // INT, VARCHAR, BOOL, FLOAT
    int size = 0;
    bool is_primary = false;
};

// ==========================================================
// WHERE 表达式
// ==========================================================

struct Expr {
    enum class Kind { Ident, Literal, Binary };
    Kind kind;
    std::string ident_name;         // for Ident
    std::variant<int64_t, std::string, bool, double> literal_val; // for Literal
    std::string op;                 // for Binary: = != > < >= <= like and or
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;

    // factory
    static Expr ident(const std::string& name);
    static Expr literal(const std::string& v);
    static Expr literal(int64_t v);
    static Expr binary(const std::string& op, Expr l, Expr r);
};

// ==========================================================
// 语句类型
// ==========================================================

struct CreateTableStmt {
    std::string table;
    std::vector<ColDef> columns;
};

struct InsertStmt {
    std::string table;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> values; // 支持多行 VALUES
};

struct SelectStmt {
    std::vector<std::string> columns; // "*" 用空或单个 "*"
    std::string table;
    std::unique_ptr<Expr> where;
};

struct SetClause {
    std::string column;
    std::string value; // 字面量字符串
};

struct UpdateStmt {
    std::string table;
    std::vector<SetClause> sets;
    std::unique_ptr<Expr> where;
};

struct DeleteStmt {
    std::string table;
    std::unique_ptr<Expr> where;
};

struct DropTableStmt {
    std::string table;
};

struct ShowTablesStmt {
};

using Stmt = std::variant<
    CreateTableStmt,
    InsertStmt,
    SelectStmt,
    UpdateStmt,
    DeleteStmt,
    DropTableStmt,
    ShowTablesStmt
>;

} // namespace sql
} // namespace blueis

#endif // BLUEIS_SQL_AST_H
