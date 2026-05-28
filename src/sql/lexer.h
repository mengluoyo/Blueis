#ifndef BLUEIS_SQL_LEXER_H
#define BLUEIS_SQL_LEXER_H

#include <string>
#include <variant>
#include <vector>

namespace blueis {
namespace sql {

struct Token {
    enum class Type {
        Keyword, Identifier, Integer, String, Comma, LParen, RParen, Semicolon,
        Eq, Ne, Lt, Gt, Le, Ge, Star, Eof, Unknown
    };
    Type type = Type::Unknown;
    std::string text;
    int64_t int_val = 0;
};

class Lexer {
public:
    explicit Lexer(const std::string& input);

    Token next();
    Token peek();

private:
    void skip_ws();
    Token scan_ident();
    Token scan_number();
    Token scan_string();
    bool match_kw(const std::string& word, const std::string& kw) const;

    const std::string& m_input;
    size_t m_pos = 0;
    bool m_peeked = false;
    Token m_peek_token;
};

} // namespace sql
} // namespace blueis

#endif // BLUEIS_SQL_LEXER_H
