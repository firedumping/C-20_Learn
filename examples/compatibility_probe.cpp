#include <algorithm>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <version>

#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
#include <format>
#define COURSE_HAS_FORMAT 1
#else
#define COURSE_HAS_FORMAT 0
#endif

#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 201911L
#include <ranges>
#define COURSE_HAS_RANGES 1
#else
#define COURSE_HAS_RANGES 0
#endif

#if defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L
#define COURSE_HAS_JTHREAD 1
#else
#define COURSE_HAS_JTHREAD 0
#endif

int main() {
    std::cout << "__cplusplus = " << __cplusplus << '\n';
    std::cout << "<format>:  "
              << (COURSE_HAS_FORMAT ? "yes" : "no") << '\n';
    std::cout << "<ranges>:  "
              << (COURSE_HAS_RANGES ? "yes" : "no") << '\n';
    std::cout << "jthread:   "
              << (COURSE_HAS_JTHREAD ? "yes" : "no") << '\n';

#if COURSE_HAS_FORMAT
    const std::string text = std::format("format works: {}", 42);
    std::cout << text << '\n';
#endif

#if COURSE_HAS_RANGES
    std::vector<int> values{3, 1, 2};
    std::ranges::sort(values);
    std::cout << "ranges::sort works\n";
#endif

#if COURSE_HAS_JTHREAD
    std::jthread worker{[] {
        std::cout << "jthread works\n";
    }};
#endif
}
