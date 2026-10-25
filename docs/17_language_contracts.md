# 第 17 章：链接、对象模型与接口合同

## 学习目标

本章补齐从“单文件练习”进入真实项目时最容易断裂的知识：

- 能解释翻译单元、链接属性、定义与 ODR。
- 能正确设计多文件库接口和头文件。
- 能判断复制消除、NRVO、移动和按值返回的成本。
- 能选择安全的类型转换，避免窄化、悬空和未定义行为。
- 能解释对象表示、对齐、填充与跨平台二进制边界。
- 能用不变量和接口合同审查一个类型是否真的安全。

本章不是冷门语法清单。每个主题都直接对应真实缺陷：重复定义、随机链接失败、隐式转换导致的错误单位、Debug/Release 行为不同、把结构体内存直接写入文件后在另一台机器无法读取。

## 1. 翻译单元与链接属性

一个源文件经过预处理后形成一个翻译单元。编译每个翻译单元时，编译器只看到它包含的头文件；链接器最后负责把符号连接起来。

```cpp
// math.hpp
#pragma once

int add(int lhs, int rhs);          // 声明，不是定义

// math.cpp
#include "math.hpp"

int add(int lhs, int rhs) {         // 外部链接定义
    return lhs + rhs;
}

// main.cpp
#include "math.hpp"

int main() {
    return add(20, 22) == 42 ? 0 : 1;
}
```

常见链接属性：

| 属性 | 含义 | 典型例子 |
|---|---|---|
| 外部链接 | 整个程序使用同一个实体 | 普通命名空间函数 |
| 内部链接 | 每个翻译单元有独立实体 | `static` 函数、匿名命名空间 |
| 无链接 | 名字只在局部作用域有效 | 局部变量 |

匿名的辅助函数适合只在单个 `.cpp` 内使用：

```cpp
namespace {

int implementation_detail(int value) {
    return value + 1;
}

} // namespace
```

它减少符号污染，也让优化器更容易知道调用范围。不要为了“国际化”把只在一个源文件使用的函数导出到公共头文件。

## 2. ODR 与头文件

ODR 的关键句是：一个非 `inline` 函数或变量在整个程序中只能有一个定义。头文件会被多个源文件复制进各自翻译单元，因此：

```cpp
// bad.hpp：多个包含者会得到多个定义
int helper() {
    return 42;
}
```

正确做法是声明放在头文件、定义放在一个 `.cpp`：

```cpp
// helper.hpp
#pragma once
int helper();

// helper.cpp
#include "helper.hpp"
int helper() {
    return 42;
}
```

确实需要在头文件里定义时，使用 `inline`：

```cpp
inline int helper() {
    return 42;
}

inline int global_counter = 0;
```

`inline` 在这里确保多个翻译单元中相同的定义可以合法合并。它不是“让函数更快”的强制命令。类内成员函数、函数模板和许多 `constexpr` 函数天然具有可在头文件定义的属性。

### 头文件卫生

- 每个头文件应能独立编译。
- 直接包含用到的标准头，不依赖别人的包含顺序。
- 使用包含守卫或团队约定的 `#pragma once`。
- 不在头文件定义普通全局对象。
- 不在公共头文件展开大型实现细节。
- 不把 `using namespace std;` 放在头文件全局作用域。

测试头文件自包含性时，可以创建只包含它的 `.cpp` 文件：

```cpp
#include "project/public_api.hpp"
```

如果它不能独立编译，头文件就有隐藏依赖。

## 3. 静态初始化与 `constinit`

命名空间作用域对象可能经历动态初始化。多个翻译单元之间的初始化顺序没有统一保证，因此：

```cpp
// 另一个文件的对象可能在 logger 构造前调用它
Logger logger;
```

若对象要求在程序启动前完成编译期初始化，可以使用 `constinit`：

```cpp
constinit int startup_value = 42;
```

`constinit` 保证静态初始化，但不保证对象之后不可变；不可变仍由 `const` / `constexpr` 表达。不要依赖跨翻译单元的可变全局初始化顺序。更稳妥的设计通常是显式初始化或依赖注入。

## 4. 按值返回、复制消除与 NRVO

C++17 起，同类型纯右值返回保证复制消除：

```cpp
std::string make_text() {
    return std::string{"hello"};
}
```

有名字局部变量的 NRVO 通常发生，但标准不保证：

```cpp
std::string make_greeting() {
    std::string result = "hello";
    return result; // 允许 NRVO；否则按隐式移动规则处理
}
```

不要写：

```cpp
return std::move(result);
```

显式移动可能阻止 NRVO。即使没有 NRVO，直接返回局部变量通常也会按右值选择移动构造。按值返回是清晰的所有权模型，不应预先改成输出参数或裸指针。

### 返回引用

返回引用只适合返回生命周期清楚的外部对象：

```cpp
const std::string& first_name(const User& user) {
    return user.name; // 对象由调用者拥有
}
```

不能返回局部对象引用，也不能返回由临时容器拥有的元素引用。调用者必须知道返回引用依赖哪个输入对象存活。

## 5. 转换、窄化与 `explicit`

隐式转换会缩短接口，也会隐藏错误：

```cpp
struct Meters {
    explicit Meters(double value) : value{value} {}
    double value;
};

void move(Meters distance);

// move(3.0);           // 拒绝隐式转换
move(Meters{3.0});      // 单位在调用点可见
```

单参数构造函数通常默认 `explicit`，除非转换本身就是类型的自然语义。转换运算符也常需要 `explicit`：

