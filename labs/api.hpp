#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#ifdef CPP20_COURSE_STARTER
#include "starter/templates.hpp"
#else
#include "solutions/templates.hpp"
#endif

namespace course_labs {

struct GreetingResult {
    std::string name;
    int age{};

    bool operator==(const GreetingResult&) const = default;
};

std::optional<int> parse_integer(std::string_view text);
std::string format_greeting(const GreetingResult& person);

bool safe_add(int lhs, int rhs, int& result);
bool is_valid_username(std::string_view username);

std::string fizzbuzz(int value);
std::optional<std::size_t> binary_search(
    std::span<const int> values, int target);
bool is_prime(int value);
std::vector<int> cleanup_scores(std::vector<int> scores);

int sum(std::span<const int> values);
std::optional<double> average(std::span<const int> values);

class Temperature {
public:
    explicit Temperature(double celsius);

    double celsius() const noexcept;
    double fahrenheit() const noexcept;

private:
    double celsius_;
};

class BankAccount {
public:
    explicit BankAccount(int opening_balance_cents);

    void deposit(int cents);
    bool withdraw(int cents);
    int balance_cents() const noexcept;

private:
    int balance_cents_;
};

class Shape {
public:
    virtual ~Shape() = default;
    virtual double area() const = 0;
    virtual double perimeter() const = 0;
};

class Circle final : public Shape {
public:
    explicit Circle(double radius);

    double area() const override;
    double perimeter() const override;

private:
    double radius_;
};

class Rectangle final : public Shape {
public:
    Rectangle(double width, double height);

    double area() const override;
    double perimeter() const override;

private:
    double width_;
    double height_;
};

double total_area(const std::vector<std::unique_ptr<Shape>>& shapes);

struct WordCount {
    std::string word;
    int count{};

    bool operator==(const WordCount&) const = default;
};

std::vector<WordCount> top_words(
    std::span<const std::string> words, std::size_t limit);

class MoveOnlyBuffer {
public:
    explicit MoveOnlyBuffer(std::size_t size);
    ~MoveOnlyBuffer();

    MoveOnlyBuffer(const MoveOnlyBuffer&) = delete;
    MoveOnlyBuffer& operator=(const MoveOnlyBuffer&) = delete;

    MoveOnlyBuffer(MoveOnlyBuffer&& other) noexcept;
    MoveOnlyBuffer& operator=(MoveOnlyBuffer&& other) noexcept;

    std::size_t size() const noexcept;
    int& operator[](std::size_t index);
    const int& operator[](std::size_t index) const;

private:
    std::vector<int> values_;
};

enum class ParseError {
    empty,
    non_digit,
    out_of_range
};

using ParsedNumber = std::variant<int, ParseError>;

ParsedNumber parse_nonnegative_int(std::string_view text);
std::unique_ptr<Temperature> make_temperature(double celsius);

std::vector<int> first_even_squares(
    std::span<const int> values, std::size_t limit);

std::string format_price_row(
    std::string_view name, int quantity, double total);

int parallel_sum(std::span<const int> values, std::size_t worker_count);
int count_until_stopped(std::stop_token token, int maximum);

class TodoList {
public:
    void add(std::string title);
    bool remove(std::string_view title);
    std::size_t size() const noexcept;

private:
    std::vector<std::string> tasks_;
};

class SquareGenerator {
public:
    explicit SquareGenerator(int limit);
    std::optional<int> next();

private:
    int current_{};
    int limit_{};
};

} // namespace course_labs
