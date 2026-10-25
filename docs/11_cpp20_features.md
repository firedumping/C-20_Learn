# 第 11 章：C++20 核心特性

## 学习目标

- 用 Concepts 写可诊断、可组合的约束。
- 用 Ranges 组织声明式数据处理管线。
- 用 `std::span`、`std::string_view` 设计低成本借用接口。
- 用 `std::format` 类型安全地格式化文本。
- 理解 `<=>`、指定初始化器、`constinit`、`consteval`。
- 了解 `std::source_location`、`std::bit_cast`、`std::jthread` 等技术的位置。

## 1. Concepts

Concept 是一个布尔编译期谓词，用于约束模板参数：

```cpp
#include <concepts>

template <std::integral T>
T twice(T value) {
    return static_cast<T>(value * 2);
}
```

函数调用不满足约束时，编译器指出 `T` 不满足 `std::integral`。这比在函数体内出现难懂错误更早、更清晰。

### `requires` 表达式

```cpp
#include <concepts>
#include <string>

template <class T>
concept Named = requires(const T& value) {
    { value.name() } -> std::convertible_to<std::string_view>;
};

struct User {
    std::string name() const;
};

static_assert(Named<User>);
```

`requires` 检查表达式是否合法、返回类型是否满足要求，但不执行函数。

### 组合约束

```cpp
#include <concepts>
#include <ranges>

template <class R>
concept IntegerRange =
    std::ranges::input_range<R> &&
    std::same_as<std::ranges::range_value_t<R>, int>;

template <IntegerRange R>
int sum(const R& values) {
    int total = 0;
    for (int value : values) {
        total += value;
    }
    return total;
}
```

Concept 可以表达“接口能力”，不要把它变成业务规则替代品。`HasArea` 只能保证有 `area()`，不能保证面积非负。

## 2. Ranges

传统算法：

```cpp
std::sort(values.begin(), values.end());
auto it = std::find(values.begin(), values.end(), target);
```

C++20 Ranges：

```cpp
std::ranges::sort(values);
auto it = std::ranges::find(values, target);
```

Ranges 接受范围对象，直接使用迭代器和哨兵，接口更短。`<ranges>` 还提供惰性视图：

```cpp
#include <ranges>
#include <vector>

auto make_even_squares(const std::vector<int>& values) {
    return values
        | std::views::filter([](int value) { return value % 2 == 0; })
        | std::views::transform([](int value) { return value * value; });
}
```

这是惰性管线：

- `filter` 不会立刻创建新容器。
- `transform` 在迭代时计算。
- 视图可能是轻量对象，涉及生命周期和缓存语义。

使用：

```cpp
for (int value : make_even_squares(values)) {
    // 只处理符合条件并转换后的值
}
```

### 视图的生命周期风险

```cpp
auto bad() {
    std::vector<int> values{1, 2, 3};
    auto view = values | std::views::filter([](int x) { return x > 1; });
    return view; // 返回后 values 销毁，view 悬空
}
```

返回视图前必须确认底层序列活得更久。需要拥有结果时，把视图具体化为 `std::vector`：

```cpp
std::vector<int> result;
std::ranges::copy(view, std::back_inserter(result));
```

### `views::take`、`drop`、`reverse`

```cpp
auto first_three = values | std::views::take(3);
auto after_three = values | std::views::drop(3);
auto reversed = values | std::views::reverse;
```

`std::views::reverse` 需要底层范围满足双向范围要求，不是所有范围都支持。

## 3. `std::span`

连续序列的无所有权视图：

```cpp
#include <span>

std::span<const int> view{values};
```

常见用途：

- 接收 `std::vector<int>`、`std::array<int, N>` 或原生数组，而无需模板化或强制具体容器。
- 传递子区间。

```cpp
void process(std::span<const int> values);
```

`span` 不检查底层对象生命周期，也不检查访问边界（`operator[]`）；需要边界检查时使用 `.at()` 或自己验证。

不要返回指向局部数组的 span。

## 4. `std::format`

```cpp
#include <format>
#include <iostream>
#include <string>

std::string greeting(std::string_view name, int age) {
    return std::format("Hello, {}! Age: {}", name, age);
}
```

格式说明：

```cpp
std::format("{:>10}|{:.2f}", "item", 3.14159);
std::format("{0} {1} {0}", "x", "y");
```

优点：

- 类型安全，错误通常在编译期发现。
- 格式规则集中，避免 `printf` 可变参数不匹配。
- 可以扩展到用户类型。

自定义类型格式化需要特化 `std::formatter<T, CharT>`，这属于进阶内容；先学会用标准类型构造格式字符串。

## 5. 三路比较 `<=>`

```cpp
#include <compare>

struct Version {
    int major{};
    int minor{};
    int patch{};

    auto operator<=>(const Version&) const = default;
};
```

默认比较按成员声明顺序生成，并产生 `std::strong_ordering`。对于 `double`，通常得到 `partial_ordering`，因为 NaN 不满足全序。

```cpp
if (Version{1, 2, 0} < Version{1, 3, 0}) {
    // true
}
```

