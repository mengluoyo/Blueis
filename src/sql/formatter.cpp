#include "formatter.h"
#include <algorithm>
#include <sstream>
#include <vector>

namespace blueis {
namespace sql {

std::string format_result(const Executor::Result& r) {
    std::ostringstream out;

    // SHOW TABLES / SELECT 有列名输出
    if (!r.columns.empty()) {
        // 计算每列宽度
        std::vector<size_t> widths(r.columns.size(), 0);
        for (size_t i = 0; i < r.columns.size(); ++i) {
            widths[i] = r.columns[i].size();
        }
        for (const auto& row : r.rows) {
            for (size_t i = 0; i < row.size() && i < widths.size(); ++i) {
                widths[i] = std::max(widths[i], row[i].size());
            }
        }
        // 最小宽度 1
        for (auto& w : widths) if (w < 1) w = 1;

        // 分隔线
        auto hr = [&]() {
            out << "+";
            for (size_t w : widths) out << std::string(w + 2, '-') << "+";
            out << "\r\n";
        };

        hr();
        out << "|";
        for (size_t i = 0; i < r.columns.size(); ++i) {
            out << " " << r.columns[i] << std::string(widths[i] - r.columns[i].size() + 1, ' ') << "|";
        }
        out << "\r\n";
        hr();

        for (const auto& row : r.rows) {
            out << "|";
            for (size_t i = 0; i < widths.size(); ++i) {
                std::string cell = (i < row.size()) ? row[i] : "";
                out << " " << cell << std::string(widths[i] - cell.size() + 1, ' ') << "|";
            }
            out << "\r\n";
        }
        hr();

        out << r.affected << " row" << (r.affected != 1 ? "s" : "") << " in set\r\n";
    } else {
        // 非查询语句
        if (r.affected > 0) {
            out << "OK (" << r.affected << " row" << (r.affected != 1 ? "s" : "") << " affected)";
        } else {
            out << "OK";
        }
    }

    return out.str();
}

} // namespace sql
} // namespace blueis
