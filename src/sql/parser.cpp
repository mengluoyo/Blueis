#include "parser.h"
#include <algorithm>
#include <stdexcept>

namespace blueis {
namespace sql {

Parser::Parser(Lexer& lexer) : m_lexer(lexer) {}

Token Parser::advance() {
    return m_lexer.next();
}

Token Parser::expect(Token::Type t) {
    Token tok = advance();
    if (tok.type != t) {
        throw std::runtime_error("Syntax error: expected token type " +
                                 std::to_string(static_cast<int>(t)) +
                                 " but got '" + tok.text + "'");
    }
    return tok;
}

bool Parser::match(Token::Type t) {
    if (m_lexer.peek().type == t) {
        advance();
        return true;
    }
    return false;
}

bool Parser::match_kw(const std::string& kw) {
    Token p = m_lexer.peek();
    if (p.type == Token::Type::Keyword) {
        std::string lower = p.text;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower == kw) {
            advance();
            return true;
        }
    }
    return false;
}

// ==========================================================
// 顶层分发
// ==========================================================

Stmt Parser::parse() {
    Token first = m_lexer.peek();
    if (first.type == Token::Type::Keyword) {
        std::string kw = first.text;
        std::transform(kw.begin(), kw.end(), kw.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (kw == "create") return parse_create();
        if (kw == "insert") return parse_insert();
        if (kw == "select") return parse_select();
        if (kw == "update") return parse_update();
        if (kw == "delete") return parse_delete();
        if (kw == "drop")   return parse_drop();
        if (kw == "show")   return parse_show();
        if (kw == "use")    return parse_use();
    }
    throw std::runtime_error("Syntax error: unexpected token '" + first.text + "'");
}

// ==========================================================
// CREATE TABLE name (col_def, ...) / CREATE DATABASE name
// ==========================================================

Stmt Parser::parse_create() {
    expect(Token::Type::Keyword); // CREATE
    Token second = m_lexer.peek();
    if (second.type == Token::Type::Keyword) {
        std::string kw2 = second.text;
        std::transform(kw2.begin(), kw2.end(), kw2.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (kw2 == "database") {
            advance(); // consume DATABASE
            Token name = expect(Token::Type::Identifier);
            match(Token::Type::Semicolon);
            return CreateDatabaseStmt{name.text};
        }
        if (kw2 == "table") {
            advance(); // consume TABLE
        } else {
            throw std::runtime_error("CREATE expects TABLE or DATABASE");
        }
    } else {
        throw std::runtime_error("CREATE expects TABLE or DATABASE");
    }

    Token name = expect(Token::Type::Identifier);
    expect(Token::Type::LParen);

    CreateTableStmt stmt;
    stmt.table = name.text;

    while (!match(Token::Type::RParen)) {
        Token cname = expect(Token::Type::Identifier);
        Token ctype = expect(Token::Type::Keyword); // int/varchar/bool/float

        ColDef col;
        col.name = cname.text;
        col.type = ctype.text;
        std::transform(col.type.begin(), col.type.end(), col.type.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (col.type == "varchar" || col.type == "char") {
            if (m_lexer.peek().type == Token::Type::LParen) {
                expect(Token::Type::LParen);
                Token sz = expect(Token::Type::Integer);
                col.size = static_cast<int>(sz.int_val);
                expect(Token::Type::RParen);
            }
        }

        // PRIMARY KEY
        if (match_kw("primary")) {
            expect(Token::Type::Keyword); // KEY
            col.is_primary = true;
        }

        stmt.columns.push_back(col);
        match(Token::Type::Comma);
    }

    match(Token::Type::Semicolon);
    return stmt;
}

// ==========================================================
// INSERT INTO name (cols) VALUES (vals), (vals), ...
// ==========================================================

Stmt Parser::parse_insert() {
    expect(Token::Type::Keyword); // INSERT
    expect(Token::Type::Keyword); // INTO
    Token table = expect(Token::Type::Identifier);

    InsertStmt stmt;
    stmt.table = table.text;

    if (match_kw("values")) {
        // 无列名: INSERT INTO t VALUES (...)
        // 暂不处理，直接报错
        throw std::runtime_error("INSERT requires column list: INSERT INTO t (cols) VALUES (...)");
    }

    expect(Token::Type::LParen);
    while (!match(Token::Type::RParen)) {
        Token col = expect(Token::Type::Identifier);
        stmt.columns.push_back(col.text);
        match(Token::Type::Comma);
    }

    expect(Token::Type::Keyword); // VALUES

    // 多行值
    do {
        expect(Token::Type::LParen);
        std::vector<std::string> row;
        while (!match(Token::Type::RParen)) {
            Token val = advance();
            if (val.type == Token::Type::Integer || val.type == Token::Type::String) {
                row.push_back(val.text);
            } else {
                throw std::runtime_error("INSERT value must be integer or string, got '" + val.text + "'");
            }
            match(Token::Type::Comma);
        }
        if (row.size() != stmt.columns.size()) {
            throw std::runtime_error("INSERT value count mismatch: " +
                                     std::to_string(row.size()) + " vs " +
                                     std::to_string(stmt.columns.size()));
        }
        stmt.values.push_back(std::move(row));
    } while (match(Token::Type::Comma));

    match(Token::Type::Semicolon);
    return stmt;
}

// ==========================================================
// SELECT col,... FROM name [WHERE expr]
// ==========================================================

Stmt Parser::parse_select() {
    expect(Token::Type::Keyword); // SELECT

    SelectStmt stmt;

    if (match(Token::Type::Star)) {
        stmt.columns.push_back("*");
    } else {
        while (true) {
            Token col = expect(Token::Type::Identifier);
            stmt.columns.push_back(col.text);
            if (!match(Token::Type::Comma)) break;
        }
    }

    expect(Token::Type::Keyword); // FROM
    Token table = expect(Token::Type::Identifier);
    stmt.table = table.text;

    if (match_kw("where")) {
        stmt.where = parse_expr();
    }

    match(Token::Type::Semicolon);
    return stmt;
}

// ==========================================================
// UPDATE name SET col=val,... [WHERE expr]
// ==========================================================

Stmt Parser::parse_update() {
    expect(Token::Type::Keyword); // UPDATE
    Token table = expect(Token::Type::Identifier);
    expect(Token::Type::Keyword); // SET

    UpdateStmt stmt;
    stmt.table = table.text;

    while (true) {
        Token col = expect(Token::Type::Identifier);
        expect(Token::Type::Eq);
        Token val = advance();
        if (val.type != Token::Type::Integer && val.type != Token::Type::String) {
            throw std::runtime_error("UPDATE value must be integer or string");
        }
        stmt.sets.push_back({col.text, val.text});
        if (!match(Token::Type::Comma)) break;
    }

    if (match_kw("where")) {
        stmt.where = parse_expr();
    }

    match(Token::Type::Semicolon);
    return stmt;
}

// ==========================================================
// DELETE FROM name [WHERE expr]
// ==========================================================

Stmt Parser::parse_delete() {
    expect(Token::Type::Keyword); // DELETE
    expect(Token::Type::Keyword); // FROM
    Token table = expect(Token::Type::Identifier);

    DeleteStmt stmt;
    stmt.table = table.text;

    if (match_kw("where")) {
        stmt.where = parse_expr();
    }

    match(Token::Type::Semicolon);
    return stmt;
}

// ==========================================================
// DROP TABLE name / DROP DATABASE name
// ==========================================================

Stmt Parser::parse_drop() {
    expect(Token::Type::Keyword); // DROP
    Token second = m_lexer.peek();
    if (second.type == Token::Type::Keyword) {
        std::string kw2 = second.text;
        std::transform(kw2.begin(), kw2.end(), kw2.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (kw2 == "database") {
            advance(); // consume DATABASE
            Token name = expect(Token::Type::Identifier);
            match(Token::Type::Semicolon);
            return DropDatabaseStmt{name.text};
        }
        if (kw2 == "table") {
            advance(); // consume TABLE
        } else {
            throw std::runtime_error("DROP expects TABLE or DATABASE");
        }
    } else {
        throw std::runtime_error("DROP expects TABLE or DATABASE");
    }

    Token table = expect(Token::Type::Identifier);

    DropTableStmt stmt;
    stmt.table = table.text;

    match(Token::Type::Semicolon);
    return stmt;
}

// ==========================================================
// SHOW TABLES / SHOW DATABASES
// ==========================================================

Stmt Parser::parse_show() {
    expect(Token::Type::Keyword); // SHOW
    Token second = m_lexer.peek();
    if (second.type == Token::Type::Keyword) {
        std::string kw2 = second.text;
        std::transform(kw2.begin(), kw2.end(), kw2.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (kw2 == "databases") {
            advance(); // consume DATABASES
            match(Token::Type::Semicolon);
            return ShowDatabasesStmt{};
        }
        if (kw2 == "tables") {
            advance(); // consume TABLES
            match(Token::Type::Semicolon);
            return ShowTablesStmt{};
        }
    }
    throw std::runtime_error("SHOW expects TABLES or DATABASES");
}

// ==========================================================
// USE database_name
// ==========================================================

Stmt Parser::parse_use() {
    expect(Token::Type::Keyword); // USE
    Token name = expect(Token::Type::Identifier);
    match(Token::Type::Semicolon);
    return UseStmt{name.text};
}

// ==========================================================
// WHERE 表达式递归下降
// 优先级: OR < AND < 比较符
// ==========================================================

std::unique_ptr<Expr> Parser::parse_expr() {
    return parse_or();
}

std::unique_ptr<Expr> Parser::parse_or() {
    auto left = parse_and();
    while (match_kw("or")) {
        auto right = parse_and();
        left = std::make_unique<Expr>(
            Expr::binary("or", std::move(*left), std::move(*right)));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parse_and() {
    auto left = parse_compare();
    while (match_kw("and")) {
        auto right = parse_compare();
        left = std::make_unique<Expr>(
            Expr::binary("and", std::move(*left), std::move(*right)));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parse_compare() {
    Token p = m_lexer.peek();

    // 处理 NOT - 暂不支持，跳过
    if (p.type == Token::Type::Keyword) {
        std::string kw = p.text;
        std::transform(kw.begin(), kw.end(), kw.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (kw == "not") {
            advance();
            // 简单处理: NOT col = val → col != val (简化版本)
        }
    }

    // 左边必须是标识符或字面量
    Token left_tok = advance();
    Expr left;
    if (left_tok.type == Token::Type::Identifier) {
        left = Expr::ident(left_tok.text);
    } else if (left_tok.type == Token::Type::Integer) {
        left = Expr::literal(left_tok.int_val);
    } else if (left_tok.type == Token::Type::String) {
        left = Expr::literal(left_tok.text);
    } else {
        throw std::runtime_error("Expected identifier or literal in WHERE, got '" + left_tok.text + "'");
    }

    // 比较符或结束
    Token op = m_lexer.peek();
    if (op.type == Token::Type::Eq || op.type == Token::Type::Ne ||
        op.type == Token::Type::Gt || op.type == Token::Type::Lt ||
        op.type == Token::Type::Ge || op.type == Token::Type::Le) {
        advance();
        Token right_tok = advance();
        Expr right;
        if (right_tok.type == Token::Type::Identifier) {
            right = Expr::ident(right_tok.text);
        } else if (right_tok.type == Token::Type::Integer) {
            right = Expr::literal(right_tok.int_val);
        } else if (right_tok.type == Token::Type::String) {
            right = Expr::literal(right_tok.text);
        } else {
            throw std::runtime_error("Expected value after operator in WHERE");
        }
        return std::make_unique<Expr>(Expr::binary(op.text, std::move(left), std::move(right)));
    }

    // LIKE
    if (op.type == Token::Type::Keyword) {
        std::string kw = op.text;
        std::transform(kw.begin(), kw.end(), kw.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (kw == "like") {
            advance();
            Token right_tok = advance();
            if (right_tok.type == Token::Type::String || right_tok.type == Token::Type::Identifier) {
                Expr right = Expr::literal(right_tok.text);
                return std::make_unique<Expr>(Expr::binary("like", std::move(left), std::move(right)));
            }
            throw std::runtime_error("Expected pattern after LIKE");
        }
    }

    // 独立字面量（如 WHERE 1 之类的，直接返回左值）
    return std::make_unique<Expr>(std::move(left));
}

} // namespace sql
} // namespace blueis
