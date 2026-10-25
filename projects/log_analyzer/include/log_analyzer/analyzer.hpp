#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace log_analyzer {

struct LogRecord {
    std::string timestamp;
    std::string level;
    int status{};
    int duration_ms{};

    bool operator==(const LogRecord&) const = default;
};

enum class ParseError {
    wrong_field_count,
    bad_timestamp,
    bad_level,
    bad_status,
    bad_duration
};

using ParseResult = std::variant<LogRecord, ParseError>;

struct Statistics {
    std::size_t total_lines{};
    std::size_t valid_lines{};
    std::size_t invalid_lines{};
    std::map<int, std::size_t> status_counts;
    std::map<std::string, std::size_t> hourly_counts;
    std::int64_t total_duration_ms{};
    double average_duration_ms{};

    bool operator==(const Statistics&) const = default;
};

ParseResult parse_line(std::string_view line);
Statistics analyze_lines(std::span<const std::string> lines);
void add_record(Statistics& statistics, const LogRecord& record);

enum class CliError {
    missing_path,
    too_many_paths,
    unknown_option
};

struct CliOptions {
    std::string path;
    bool strict{};
    bool help{};

    bool operator==(const CliOptions&) const = default;
};

using CliResult = std::variant<CliOptions, CliError>;
CliResult parse_cli(std::span<const std::string_view> arguments);

} // namespace log_analyzer
