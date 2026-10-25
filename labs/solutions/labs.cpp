#include "api.hpp"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <format>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace labs {

std::optional<int> parse_integer(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    bool negative = false;
    std::size_t index = 0;
    if (text.front() == '-' || text.front() == '+') {
        negative = text.front() == '-';
        index = 1;
        if (index == text.size()) {
            return std::nullopt;
        }
    }

    long long value = 0;
    const long long positive_limit =
        static_cast<long long>(std::numeric_limits<int>::max()) + 1;
    for (; index < text.size(); ++index) {
        const char ch = text[index];
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        const int digit = ch - '0';
        if (value > (positive_limit - digit) / 10) {
            return std::nullopt;
        }
        value = value * 10 + digit;
    }

    if (negative) {
        value = -value;
    }
    if (value < std::numeric_limits<int>::min() ||
        value > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }
    return static_cast<int>(value);
}

std::string format_greeting(const GreetingResult& person) {
    return std::format("Hello, {}! Age: {}", person.name, person.age);
}

bool safe_add(int lhs, int rhs, int& result) {
    if ((rhs > 0 && lhs > std::numeric_limits<int>::max() - rhs) ||
        (rhs < 0 && lhs < std::numeric_limits<int>::min() - rhs)) {
        return false;
    }
    result = lhs + rhs;
    return true;
}

bool is_valid_username(std::string_view username) {
    if (username.size() < 3 || username.size() > 16) {
        return false;
    }
    if (username.front() >= '0' && username.front() <= '9') {
        return false;
    }
    return std::ranges::all_of(username, [](char ch) {
        return (ch >= 'a' && ch <= 'z') ||
               (ch >= 'A' && ch <= 'Z') ||
               (ch >= '0' && ch <= '9') ||
               ch == '_';
    });
}

std::string fizzbuzz(int value) {
    if (value <= 0) {
        throw std::invalid_argument{"fizzbuzz requires a positive number"};
    }
    if (value % 15 == 0) {
        return "FizzBuzz";
    }
    if (value % 3 == 0) {
        return "Fizz";
    }
    if (value % 5 == 0) {
        return "Buzz";
    }
    return std::to_string(value);
}

std::optional<std::size_t> binary_search(
    std::span<const int> values, int target) {
    std::size_t left = 0;
    std::size_t right = values.size();
    while (left < right) {
        const std::size_t middle = left + (right - left) / 2;
        if (values[middle] < target) {
            left = middle + 1;
        } else if (values[middle] > target) {
            right = middle;
        } else {
            return middle;
        }
    }
    return std::nullopt;
}

bool is_prime(int value) {
    if (value < 2) {
        return false;
    }
    if (value == 2) {
        return true;
    }
    if (value % 2 == 0) {
        return false;
    }
    for (long long divisor = 3;
         divisor * divisor <= static_cast<long long>(value);
         divisor += 2) {
        if (value % static_cast<int>(divisor) == 0) {
            return false;
        }
    }
    return true;
}

int sum(std::span<const int> values) {
    long long result = 0;
    for (const int value : values) {
        result += value;
    }
    if (result < std::numeric_limits<int>::min() ||
        result > std::numeric_limits<int>::max()) {
        throw std::overflow_error{"sum does not fit in int"};
    }
    return static_cast<int>(result);
}

std::optional<double> average(std::span<const int> values) {
    if (values.empty()) {
        return std::nullopt;
    }
    double total = 0.0;
    for (const int value : values) {
        total += static_cast<double>(value);
    }
    return total / static_cast<double>(values.size());
}

Temperature::Temperature(double celsius) : celsius_{celsius} {
    if (!std::isfinite(celsius) || celsius < -273.15) {
        throw std::invalid_argument{"temperature is below absolute zero"};
    }
}

double Temperature::celsius() const noexcept {
    return celsius_;
}

double Temperature::fahrenheit() const noexcept {
    return celsius_ * 9.0 / 5.0 + 32.0;
}

BankAccount::BankAccount(int opening_balance_cents)
    : balance_cents_{opening_balance_cents} {
    if (opening_balance_cents < 0) {
        throw std::invalid_argument{"opening balance must be non-negative"};
    }
}

void BankAccount::deposit(int cents) {
    if (cents <= 0) {
        throw std::invalid_argument{"deposit must be positive"};
    }
    if (balance_cents_ > std::numeric_limits<int>::max() - cents) {
        throw std::overflow_error{"balance overflow"};
    }
    balance_cents_ += cents;
}

bool BankAccount::withdraw(int cents) {
    if (cents <= 0 || cents > balance_cents_) {
        return false;
    }
    balance_cents_ -= cents;
    return true;
}

int BankAccount::balance_cents() const noexcept {
    return balance_cents_;
}

Circle::Circle(double radius) : radius_{radius} {
    if (!std::isfinite(radius) || radius <= 0.0) {
        throw std::invalid_argument{"radius must be positive"};
    }
}

double Circle::area() const {
    constexpr double pi = 3.141592653589793;
    return pi * radius_ * radius_;
}

double Circle::perimeter() const {
    constexpr double pi = 3.141592653589793;
    return 2.0 * pi * radius_;
}

Rectangle::Rectangle(double width, double height)
    : width_{width}, height_{height} {
    if (!std::isfinite(width) || !std::isfinite(height) ||
        width <= 0.0 || height <= 0.0) {
        throw std::invalid_argument{"rectangle dimensions must be positive"};
    }
}

double Rectangle::area() const {
    return width_ * height_;
}

double Rectangle::perimeter() const {
    return 2.0 * (width_ + height_);
}

double total_area(const std::vector<std::unique_ptr<Shape>>& shapes) {
    double result = 0.0;
    for (const auto& shape : shapes) {
        result += shape->area();
    }
    return result;
}

