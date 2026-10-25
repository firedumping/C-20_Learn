#include "log_analyzer/analyzer.hpp"
#include "log_analyzer/report.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

void print_usage(std::ostream& output) {
    output << "Usage: log_analyzer [--strict] <path>\n";
}

const char* cli_error_message(log_analyzer::CliError error) {
    using log_analyzer::CliError;
    switch (error) {
    case CliError::missing_path:
        return "missing input path";
    case CliError::too_many_paths:
        return "too many input paths";
    case CliError::unknown_option:
        return "unknown option";
    }
    return "invalid arguments";
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string_view> arguments;
    arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    const auto cli = log_analyzer::parse_cli(arguments);
    if (const auto* error = std::get_if<log_analyzer::CliError>(&cli)) {
        std::cerr << "error: " << cli_error_message(*error) << '\n';
        print_usage(std::cerr);
        return 1;
    }

    const auto& options = std::get<log_analyzer::CliOptions>(cli);
    if (options.help) {
        print_usage(std::cout);
        return 0;
    }

    std::ifstream input{options.path};
    if (!input) {
        std::cerr << "error: cannot open file '" << options.path << "'\n";
        return 2;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }
    if (input.bad()) {
        std::cerr << "error: failed while reading '" << options.path << "'\n";
        return 2;
    }

    const auto statistics = log_analyzer::analyze_lines(lines);
    std::cout << log_analyzer::format_report(statistics);

    if (options.strict && statistics.invalid_lines != 0) {
        std::cerr << "error: strict mode found "
                  << statistics.invalid_lines << " invalid line(s)\n";
        return 3;
    }
    return 0;
}
