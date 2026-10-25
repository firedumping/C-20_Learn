#include "log_analyzer/analyzer.hpp"
#include "log_analyzer/report.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void test_valid_line() {
    constexpr std::string_view line =
        "2026-10-25T10:15:30Z INFO request completed "
        "status=200 duration_ms=37";
    const auto parsed = log_analyzer::parse_line(line);
    const auto* record = std::get_if<log_analyzer::LogRecord>(&parsed);
    check(record != nullptr, "valid line parses");
    if (record == nullptr) {
        return;
    }
    check(record->status == 200, "status parsed");
    check(record->duration_ms == 37, "duration parsed");
    check(record->level == "INFO", "level parsed");
    check(record->timestamp == "2026-10-25T10:15:30Z", "timestamp parsed");
}

void test_invalid_lines() {
    using log_analyzer::ParseError;
    const std::vector<std::pair<std::string_view, ParseError>> cases{
        {"", ParseError::wrong_field_count},
        {"garbage line", ParseError::wrong_field_count},
        {"2026-10-25T10:15:30Z INFO request completed status=200",
         ParseError::wrong_field_count},
        {"2026-13-25T10:15:30Z INFO request completed status=200 duration_ms=1",
         ParseError::bad_timestamp},
        {"2026-10-25T10:15:30Z TRACE request completed status=200 duration_ms=1",
         ParseError::bad_level},
        {"2026-10-25T10:15:30Z INFO request completed status=99 duration_ms=1",
         ParseError::bad_status},
        {"2026-10-25T10:15:30Z INFO request completed status=200 duration_ms=-1",
         ParseError::bad_duration},
    };

    for (const auto& [line, expected] : cases) {
        const auto parsed = log_analyzer::parse_line(line);
        const auto* error = std::get_if<ParseError>(&parsed);
        check(error != nullptr && *error == expected,
              "invalid line reports expected error");
    }
}

void test_aggregation() {
    const std::vector<std::string> lines{
        "2026-10-25T10:15:30Z INFO request completed status=200 duration_ms=10",
        "2026-10-25T10:16:30Z WARN request completed status=503 duration_ms=30",
        "2026-10-25T11:01:00Z ERROR request completed status=503 duration_ms=20",
        "broken",
    };
    const auto statistics = log_analyzer::analyze_lines(lines);
    check(statistics.total_lines == 4, "total line count");
    check(statistics.valid_lines == 3, "valid line count");
    check(statistics.invalid_lines == 1, "invalid line count");
    check(statistics.status_counts.at(200) == 1, "status 200 count");
    check(statistics.status_counts.at(503) == 2, "status 503 count");
    check(statistics.hourly_counts.at("2026-10-25T10:") == 2,
          "hour 10 count");
    check(statistics.hourly_counts.at("2026-10-25T11:") == 1,
          "hour 11 count");
    check(statistics.total_duration_ms == 60, "total duration");
    check(statistics.average_duration_ms == 20.0, "average duration");

    const std::string report = log_analyzer::format_report(statistics);
    check(report.find("valid:   3") != std::string::npos,
          "report contains valid count");
    check(report.find("Average duration: 20.00 ms") != std::string::npos,
          "report contains average duration");
    check(report.find("2026-10-25T10: 2") != std::string::npos,
          "report formats hourly count without a duplicate colon");
}

void test_empty_input() {
    const auto statistics =
        log_analyzer::analyze_lines(std::vector<std::string>{});
    check(statistics.total_lines == 0, "empty input total");
    check(statistics.average_duration_ms == 0.0,
          "empty average avoids division by zero");
}

void test_cli() {
    using log_analyzer::CliError;
    using log_analyzer::CliOptions;

    const std::vector<std::string_view> normal{"--strict", "access.log"};
    const auto options = log_analyzer::parse_cli(normal);
    check(std::holds_alternative<CliOptions>(options), "CLI parses options");
    if (const auto* parsed = std::get_if<CliOptions>(&options)) {
        check(parsed->strict, "CLI strict flag");
        check(parsed->path == "access.log", "CLI path");
    }

    const std::vector<std::string_view> missing{};
    const auto missing_result = log_analyzer::parse_cli(missing);
    check(std::holds_alternative<CliError>(missing_result) &&
              std::get<CliError>(missing_result) == CliError::missing_path,
          "CLI rejects missing path");

    const std::vector<std::string_view> unknown{"--unknown", "access.log"};
    const auto unknown_result = log_analyzer::parse_cli(unknown);
    check(std::holds_alternative<CliError>(unknown_result) &&
              std::get<CliError>(unknown_result) == CliError::unknown_option,
          "CLI rejects unknown option");
}

} // namespace

int main() {
    test_valid_line();
    test_invalid_lines();
    test_aggregation();
    test_empty_input();
    test_cli();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "all log analyzer tests passed\n";
    return 0;
}
