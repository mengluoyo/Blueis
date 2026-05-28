#ifndef BLUEIS_SQL_EXECUTOR_H
#define BLUEIS_SQL_EXECUTOR_H

#include "ast.h"
#include "../storage/engine.h"
#include <string>
#include <tuple>
#include <vector>

namespace blueis {
namespace sql {

class Executor {
public:
    explicit Executor(StorageEngine& store);

    // 返回 (列名, 行数据, 影响行数)
    struct Result {
        std::vector<std::string> columns;
        std::vector<std::vector<std::string>> rows;
        int affected = 0;
    };

    Result execute(const Stmt& stmt);

private:
    Result exec_create(const CreateTableStmt& s);
    Result exec_insert(const InsertStmt& s);
    Result exec_select(const SelectStmt& s);
    Result exec_update(const UpdateStmt& s);
    Result exec_delete(const DeleteStmt& s);
    Result exec_drop(const DropTableStmt& s);
    Result exec_show(const ShowTablesStmt& s);

    // WHERE 表达式求值
    bool eval_expr(const std::unique_ptr<Expr>& e, const DataRow& row) const;

    StorageEngine& m_store;
};

} // namespace sql
} // namespace blueis

#endif // BLUEIS_SQL_EXECUTOR_H
