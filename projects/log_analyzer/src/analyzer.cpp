#include "log_analyzer/analyzer.hpp"

#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace log_analyzer {
namespace {

bool is_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

std::optional<int> parse_fixed_hundred(std::string_view text) {
    // Parse exactly two decimal digits (00..99).
    if (text.size() != 2 || !is_digit(text[0]) || !is_digit(text[1])) {
        return std::nullopt;
    }
    return (text[0] - '0') * 10 + (text[1] - '0');
}

bool is_valid_timestamp(std::string_view timestamp) {
    if (timestamp.size() != 20) {
        return false;
    }

    if (timestamp[4] != '-' || timestamp[7] != '-' ||
        timestamp[10] != 'T' || timestamp[13] != ':' ||
        timestamp[16] != ':' || timestamp[19] != 'Z') {
        return false;
    }

    for (std::size_t index = 0; index < 19; ++index) {
        if (index == 4 || index == 7 || index == 10 ||
            index == 13 || index == 16) {
            continue;
        }
        if (!is_digit(timestamp[index])) {
            return false;
        }
    }

    auto parse_four = [](std::string_view text) {
        int value = 0;
        for (const char ch : text) {
            value = value * 10 + (ch - '0');
        }
        return value;
    };

    const int year = parse_four(timestamp.substr(0, 4));
    const auto month = parse_fixed_hundred(timestamp.substr(5, 2));
    const auto day = parse_fixed_hundred(timestamp.substr(8, 2));
    const auto hour = parse_fixed_hundred(timestamp.substr(11, 2));
    const auto minute = parse_fixed_hundred(timestamp.substr(14, 2));
    const auto second = parse_fixed_hundred(timestamp.substr(17, 2));

    return year >= 1970 &&
           month && *month >= 1 && *month <= 12 &&
           day && *day >= 1 && *day <= 31 &&
           hour && *hour <= 23 &&
           minute && *minute <= 59 &&
           second && *second <= 59;
}

std::optional<int> parse_nonnegative_int(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    int value = 0;
    for (const char ch : text) {
        if (!is_digit(ch)) {
            return std::nullopt;
        }
        const int digit = ch - '0';
        if (value > (std::numeric_limits<int>::max() - digit) / 10) {
            return std::nullopt;
        }
        value = value * 10 + digit;
    }
    return value;
}

std::vector<std::string_view> split_whitespace(std::string_view line) {
    std::vector<std::string_view> tokens;
    std::size_t index = 0;
    while (index < line.size()) {
        while (index < line.size() &&
               (line[index] == ' ' || line[index] == '\t')) {
            ++index;
        }
        const std::size_t first = index;
        while (index < line.size() &&
               line[index] != ' ' && line[index] != '\t') {
            ++index;
        }
        if (first != index) {
            tokens.push_back(line.substr(first, index - first));
        }
    }
    return tokens;
}

bool is_valid_level(std::string_view level) {
    return level == "DEBUG" || level == "INFO" ||
           level == "WARN" || level == "ERROR";
}

} // namespace

ParseResult parse_line(std::string_view line) {
    const auto tokens = split_whitespace(line);
    if (tokens.size() != 6) {
        return ParseError::wrong_field_count;
    }
    if (!is_valid_timestamp(tokens[0])) {
        return ParseError::bad_timestamp;
    }
    if (!is_valid_level(tokens[1])) {
        return ParseError::bad_level;
    }
    if (tokens[2] != "request" || tokens[3] != "completed") {
        return ParseError::wrong_field_count;
    }

    constexpr std::string_view status_prefix{"status="};
    constexpr std::string_view duration_prefix{"duration_ms="};
    if (!tokens[4].starts_with(status_prefix) ||
        !tokens[5].starts_with(duration_prefix)) {
        return ParseError::wrong_field_count;
    }

    const auto status = parse_nonnegative_int(tokens[4].substr(status_prefix.size()));
    if (!status || *status < 100 || *status > 599) {
        return ParseError::bad_status;
    }

    const auto duration =
        parse_nonnegative_int(tokens[5].substr(duration_prefix.size()));
    if (!duration) {
        return ParseError::bad_duration;
    }

    return LogRecord{
        std::string{tokens[0]},
        std::string{tokens[1]},
        *status,
        *duration,
    };
}

void add_record(Statistics& statistics, const LogRecord& record) {
    ++statistics.valid_lines;
    ++statistics.status_counts[record.status];
    const std::string hour_key{record.timestamp.substr(0, 14)};
    ++statistics.hourly_counts[hour_key];
    statistics.total_duration_ms += record.duration_ms;
    statistics.average_duration_ms =
        static_cast<double>(statistics.total_duration_ms) /
        static_cast<double>(statistics.valid_lines);
}

Statistics analyze_lines(std::span<const std::string> lines) {
    Statistics statistics;
    statistics.total_lines = lines.size();
    for (const std::string& line : lines) {
        const auto parsed = parse_line(line);
        if (const auto* record = std::get_if<LogRecord>(&parsed)) {
            add_record(statistics, *record);
        } else {
            ++statistics.invalid_lines;
        }
    }
    if (statistics.valid_lines == 0) {
        statistics.average_duration_ms = 0.0;
    }
    return statistics;
}

CliResult parse_cli(std::span<const std::string_view> arguments) {
    CliOptions options;
    for (const std::string_view argument : arguments) {
        if (argument == "--help" || argument == "-h") {
            options.help = true;
        } else if (argument == "--strict") {
            options.strict = true;
        } else if (argument.starts_with('-')) {
            return CliError::unknown_option;
        } else if (options.path.empty()) {
            options.path = std::string{argument};
        } else {
            return CliError::too_many_paths;
        }
    }
    if (!options.help && options.path.empty()) {
        return CliError::missing_path;
    }
    return options;
}

} // namespace log_analyzer
