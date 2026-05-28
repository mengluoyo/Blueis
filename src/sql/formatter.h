#ifndef BLUEIS_SQL_FORMATTER_H
#define BLUEIS_SQL_FORMATTER_H

#include "executor.h"
#include <string>

namespace blueis {
namespace sql {

std::string format_result(const Executor::Result& r);

} // namespace sql
} // namespace blueis

#endif // BLUEIS_SQL_FORMATTER_H
