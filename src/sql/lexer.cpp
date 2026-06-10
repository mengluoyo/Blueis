#include "lexer.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unordered_set>

namespace blueis {
namespace sql {

// SQL 关键字集合
static const std::unordered_set<std::string> keywords = {
    "create", "table", "insert", "into", "values", "select",
    "from", "where", "update", "set", "delete", "drop", "show",
    "tables", "primary", "key", "int", "varchar", "bool", "float",
    "char", "text", "double",
    "like", "and", "or", "not", "order", "by", "databases"
};

Lexer::Lexer(const std::string& input) : m_input(input) {}

bool Lexer::match_kw(const std::string& word, const std::string& kw) const {
    if (word.size() != kw.size()) return false;
    for (size_t i = 0; i < word.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(word[i])) !=
            std::tolower(static_cast<unsigned char>(kw[i])))
            return false;
    }
    return true;
}

void Lexer::skip_ws() {
    while (m_pos < m_input.size() &&
           std::isspace(static_cast<unsigned char>(m_input[m_pos]))) {
        ++m_pos;
    }
}

Token Lexer::peek() {
    if (!m_peeked) {
        m_peek_token = next();
        m_peeked = true;
    }
    return m_peek_token;
}

Token Lexer::next() {
    if (m_peeked) {
        m_peeked = false;
        return m_peek_token;
    }

    skip_ws();
    if (m_pos >= m_input.size()) {
        return {Token::Type::Eof, ""};
    }

    char c = m_input[m_pos];

    // 单引号或双引号字符串
    if (c == '\'' || c == '"') {
        return scan_string();
    }

    // 数字
    if (std::isdigit(static_cast<unsigned char>(c)) || c == '-') {
        if (c == '-' && m_pos + 1 < m_input.size() &&
            std::isdigit(static_cast<unsigned char>(m_input[m_pos + 1]))) {
            return scan_number();
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            return scan_number();
        }
    }

    // 标识符/关键字 (以字母或下划线开头)
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        return scan_ident();
    }

    // 多字符运算符
    if (c == '!' && m_pos + 1 < m_input.size() && m_input[m_pos + 1] == '=') {
        m_pos += 2;
        return {Token::Type::Ne, "!="};
    }
    if (c == '>' && m_pos + 1 < m_input.size() && m_input[m_pos + 1] == '=') {
        m_pos += 2;
        return {Token::Type::Ge, ">="};
    }
    if (c == '<' && m_pos + 1 < m_input.size() && m_input[m_pos + 1] == '=') {
        m_pos += 2;
        return {Token::Type::Le, "<="};
    }

    // 单字符
    ++m_pos;
    switch (c) {
        case ',': return {Token::Type::Comma, ","};
        case '(': return {Token::Type::LParen, "("};
        case ')': return {Token::Type::RParen, ")"};
        case ';': return {Token::Type::Semicolon, ";"};
        case '=': return {Token::Type::Eq, "="};
        case '>': return {Token::Type::Gt, ">"};
        case '<': return {Token::Type::Lt, "<"};
        case '*': return {Token::Type::Star, "*"};
        default:  return {Token::Type::Unknown, std::string(1, c)};
    }
}

Token Lexer::scan_ident() {
    std::string word;
    while (m_pos < m_input.size() &&
           (std::isalnum(static_cast<unsigned char>(m_input[m_pos])) ||
            m_input[m_pos] == '_')) {
        word += m_input[m_pos];
        ++m_pos;
    }
    // 判断是否关键字
    std::string lower = word;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (keywords.find(lower) != keywords.end()) {
        return {Token::Type::Keyword, word};
    }
    return {Token::Type::Identifier, word};
}

Token Lexer::scan_number() {
    std::string num;
    if (m_input[m_pos] == '-') {
        num += m_input[m_pos];
        ++m_pos;
    }
    while (m_pos < m_input.size() &&
           std::isdigit(static_cast<unsigned char>(m_input[m_pos]))) {
        num += m_input[m_pos];
        ++m_pos;
    }
    Token t;
    t.type = Token::Type::Integer;
    t.text = num;
    t.int_val = std::strtoll(num.c_str(), nullptr, 10);
    return t;
}

Token Lexer::scan_string() {
    char quote = m_input[m_pos];
    ++m_pos; // 跳过开引号
    std::string val;
    while (m_pos < m_input.size() && m_input[m_pos] != quote) {
        val += m_input[m_pos];
        ++m_pos;
    }
    if (m_pos < m_input.size()) ++m_pos; // 跳闭引号
    return {Token::Type::String, val};
}

} // namespace sql
} // namespace blueis
