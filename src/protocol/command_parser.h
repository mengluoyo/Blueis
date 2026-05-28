#ifndef BLUEIS_PROTOCOL_COMMAND_PARSER_H
#define BLUEIS_PROTOCOL_COMMAND_PARSER_H

#include "common.h"
#include <string>
#include <vector>

namespace blueis {

class ProtocolParser {
public:
    Command parse(const std::string& raw) const;

private:
    CommandType detect_type(const std::string& first_token) const;
    std::vector<std::string> tokenize(const std::string& raw) const;
};

} // namespace blueis

#endif // BLUEIS_PROTOCOL_COMMAND_PARSER_H
