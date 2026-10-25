#pragma once

#include "log_analyzer/analyzer.hpp"

#include <string>

namespace log_analyzer {

std::string format_report(const Statistics& statistics);

} // namespace log_analyzer
