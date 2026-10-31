#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace {

bool write_lines(const std::filesystem::path& path) {
    std::ofstream output{path, std::ios::out | std::ios::trunc};
    if (!output) {
        return false;
    }

    output << "alpha\n";
    output << "beta\n";
    output << "gamma\n";
    output.close();
    return static_cast<bool>(output);
}

bool read_lines(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input) {
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        std::cout << line << '\n';
    }
    return input.eof() && !input.bad();
}

} // namespace

int main() {
    const auto path =
        std::filesystem::temp_directory_path() /
        "cpp20_course_file_io_example.txt";

    if (!write_lines(path)) {
        std::cerr << "failed to write " << path << '\n';
        return 1;
    }

    if (!read_lines(path)) {
        std::cerr << "failed to read " << path << '\n';
        return 1;
    }

    std::error_code error;
    std::filesystem::remove(path, error);
    if (error) {
        std::cerr << "failed to remove " << path << ": "
                  << error.message() << '\n';
        return 1;
    }
}