```cpp
class Flag {
public:
    explicit operator bool() const noexcept;
};
```

### 四种显式转换

- `static_cast<T>`：已知相关的值转换、算术转换、去掉 `const` 以外的显式转换；仍需自己确认语义。
- `dynamic_cast<T>`：多态类型的运行时检查，失败时对指针返回空、对引用抛异常。
- `const_cast<T>`：只改变 `const` / `volatile` 限定。仅当原对象本来不是 const 且通过无 const 路径修改时才可能合法。
- `reinterpret_cast<T>`：重新解释位模式，通常与平台和严格别名规则相关，不应作为序列化手段。

C 风格转换 `(T)value` 不容易搜索，也可能悄悄选择错误种类。项目中统一使用有名字的转换。

### 整数转换

```cpp
std::size_t count = values.size();
// int truncated = count;   // 可能窄化或改变符号
```

从有符号到无符号或从宽到窄都应有明确边界检查：

```cpp
std::optional<int> to_int(std::size_t value) {
    if (value > static_cast<std::size_t>(
                    std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    return static_cast<int>(value);
}
```

不要只因为类型能转换就认为值一定在目标范围内。

## 6. 对象表示、对齐与填充

结构体大小不一定等于字段大小之和：

```cpp
struct Packet {
    char tag;
    int value;
};

static_assert(sizeof(Packet) >= sizeof(char) + sizeof(int));
static_assert(alignof(Packet) >= alignof(int));
```

编译器可在 `tag` 后插入填充字节。直接 `write(reinterpret_cast<const char*>(&packet), sizeof packet)` 会把填充、字节序和 ABI 细节写进文件，换编译器或平台后可能失效。

### `std::bit_cast`

需要在同大小、可平凡复制的类型之间复制位模式时，可以使用：

```cpp
#include <bit>
#include <cstdint>

static_assert(sizeof(float) == sizeof(std::uint32_t));
std::uint32_t bits = std::bit_cast<std::uint32_t>(3.5f);
```

`std::bit_cast` 比 `memcpy` 表达得更直接，但它不定义字节序，也不是通用序列化格式。

### 对齐

```cpp
struct alignas(64) CacheLineAligned {
    int value;
};
```

过度对齐能避免伪共享，也会增加内存占用并影响分配器要求。只在测量到并发缓存行竞争时使用。

不要假设一个对象的全部字节都能安全读写。对于非平凡类型，复制对象应使用其复制/移动语义，而不是按字节复制生命周期。

## 7. ABI 与异常边界

C++ 标准没有规定不同编译器、编译器版本或标准库版本之间的二进制 ABI。一个库若要在二进制层面稳定发布，需要明确：

- 公共符号如何导出。
- 哪些标准库类型可以跨 DLL/共享库边界。
- 异常能否跨越边界。
- 调用约定和对齐要求。
- 是否使用 PImpl 隐藏实现。

对于稳定 ABI 的库，常见方案是暴露不透明句柄或接口，把标准库容器和内部类型藏在实现中。头文件/源码级兼容与二进制级兼容是不同目标。

异常跨越 `noexcept` 函数边界会触发 `std::terminate`。公共接口应明确：

- 是否允许抛出。
- 抛出哪些可恢复错误。
- 资源在异常路径是否释放。
- 跨库边界是否应转换为错误码。

## 8. 接口合同审查

一个接口至少回答：

1. 输入是否可为空？
2. 对象由谁拥有，活多久？
3. 函数失败时原状态是否改变？
4. 是否可能抛出，调用者需要处理什么？
5. 复杂度和分配行为是否合理？
6. 调用方能否在编译期发现单位、空值和错误用法？

好接口让错误更难写出来：

```cpp
// 语义模糊
void configure(bool, int, int);

// 调用点可读
struct RetryPolicy {
    bool enabled;
    int max_attempts;
    int delay_ms;
};

void configure(RetryPolicy policy);
```

## 练习 1：多翻译单元

把以下接口拆成 `include/` 与 `src/`：

```cpp
struct Score {
    int correct;
    int total;
};

double percentage(Score score);
```

要求：

- 头文件可独立包含。
- 实现只在一个 `.cpp` 中。
- 增加第二个调用 `.cpp`，证明不会重复定义。
- 故意把普通定义放回头文件，记录链接错误，再恢复。

## 练习 2：观察复制消除

实现带复制/移动计数器的类型，分别测试：

1. `return Type{}`。
2. `return local;`。
3. `return std::move(local);`。

在 Debug 和 Release 下比较结果。结论必须写成“哪些行为由标准保证，哪些只是当前编译器实现”。

## 练习 3：安全接口

设计一个处理文件偏移量的接口：

- 外部输入可能超过 `size_t` 或 `long`。
- 返回错误原因而不是静默截断。
- 接口明确接收路径、偏移和长度。
- 偏移加长度不得溢出。

写出至少 8 个边界测试。

## 自测

1. 为什么头文件中的普通函数定义可能导致链接错误？
2. `inline` 的 ODR 含义与性能提示有什么区别？
3. 哪些复制消除由 C++17 标准保证？
4. 为什么直接把结构体对象表示写入文件不可移植？
5. `const_cast` 什么时候仍然会触发未定义行为？
6. 公共接口为什么要说明所有权、异常和失败后状态？

## 下一步

[第 18 章](18_data_boundaries.md)把接口合同应用到文件、文本编码和二进制数据。
