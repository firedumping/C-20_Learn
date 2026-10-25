#pragma once

#include <concepts>
#include <type_traits>

namespace labs08 {

template <class T>
concept Addable = requires(T value) {
    value + value;
};

// TODO: constrain T to a type for which lhs + rhs is convertible back to T.
template <Addable T>
T add_values(const T& lhs, const T& rhs) {
    (void)lhs;
    (void)rhs;
    return T{};
}

template <class T>
concept ArithmeticValue = std::is_arithmetic_v<T>;

// TODO: constrain to arithmetic types and return value squared.
template <ArithmeticValue T>
constexpr T square_value(T value) {
    (void)value;
    return T{};
}

} // namespace labs08
