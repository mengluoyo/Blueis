#ifndef BLUEIS_PERSISTENCE_JSON_H
#define BLUEIS_PERSISTENCE_JSON_H

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

namespace blueis {
namespace json {

// === 简易 JSON 写入器 ===
class Writer {
public:
    std::string str() const { return m_out.str(); }

    Writer& begin_object() { maybe_comma(); m_out << "{"; push_first(); return *this; }
    Writer& end_object()   { m_out << "}"; pop_first(); return *this; }
    Writer& begin_array()  { maybe_comma(); m_out << "["; push_first(); return *this; }
    Writer& end_array()    { m_out << "]"; pop_first(); return *this; }

    Writer& key(const std::string& k) { comma(); write_string(k); m_out << ":"; m_after_key = true; return *this; }
    Writer& value(const std::string& v) { maybe_comma(); write_string(v); return *this; }
    Writer& value(int64_t v)            { maybe_comma(); m_out << v; return *this; }
    Writer& value(int v)                { maybe_comma(); m_out << v; return *this; }
    Writer& value(bool v)               { maybe_comma(); m_out << (v ? "true" : "false"); return *this; }

    void reset() { m_out.str(""); m_out.clear(); m_first.clear(); m_after_key = false; }

private:
    void push_first() { m_first.push_back(true); m_after_key = false; }
    void pop_first()  { m_first.pop_back(); }

    void maybe_comma() {
        if (m_after_key) { m_after_key = false; return; }
        comma();
    }

    void comma() {
        if (m_first.empty()) return;
        if (m_first.back()) {
            m_first.back() = false;
        } else {
            m_out << ",";
        }
    }
    void write_string(const std::string& s) {
        m_out << '"';
        for (char c : s) {
            switch (c) {
                case '"':  m_out << "\\\""; break;
                case '\\': m_out << "\\\\"; break;
                case '\n': m_out << "\\n"; break;
                case '\r': m_out << "\\r"; break;
                case '\t': m_out << "\\t"; break;
                default:   m_out << c;
            }
        }
        m_out << '"';
    }

    std::ostringstream m_out;
    std::vector<bool> m_first;
    bool m_after_key = false;
};

// RAII 作用域
struct ObjectScope {
    Writer& w;
    ObjectScope(Writer& wr) : w(wr) { w.begin_object(); }
    ~ObjectScope() { w.end_object(); }
};
struct ArrayScope {
    Writer& w;
    ArrayScope(Writer& wr) : w(wr) { w.begin_array(); }
    ~ArrayScope() { w.end_array(); }
};

// === 简易 JSON 读取器 ===
class Value {
public:
    enum class Type { Null, String, Integer, Object, Array };
    Type type = Type::Null;
    std::string str_val;
    int64_t int_val = 0;
    std::vector<std::pair<std::string, Value>> obj_val;
    std::vector<Value> arr_val;

    const Value& operator[](const std::string& k) const {
        static Value null_val;
        for (const auto& [key, val] : obj_val) {
            if (key == k) return val;
        }
        return null_val;
    }
    std::string as_str() const { return str_val; }
    int64_t as_int() const { return int_val; }
    size_t size() const { return arr_val.size(); }
    const Value& operator[](size_t i) const {
        static Value null_val;
        return i < arr_val.size() ? arr_val[i] : null_val;
    }
};

class Reader {
public:
    Value parse(const std::string& json);

private:
    void skip_ws();
    char peek();
    char advance();
    Value parse_value();
    Value parse_object();
    Value parse_array();
    Value parse_string();
    Value parse_number();
    void expect(char c);

    const std::string* m_input = nullptr;
    size_t m_pos = 0;
};

} // namespace json
} // namespace blueis

#endif // BLUEIS_PERSISTENCE_JSON_H