需要自定义语义时，不要让默认比较误导调用者。例如版本比较可能需要忽略构建号，或 `1.10` 大于 `1.2`。

## 6. 指定初始化器

```cpp
struct Options {
    std::string host;
    int port{8080};
    bool tls{false};
};

Options options{
    .host = "example.com",
    .port = 443,
    .tls = true,
};
```

限制：

- 必须按成员声明顺序指定。
- 不能与按位置初始化混用。
- 只适用于聚合类型。

它让配置值在调用点可读，但不要用超大的聚合结构替代清晰的构造函数或不变量。

## 7. 编译期工具

### `consteval`

```cpp
consteval int compile_time_square(int x) {
    return x * x;
}

constexpr int value = compile_time_square(4);
// int runtime = compile_time_square(5); // 这里仍可在编译期求值
```

`consteval` 函数必须在常量求值上下文中调用，不适合普通运行时输入。

### `constinit`

```cpp
constinit int global_value = 10;
```

要求变量具有静态初始化，避免运行期初始化顺序问题，同时变量之后仍可修改。

### `consteval` 与 `constexpr` 的选择

- `constexpr` 函数可以在编译期或运行期求值。
- `consteval` 函数只允许编译期求值。
- `constexpr` 变量是常量表达式。
- `constinit` 要求初始化发生在静态初始化阶段，不要求变量不可变。

## 8. 标准库小工具

```cpp
#include <bit>
#include <cstdint>
#include <source_location>

constexpr bool little_endian = std::endian::native == std::endian::little;

std::uint32_t bits = std::bit_cast<std::uint32_t>(3.5f);
```

`std::bit_cast` 复制对象表示，要求源和目标同大小、可平凡复制。它不是任意“类型转换”，也不解决字节序。

`std::source_location`：

```cpp
#include <source_location>

void log(std::string_view message,
         const std::source_location location =
             std::source_location::current()) {
    // location.file_name(), line(), column(), function_name()
}
```

可用于轻量日志和断言辅助，但不要在每个热点路径保存昂贵字符串。

## 9. 时间、数值与位操作

C++20 增强了日历和数值工具：

```cpp
#include <chrono>
#include <numbers>

constexpr auto date =
    std::chrono::year{2026} / std::chrono::October / 25;
static_assert(date.ok());

const auto today = std::chrono::floor<std::chrono::days>(
    std::chrono::system_clock::now());

constexpr double pi = std::numbers::pi;
```

`std::chrono` 的日历类型能表达有意义的日期，而不仅是时间戳差值。计算“多少天后”时要注意时区、夏令时和 `system_clock` 与 `steady_clock` 的不同用途：

- `system_clock` 对应墙上时间，可能被校时调整。
- `steady_clock` 单调前进，适合测量耗时。

数值和位工具：

```cpp
#include <bit>
#include <numeric>

constexpr int middle = std::midpoint(2, 5);
constexpr int width = std::bit_width(42u);
constexpr int ones = std::popcount(42u);
```

`std::midpoint` 用于避免 `(a + b) / 2` 的溢出。`bit_width` 和 `popcount` 只对有适当定义的无符号整数使用最清楚。

## 10. 其他常用 C++20 改进

- `std::jthread`：线程析构时自动请求停止并 join。
- `std::stop_token`：协作式取消。
- `std::atomic_ref`：对现有对象进行原子访问，需满足生命周期和别名约束。
- `std::latch`、`std::barrier`、`std::counting_semaphore`：同步原语。
- `std::ssize(range)`：以有符号类型获得大小。
- `std::midpoint`：避免整数中点溢出。
- 字符串 `starts_with` / `ends_with`。
- `std::erase` / `std::erase_if`。
- `std::construct_at`、`std::destroy_at`。

不要因为特性新就使用。先判断它是否改善正确性、可读性或可测量性能。

## 练习 1：Ranges 词频管线

给定 `std::vector<std::string>`：

1. 过滤掉空字符串。
2. 转为小写（可用 `transform`）。
3. 获取前 5 个结果。

不使用显式 `for` 完成转换，并写出惰性视图和具体化结果的版本。

## 练习 2：Concept 设计

定义 `Named`、`Sized`、`Numeric` 三个 Concept。组合成 `NamedSized`，并用 `static_assert` 验证：

- 一个同时有 `name()` 和 `size()` 的类型。
- 一个只有 `name()` 的类型不满足组合约束。

## 练习 3：格式化报告

实现 `format_row(name, count, price)`，输出对齐的固定宽度文本。输入：

```text
apple 3 5.5
```

要求：

- 名称左对齐宽度 10。
- 数量右对齐宽度 5。
- 金额固定两位小数。
- 用 `std::format`；若工具链不支持，记录替代方案。

## 自测

1. Concept 与运行时 `if` 有什么区别？
2. Ranges 视图是否拥有元素？
3. `std::span` 能防止底层数组销毁后继续访问吗？
4. `std::format` 相比 C 风格格式化有什么优势？
5. `consteval` 与 `constexpr` 的差异是什么？

## 下一步

[第 12 章](12_concurrency.md)讨论 C++20 并发和协作取消。
