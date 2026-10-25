#pragma once

#include <concepts>
#include <type_traits>

namespace labs08 {

template <class T>
concept Addable = requires(const T& lhs, const T& rhs) {
    { lhs + rhs } -> std::convertible_to<T>;
};

template <Addable T>
T add_values(const T& lhs, const T& rhs) {
    return lhs + rhs;
}

template <class T>
concept ArithmeticValue =
    std::is_arithmetic_v<T> &&
    requires(T value) {
        value * value;
    };

template <ArithmeticValue T>
constexpr T square_value(T value) {
    return value * value;
}

} // namespace labs08
