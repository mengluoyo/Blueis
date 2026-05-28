#include "json.h"
#include <cstdlib>
#include <stdexcept>

namespace blueis {
namespace json {

// ============================================================
// Reader
// ============================================================

Value Reader::parse(const std::string& json) {
    m_input = &json;
    m_pos = 0;
    skip_ws();
    auto v = parse_value();
    skip_ws();
    return v;
}

void Reader::skip_ws() {
    while (m_pos < m_input->size() && std::isspace((*m_input)[m_pos])) ++m_pos;
}

char Reader::peek() {
    return m_pos < m_input->size() ? (*m_input)[m_pos] : '\0';
}

char Reader::advance() {
    return m_pos < m_input->size() ? (*m_input)[m_pos++] : '\0';
}

void Reader::expect(char c) {
    if (peek() != c) throw std::runtime_error(std::string("Expected '") + c + "'");
    advance();
}

Value Reader::parse_value() {
    char c = peek();
    if (c == '{') return parse_object();
    if (c == '[') return parse_array();
    if (c == '"') return parse_string();
    if (c == '-' || (c >= '0' && c <= '9')) return parse_number();
    throw std::runtime_error(std::string("Unexpected char in JSON: '") + c + "'");
}

Value Reader::parse_object() {
    Value v;
    v.type = Value::Type::Object;
    expect('{');
    skip_ws();
    if (peek() == '}') { advance(); return v; }
    while (true) {
        skip_ws();
        Value key = parse_string();
        skip_ws();
        expect(':');
        skip_ws();
        v.obj_val.emplace_back(key.str_val, parse_value());
        skip_ws();
        if (peek() == '}') { advance(); break; }
        expect(',');
    }
    return v;
}

Value Reader::parse_array() {
    Value v;
    v.type = Value::Type::Array;
    expect('[');
    skip_ws();
    if (peek() == ']') { advance(); return v; }
    while (true) {
        skip_ws();
        v.arr_val.push_back(parse_value());
        skip_ws();
        if (peek() == ']') { advance(); break; }
        expect(',');
    }
    return v;
}

Value Reader::parse_string() {
    Value v;
    v.type = Value::Type::String;
    expect('"');
    while (true) {
        char c = advance();
        if (c == '"') break;
        if (c == '\\') {
            char next = advance();
            switch (next) {
                case '"':  v.str_val += '"';  break;
                case '\\': v.str_val += '\\'; break;
                case 'n':  v.str_val += '\n'; break;
                case 'r':  v.str_val += '\r'; break;
                case 't':  v.str_val += '\t'; break;
                default:   v.str_val += next;
            }
        } else {
            v.str_val += c;
        }
    }
    return v;
}

Value Reader::parse_number() {
    Value v;
    v.type = Value::Type::Integer;
    std::string num;
    if (peek() == '-') { num += advance(); }
    while (peek() >= '0' && peek() <= '9') num += advance();
    v.int_val = std::strtoll(num.c_str(), nullptr, 10);
    return v;
}

} // namespace json
} // namespace blueis
