# 第 8 章：模板、类型推导与泛型设计

## 学习目标

- 读懂函数模板和类模板的实例化过程。
- 掌握模板参数推导、`auto` 和 `decltype` 的基本规则。
- 理解模板定义通常为何放在头文件。
- 会用可变参数模板和折叠表达式处理参数包。
- 知道 SFINAE 的历史角色，并能用 Concepts 写成更清晰的约束。

## 1. 函数模板

```cpp
template <class T>
T maximum(T a, T b) {
    return a < b ? b : a;
}
```

调用 `maximum(3, 7)` 时，编译器推导 `T = int`，实例化一个具体函数。模板不是运行时泛型的动态分派，而是编译期生成代码。

显式指定类型：

```cpp
maximum<double>(3, 7.5);
```

这里 `3` 被转换为 `double`，两个参数都变成 `double`。

混合类型会推导失败：

```cpp
// maximum(3, 7.5); // T 无法同时是 int 和 double
```

可以设计两个模板参数，或者明确转换：

```cpp
template <class A, class B>
auto maximum(A a, B b) {
    using Common = std::common_type_t<A, B>;
    return a < b ? static_cast<Common>(b) : static_cast<Common>(a);
}
```

但返回类型和比较语义仍要验证。模板接口同样需要认真设计，不是把所有类型都硬塞进去。

## 2. 类模板

```cpp
template <class T>
class Box {
public:
    explicit Box(T value) : value_{std::move(value)} {}

    const T& value() const noexcept {
        return value_;
    }

private:
    T value_;
};

Box<int> integer_box{42};
Box<std::string> text_box{"hello"};
```

类模板实参推导（CTAD）允许：

```cpp
Box box{42}; // 推导 Box<int>
```

CTAD 来自构造函数，推导规则并不总是显而易见。接口复杂时显式写出类模板参数更可读。

## 3. `auto`、`decltype` 和引用折叠

```cpp
int value = 42;
int& ref = value;

auto a = value;       // int
auto b = ref;         // int，引用被丢弃
auto& c = ref;        // int&
const auto& d = value; // const int&
```

`decltype` 保留表达式细节：

```cpp
decltype(value) x = 1;     // int
decltype(ref) y = value;   // int&
decltype((value)) z = y;   // int&，多括号让表达式成为左值
```

通用代码中，`auto&&` 常与转发引用配合；在范围 `for` 中，`const auto&` 表示只读观察，`auto&&` 有时用于保留元素的值类别。

## 4. 模板定义为什么在头文件

编译器在看到调用点时通常需要看到模板完整定义，才能实例化具体函数。因此普通函数模板一般放在头文件：

```cpp
// math.hpp
#pragma once

template <class T>
T square(T value) {
    return value * value;
}
```

可以使用显式实例化把定义放在 `.cpp` 中，但只适合有限的类型集合：

```cpp
// math.cpp
template int square<int>(int);
```

这也是为什么标准库的大部分实现都在头文件中。

## 5. 可变参数模板

```cpp
#include <iostream>

template <class... Args>
void print_all(const Args&... args) {
    ((std::cout << args << ' '), ...);
    std::cout << '\n';
}
```

`Args...` 是类型参数包，`args...` 是函数参数包。`((expr), ...)` 是逗号折叠表达式，依次展开每个参数。

求和：

```cpp
template <class... Values>
auto sum(Values... values) {
    return (values + ... + 0);
}
```

空参数包时，加号折叠需要初始值，否则没有可推导的结果。折叠的优先级和结合性也必须检查。

## 6. 模板里的依赖名称

模板参数依赖的类型成员有时需要 `typename`：

```cpp
template <class Container>
using ValueType = typename Container::value_type;
```

依赖的模板名需要 `template` 关键字：

```cpp
// value.template get<0>()
```

这类语法看起来繁琐，但它是告诉编译器“这里不是在比较大小”。

## 7. SFINAE 与 Concepts

早期常用 `enable_if` 约束模板：

```cpp
#include <type_traits>

template <class T,
          std::enable_if_t<std::is_integral_v<T>, int> = 0>
T doubled(T value) {
    return static_cast<T>(value * 2);
}
```

SFINAE 表示“替换失败不算程序错误，只是该候选不可用”。问题是对初学者和错误诊断都不友好。

C++20 Concepts 直接表达约束：

```cpp
#include <concepts>

template <std::integral T>
T doubled(T value) {
    return static_cast<T>(value * 2);
}
```

自定义 Concept：

```cpp
template <class T>
concept Addable = requires(T a, T b) {
    { a + b } -> std::convertible_to<T>;
};

template <Addable T>
T add(T a, T b) {
    return a + b;
}
```

概念不是运行时检查，也不自动保证每个语义性质。`a + b` 能编译不代表加法满足结合律，也不代表“无穷”等业务定义。

## 8. 泛型接口的层级

编写模板时，从弱约束到强约束逐步思考：

1. 类型需要可复制吗？
2. 需要可比较吗？
3. 需要能索引吗？
4. 需要产生哪种返回类型？
5. 是否允许抛出异常？
6. 是否要求迭代器/范围？

例如，求范围长度：

```cpp
#include <iterator>
#include <ranges>

template <std::ranges::sized_range Range>
auto count_items(const Range& range) {
    return std::ranges::size(range);
}
```

如果只要求可遍历，`input_range` 更宽松；如果要求随机访问，使用更强概念。约束越强，错误越早出现，但通用性会降低。

## 9. 模板不是宏

模板：

- 有类型检查。
- 遵循作用域和名称查找。
- 可通过 Concepts 给出约束。
- 错误信息通常可以定位到实参类型。

宏：

- 纯文本替换。
- 没有类型系统。
- 容易重复求值参数。

不要用宏模拟泛型。用 `constexpr` 函数、模板、内联函数和泛型 lambda 替代绝大多数宏需求。

## 练习 1：泛型 `clamp`

实现：

```cpp
template <class T>
const T& clamp_value(const T& value, const T& low, const T& high);
```

要求：

- 当 `low > high` 时行为明确。
- 至少支持可比较的 `int` 和 `std::string`。
- 不要返回临时对象引用。

再比较 `std::clamp`，说明标准接口的约定。

## 练习 2：参数包求和

实现：

```cpp
template <class... Ts>
auto safe_sum(Ts... values);
```

先只要求能处理 `int` 和 `double`。讨论空参数包、类型提升和混合类型的结果类型。

## 练习 3：自定义 Concept

定义 `Number`，要求类型支持：

- 默认构造？
- 加、减、乘？
- 与自身 `0` 比较？

设计时不要塞入不必要的要求。分别写一个只约束加法、一个约束算术用途的 Concept，并写 `static_assert` 测试。

## 自测

1. 函数模板什么时候被实例化？
2. 为什么模板定义通常放在头文件？
3. `auto x = ref;` 与 `auto& x = ref;` 的差别是什么？
4. SFINAE 和 Concepts 的约束目标有什么相同点？
5. Concept 能否保证运行时的语义正确？

## 下一步

[第 9 章](09_move_semantics.md)将系统解释值类别、移动和完美转发，这是读懂现代标准库接口的关键。