std::vector<WordCount> top_words(
    std::span<const std::string> words, std::size_t limit) {
    std::map<std::string, int> counts;
    for (std::string_view token : words) {
        std::string normalized;
        normalized.reserve(token.size());
        for (const char raw_ch : token) {
            const auto ch = static_cast<unsigned char>(raw_ch);
            if (ch >= 'A' && ch <= 'Z') {
                normalized.push_back(static_cast<char>(ch - 'A' + 'a'));
            } else if ((ch >= 'a' && ch <= 'z') ||
                       (ch >= '0' && ch <= '9') ||
                       ch == '_') {
                normalized.push_back(static_cast<char>(ch));
            }
        }
        if (!normalized.empty()) {
            ++counts[normalized];
        }
    }

    std::vector<WordCount> result;
    result.reserve(counts.size());
    for (const auto& [word, count] : counts) {
        result.push_back(WordCount{word, count});
    }
    std::ranges::sort(result, [](const WordCount& lhs, const WordCount& rhs) {
        if (lhs.count != rhs.count) {
            return lhs.count > rhs.count;
        }
        return lhs.word < rhs.word;
    });
    if (result.size() > limit) {
        result.resize(limit);
    }
    return result;
}

MoveOnlyBuffer::MoveOnlyBuffer(std::size_t size) : values_(size) {}
MoveOnlyBuffer::~MoveOnlyBuffer() = default;

MoveOnlyBuffer::MoveOnlyBuffer(MoveOnlyBuffer&& other) noexcept
    : values_{std::move(other.values_)} {}

MoveOnlyBuffer& MoveOnlyBuffer::operator=(MoveOnlyBuffer&& other) noexcept {
    if (this != &other) {
        values_ = std::move(other.values_);
    }
    return *this;
}

std::size_t MoveOnlyBuffer::size() const noexcept {
    return values_.size();
}

int& MoveOnlyBuffer::operator[](std::size_t index) {
    return values_[index];
}

const int& MoveOnlyBuffer::operator[](std::size_t index) const {
    return values_[index];
}

ParsedNumber parse_nonnegative_int(std::string_view text) {
    if (text.empty()) {
        return ParseError::empty;
    }

    int value = 0;
    for (const char ch : text) {
        if (ch < '0' || ch > '9') {
            return ParseError::non_digit;
        }
        const int digit = ch - '0';
        if (value > (std::numeric_limits<int>::max() - digit) / 10) {
            return ParseError::out_of_range;
        }
        value = value * 10 + digit;
    }
    return value;
}

std::unique_ptr<Temperature> make_temperature(double celsius) {
    return std::make_unique<Temperature>(celsius);
}

std::vector<int> first_even_squares(
    std::span<const int> values, std::size_t limit) {
    auto pipeline = values
        | std::views::filter([](int value) { return value % 2 == 0; })
        | std::views::transform([](int value) {
              const long long squared =
                  static_cast<long long>(value) * value;
              if (squared > std::numeric_limits<int>::max()) {
                  throw std::overflow_error{"square does not fit in int"};
              }
              return static_cast<int>(squared);
          })
        | std::views::take(limit);

    std::vector<int> result;
    for (const int value : pipeline) {
        result.push_back(value);
    }
    return result;
}

std::string format_price_row(
    std::string_view name, int quantity, double total) {
    return std::format("{:<10}{:>5} {:>8.2f}", name, quantity, total);
}

int parallel_sum(std::span<const int> values, std::size_t worker_count) {
    if (worker_count == 0) {
        throw std::invalid_argument{"worker_count must be positive"};
    }
    if (values.empty()) {
        return 0;
    }

    const std::size_t active_workers =
        std::min(worker_count, values.size());
    std::vector<long long> partials(active_workers, 0);
    std::vector<std::jthread> workers;
    workers.reserve(active_workers);

    for (std::size_t worker = 0; worker < active_workers; ++worker) {
        const std::size_t first = values.size() * worker / active_workers;
        const std::size_t last = values.size() * (worker + 1) / active_workers;
        workers.emplace_back([values, first, last, &partials, worker] {
            long long total = 0;
            for (std::size_t index = first; index < last; ++index) {
                total += values[index];
            }
            partials[worker] = total;
        });
    }
    // jthread destructors request stop and join before partials goes away.
    workers.clear();

    long long total = 0;
    for (const long long partial : partials) {
        total += partial;
    }
    if (total < std::numeric_limits<int>::min() ||
        total > std::numeric_limits<int>::max()) {
        throw std::overflow_error{"sum does not fit in int"};
    }
    return static_cast<int>(total);
}

int count_until_stopped(std::stop_token token, int maximum) {
    int count = 0;
    while (count < maximum && !token.stop_requested()) {
        ++count;
    }
    return count;
}

void TodoList::add(std::string title) {
    if (title.empty()) {
        throw std::invalid_argument{"task title must not be empty"};
    }
    tasks_.push_back(std::move(title));
}

bool TodoList::remove(std::string_view title) {
    const auto found = std::ranges::find_if(
        tasks_, [title](const std::string& task) {
            return std::string_view{task} == title;
        });
    if (found == tasks_.end()) {
        return false;
    }
    tasks_.erase(found);
    return true;
}

std::size_t TodoList::size() const noexcept {
    return tasks_.size();
}

SquareGenerator::SquareGenerator(int limit) : limit_{limit} {}

std::optional<int> SquareGenerator::next() {
    if (current_ >= limit_) {
        return std::nullopt;
    }
    const long long squared = static_cast<long long>(current_) * current_;
    if (squared > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }
    const int value = static_cast<int>(squared);
    ++current_;
    return value;
}

} // namespace labs
