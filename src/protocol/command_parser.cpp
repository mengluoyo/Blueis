#include "command_parser.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace blueis {

Command ProtocolParser::parse(const std::string& raw) const {
    Command cmd;
    cmd.raw = raw;
    cmd.tokens = tokenize(raw);

    if (cmd.tokens.empty()) {
        cmd.type = CommandType::Unknown;
        return cmd;
    }

    auto& first = cmd.tokens[0];
    std::transform(first.begin(), first.end(), first.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    cmd.type = detect_type(cmd.tokens[0]);
    return cmd;
}

CommandType ProtocolParser::detect_type(const std::string& first_token) const {
    static const std::unordered_set<std::string> sql_keywords = {
        "create", "insert", "select", "update", "delete", "drop", "show"
    };

    if (sql_keywords.find(first_token) != sql_keywords.end()) {
        return CommandType::SQL;
    }
    return CommandType::Redis;
}

std::vector<std::string> ProtocolParser::tokenize(const std::string& raw) const {
    std::vector<std::string> tokens;

    size_t i = 0;
    while (i < raw.size() && std::isspace(static_cast<unsigned char>(raw[i]))) {
        ++i;
    }

    while (i < raw.size()) {
        if (raw[i] == '\'' || raw[i] == '"') {
            char quote = raw[i];
            ++i;
            std::string token;
            while (i < raw.size() && raw[i] != quote) {
                token += raw[i];
                ++i;
            }
            tokens.push_back(token);
            if (i < raw.size()) ++i;
        }
        else if (raw[i] == ';' && i == raw.size() - 1) {
            break;
        }
        else {
            std::string token;
            while (i < raw.size() && !std::isspace(static_cast<unsigned char>(raw[i])) && raw[i] != ';') {
                token += raw[i];
                ++i;
            }
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        while (i < raw.size() && std::isspace(static_cast<unsigned char>(raw[i]))) {
            ++i;
        }
    }

    return tokens;
}

} // namespace blueis
