#include "api.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

int checks = 0;
int failures = 0;

void check(bool condition, std::string_view message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

template <class T>
void expect_equal(const T& actual, const T& expected, std::string_view message) {
    check(actual == expected, message);
}

void expect_close(double actual, double expected, double tolerance,
           std::string_view message) {
    check(std::abs(actual - expected) <= tolerance, message);
}

template <class Exception, class Fn>
void throws(Fn&& fn, std::string_view message) {
    try {
        fn();
        check(false, message);
    } catch (const Exception&) {
        check(true, message);
    } catch (...) {
        check(false, message);
    }
}

void test_lab01_types_and_io() {
    expect_equal(course_labs::parse_integer("42"), std::optional<int>{42},
          "lab01: parse positive integer");
    expect_equal(course_labs::parse_integer("+7"), std::optional<int>{7},
          "lab01: parse explicit positive sign");
    expect_equal(course_labs::parse_integer("-123"), std::optional<int>{-123},
          "lab01: parse negative integer");
    expect_equal(course_labs::parse_integer(""), std::optional<int>{},
          "lab01: reject empty integer");
    expect_equal(course_labs::parse_integer("12x"), std::optional<int>{},
          "lab01: reject trailing non-digit");
    expect_equal(course_labs::parse_integer("2147483647"),
          std::optional<int>{std::numeric_limits<int>::max()},
          "lab01: parse int max");
    expect_equal(course_labs::parse_integer("-2147483648"),
          std::optional<int>{std::numeric_limits<int>::min()},
          "lab01: parse int min");
    expect_equal(course_labs::parse_integer("2147483648"), std::optional<int>{},
          "lab01: reject int overflow");
    expect_equal(course_labs::parse_integer(std::string(100, '9')), std::optional<int>{},
          "lab01: reject very long integer without arithmetic overflow");

    expect_equal(course_labs::format_greeting({"Ada", 36}),
          std::string{"Hello, Ada! Age: 36"},
          "lab01: format greeting");
}

void test_lab02_safe_values() {
    int result = 0;
    check(course_labs::safe_add(20, 22, result) && result == 42,
          "lab02: safe_add succeeds for ordinary values");
    check(!course_labs::safe_add(std::numeric_limits<int>::max(), 1, result),
          "lab02: safe_add rejects positive overflow");
    check(!course_labs::safe_add(std::numeric_limits<int>::min(), -1, result),
          "lab02: safe_add rejects negative overflow");

    check(course_labs::is_valid_username("Ada_01"),
          "lab02: valid username");
    check(!course_labs::is_valid_username("ab"),
          "lab02: username too short");
    check(!course_labs::is_valid_username("1ada"),
          "lab02: username cannot start with digit");
    check(!course_labs::is_valid_username("Ada Lovelace"),
          "lab02: username rejects whitespace");
    check(!course_labs::is_valid_username("abcdefghijklmnopq"),
          "lab02: username too long");
}

void test_lab03_control_flow() {
    expect_equal(course_labs::fizzbuzz(1), std::string{"1"}, "lab03: ordinary FizzBuzz");
    expect_equal(course_labs::fizzbuzz(3), std::string{"Fizz"}, "lab03: multiple of 3");
    expect_equal(course_labs::fizzbuzz(5), std::string{"Buzz"}, "lab03: multiple of 5");
    expect_equal(course_labs::fizzbuzz(15), std::string{"FizzBuzz"},
          "lab03: multiple of 15");
    throws<std::invalid_argument>([] { (void)course_labs::fizzbuzz(0); },
                                  "lab03: FizzBuzz rejects zero");

    check(!course_labs::is_prime(-7), "lab03: negative is not prime");
    check(!course_labs::is_prime(1), "lab03: one is not prime");
    check(course_labs::is_prime(2), "lab03: two is prime");
    check(course_labs::is_prime(97), "lab03: ninety-seven is prime");
    check(!course_labs::is_prime(100), "lab03: one hundred is not prime");
    check(course_labs::is_prime(2'147'483'647),
          "lab03: large Mersenne prime without i*i overflow");

    const auto cleaned = course_labs::cleanup_scores(
        {-10, 0, 59, 100, 101, 42});
    expect_equal(cleaned, std::vector<int>{0, 59, 100, 42},
                 "lab03: erase_if keeps only scores in [0, 100]");

    const std::vector<int> values{1, 3, 5, 7, 9};
    expect_equal(course_labs::binary_search(values, 5), std::optional<std::size_t>{2},
          "lab03: binary search finds middle");
    expect_equal(course_labs::binary_search(values, 4), std::optional<std::size_t>{},
          "lab03: binary search reports missing");
    expect_equal(course_labs::binary_search(std::span<const int>{}, 4),
          std::optional<std::size_t>{}, "lab03: binary search empty range");
}

void test_lab04_views_and_lifetime() {
    const std::vector<int> values{1, 2, 3, 4};
    expect_equal(course_labs::sum(values), 10, "lab04: sum spans");
    expect_equal(course_labs::average(values), std::optional<double>{2.5},
          "lab04: average spans");
    expect_equal(course_labs::average(std::span<const int>{}), std::optional<double>{},
          "lab04: empty average has no value");
}

void test_lab05_raii_and_invariants() {
    course_labs::Temperature freezing{0.0};
    expect_close(freezing.celsius(), 0.0, 1e-12, "lab05: store Celsius");
    expect_close(freezing.fahrenheit(), 32.0, 1e-12,
          "lab05: Celsius to Fahrenheit");
    throws<std::invalid_argument>([] { course_labs::Temperature bad{-274.0}; },
                                  "lab05: reject below absolute zero");

    course_labs::BankAccount account{1'000};
    account.deposit(250);
    expect_equal(account.balance_cents(), 1'250, "lab05: account deposit");
    check(account.withdraw(500), "lab05: valid withdrawal");
    expect_equal(account.balance_cents(), 750, "lab05: withdrawal updates balance");
    check(!account.withdraw(1'000), "lab05: overdraft is rejected");
    expect_equal(account.balance_cents(), 750,
          "lab05: failed withdrawal keeps invariant");
    throws<std::invalid_argument>(
        [] { course_labs::BankAccount bad{-1}; },
        "lab05: reject negative opening balance");
    throws<std::invalid_argument>(
        [&account] { account.deposit(0); },
        "lab05: reject non-positive deposit");
}

void test_lab06_polymorphism() {
    std::vector<std::unique_ptr<course_labs::Shape>> shapes;
    shapes.push_back(std::make_unique<course_labs::Circle>(1.0));
    shapes.push_back(std::make_unique<course_labs::Rectangle>(2.0, 3.0));

    expect_close(course_labs::total_area(shapes), 3.141592653589793 + 6.0, 1e-9,
          "lab06: total polymorphic area");
    expect_close(shapes[0]->perimeter(), 2.0 * 3.141592653589793, 1e-9,
          "lab06: circle perimeter");
    expect_close(shapes[1]->perimeter(), 10.0, 1e-9,
          "lab06: rectangle perimeter");
    throws<std::invalid_argument>(
        [] { course_labs::Circle invalid{-1.0}; },
        "lab06: reject negative circle radius");
    throws<std::invalid_argument>(
        [] { course_labs::Rectangle invalid{0.0, 2.0}; },
        "lab06: reject zero rectangle dimension");
}

void test_lab07_algorithms() {
    const std::vector<std::string> words{
        "Apple", "banana", "apple!", "", "BANANA", "cherry", "banana"
    };
    const auto actual = course_labs::top_words(words, 10);
    const std::vector<course_labs::WordCount> expected{
        {"banana", 3},
        {"apple", 2},
        {"cherry", 1},
    };
    expect_equal(actual, expected,
          "lab07: normalize, count, and sort words");

    const auto limited = course_labs::top_words(words, 2);
    check(limited.size() == 2 && limited[0].word == "banana",
          "lab07: top_words applies limit");
}

void test_lab08_templates() {
    static_assert(labs08::Addable<int>);
    static_assert(labs08::Addable<std::string>);
    static_assert(labs08::ArithmeticValue<double>);
    expect_equal(labs08::add_values(20, 22), 42, "lab08: add ints");
    expect_equal(labs08::add_values(std::string{"C++"}, std::string{"20"}),
          std::string{"C++20"}, "lab08: add strings");
    expect_equal(labs08::square_value(7), 49, "lab08: constexpr square");
}

void test_lab09_move() {
    course_labs::MoveOnlyBuffer first{3};
    first[0] = 10;
    first[1] = 20;
    first[2] = 30;

    course_labs::MoveOnlyBuffer second{std::move(first)};
    expect_equal(second.size(), std::size_t{3}, "lab09: move constructor size");
    if (second.size() == 3) {
        expect_equal(second[1], 20, "lab09: move constructor transfers elements");
    }
    expect_equal(first.size(), std::size_t{0}, "lab09: moved-from source is empty");
    expect_equal(first.size(), std::size_t{0},
          "lab09: moved-from source can be observed safely");

    course_labs::MoveOnlyBuffer third{1};
    third = std::move(second);
    expect_equal(third.size(), std::size_t{3}, "lab09: move assignment");
    expect_equal(second.size(), std::size_t{0},
          "lab09: move assignment leaves source valid");
}

void test_lab10_errors_and_ownership() {
    const auto good = course_labs::parse_nonnegative_int("2147483647");
    check(std::holds_alternative<int>(good) &&
              std::get<int>(good) == std::numeric_limits<int>::max(),
          "lab10: parse non-negative max int");

    const auto empty = course_labs::parse_nonnegative_int("");
    check(std::holds_alternative<course_labs::ParseError>(empty) &&
              std::get<course_labs::ParseError>(empty) == course_labs::ParseError::empty,
          "lab10: distinguish empty input");

    const auto bad = course_labs::parse_nonnegative_int("-1");
    check(std::holds_alternative<course_labs::ParseError>(bad) &&
              std::get<course_labs::ParseError>(bad) == course_labs::ParseError::non_digit,
          "lab10: distinguish non-digit input");

    const auto overflow = course_labs::parse_nonnegative_int("2147483648");
    check(std::holds_alternative<course_labs::ParseError>(overflow) &&
              std::get<course_labs::ParseError>(overflow) ==
                  course_labs::ParseError::out_of_range,
          "lab10: distinguish out-of-range input");

    auto temperature = course_labs::make_temperature(20.0);
    check(temperature != nullptr, "lab10: unique_ptr factory returns object");
    if (temperature != nullptr) {
        expect_close(temperature->fahrenheit(), 68.0, 1e-12,
              "lab10: unique_ptr owns usable object");
    }
}

void test_lab11_cpp20_features() {
    const std::vector<int> values{1, 2, 3, 4, 5, 6, 7, 8};
    expect_equal(course_labs::first_even_squares(values, 3),
          std::vector<int>{4, 16, 36},
          "lab11: lazy filter-transform-take pipeline");
    expect_equal(course_labs::first_even_squares(values, 0), std::vector<int>{},
          "lab11: take zero values");

    expect_equal(course_labs::format_price_row("apple", 3, 5.5),
          std::string{"apple         3     5.50"},
          "lab11: std::format aligned report row");
}

void test_lab12_concurrency() {
    std::vector<int> values(1'000);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<int>(i);
    }
    expect_equal(course_labs::parallel_sum(values, 4), 499'500,
          "lab12: parallel sum matches closed form");
    expect_equal(course_labs::parallel_sum(std::span<const int>{}, 8), 0,
          "lab12: empty parallel sum");
    throws<std::invalid_argument>(
        [] { (void)course_labs::parallel_sum(std::span<const int>{}, 0); },
        "lab12: reject zero workers");

    std::stop_source stopped;
    (void)stopped.request_stop();
    expect_equal(course_labs::count_until_stopped(stopped.get_token(), 100), 0,
          "lab12: pre-requested stop prevents work");

    std::stop_source running;
    expect_equal(course_labs::count_until_stopped(running.get_token(), 25), 25,
          "lab12: token without stop request runs to limit");
}

void test_lab13_testable_domain() {
    course_labs::TodoList todos;
    throws<std::invalid_argument>(
        [&todos] { todos.add(""); },
        "lab13: reject empty task");

    todos.add("learn C++");
    todos.add("write tests");
    expect_equal(todos.size(), std::size_t{2}, "lab13: add tasks");
    check(todos.remove("learn C++"), "lab13: remove existing task");
    check(!todos.remove("missing"), "lab13: removal reports missing task");
    expect_equal(todos.size(), std::size_t{1}, "lab13: size after removal");
}

void test_lab14_stateful_iteration() {
    course_labs::SquareGenerator generator{4};
    expect_equal(generator.next(), std::optional<int>{0},
          "lab14: generator first value");
    expect_equal(generator.next(), std::optional<int>{1},
          "lab14: generator resumes after first value");
    expect_equal(generator.next(), std::optional<int>{4},
          "lab14: generator third value");
    expect_equal(generator.next(), std::optional<int>{9},
          "lab14: generator last value");
    expect_equal(generator.next(), std::optional<int>{},
          "lab14: generator completion");
    expect_equal(generator.next(), std::optional<int>{},
          "lab14: completed generator stays complete");

    course_labs::SquareGenerator empty{0};
    expect_equal(empty.next(), std::optional<int>{},
          "lab14: empty generator completes immediately");
}

} // namespace

int main() {
    test_lab01_types_and_io();
    test_lab02_safe_values();
    test_lab03_control_flow();
    test_lab04_views_and_lifetime();
    test_lab05_raii_and_invariants();
    test_lab06_polymorphism();
    test_lab07_algorithms();
    test_lab08_templates();
    test_lab09_move();
    test_lab10_errors_and_ownership();
    test_lab11_cpp20_features();
    test_lab12_concurrency();
    test_lab13_testable_domain();
    test_lab14_stateful_iteration();

    std::cout << checks - failures << '/' << checks << " checks passed\n";
    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    return 0;
}
