#ifndef BLUEIS_SQL_PARSER_H
#define BLUEIS_SQL_PARSER_H

#include "ast.h"
#include "lexer.h"
#include <memory>
#include <string>

namespace blueis {
namespace sql {

class Parser {
public:
    explicit Parser(Lexer& lexer);

    Stmt parse();

private:
    Token advance();
    Token expect(Token::Type t);
    bool match(Token::Type t);
    bool match_kw(const std::string& kw);

    Stmt parse_create();
    Stmt parse_insert();
    Stmt parse_select();
    Stmt parse_update();
    Stmt parse_delete();
    Stmt parse_drop();
    Stmt parse_show();

    std::unique_ptr<Expr> parse_expr();
    std::unique_ptr<Expr> parse_or();
    std::unique_ptr<Expr> parse_and();
    std::unique_ptr<Expr> parse_compare();

    Lexer& m_lexer;
};

} // namespace sql
} // namespace blueis

#endif // BLUEIS_SQL_PARSER_H
