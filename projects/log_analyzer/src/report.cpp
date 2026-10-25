#include "log_analyzer/report.hpp"

#include <format>
#include <string>

namespace log_analyzer {

std::string format_report(const Statistics& statistics) {
    std::string output = std::format(
        "Summary\n"
        "  lines:   {}\n"
        "  valid:   {}\n"
        "  invalid: {}\n",
        statistics.total_lines,
        statistics.valid_lines,
        statistics.invalid_lines);

    output += "\nStatus codes\n";
    for (const auto& [status, count] : statistics.status_counts) {
        output += std::format("  {}: {}\n", status, count);
    }

    output += std::format(
        "\nAverage duration: {:.2f} ms\n",
        statistics.average_duration_ms);

    output += "\nRequests by hour\n";
    for (const auto& [hour, count] : statistics.hourly_counts) {
        output += std::format("  {}: {}\n", hour, count);
    }
    return output;
}

} // namespace log_analyzer
