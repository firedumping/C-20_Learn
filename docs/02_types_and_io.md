# 第 2 章：类型、表达式与输入输出

## 学习目标

- 理解“对象 = 存储区域 + 类型 + 值/状态”这一模型。
- 熟练使用整数、浮点数、字符、布尔值、`std::string` 和 `std::string_view`。
- 识别隐式转换、窄化转换和整数溢出风险。
- 正确设计输入解析与格式化输出。
- 了解 `auto`、`const`、引用和 `constexpr` 在第一阶段的作用。

## 1. 类型是行为合同

类型决定三件事：

1. 对象在内存中的表示和大小。
2. 哪些操作合法。
3. 运算如何解释这些位。

```cpp
#include <iostream>

int main() {
    int count = 3;
    double price = 12.5;
    char grade = 'A';
    bool available = true;

    std::cout << count << '\n'
              << price << '\n'
              << grade << '\n'
              << std::boolalpha << available << '\n';
}
```

`int` 的大小通常为 4 字节，但标准只规定至少 16 位。需要固定宽度时使用 `<cstdint>`：

```cpp
#include <cstdint>

std::int32_t signed_value = -7;
std::uint64_t unsigned_value = 42;
std::size_t object_count = 100;  // 大小和索引常用类型
```

固定宽度类型用于文件格式、网络协议等需要精确位宽的场合。容器大小和内存偏移一般用 `std::size_t`。

## 2. 初始化：优先使用花括号

```cpp
int a = 42;
int b(42);
int c{42};
```

现代 C++ 默认优先：

```cpp
int count{};
double total{0.0};
std::string name{};
```

花括号初始化能阻止一部分窄化：

```cpp
double pi = 3.14159;
// int truncated{pi};  // 编译错误：double 到 int 是窄化
int truncated = pi;    // 允许，但丢失小数部分
```

注意：`int value{3.14}` 被拒绝，但浮点到浮点的可表示范围检查并非在所有场景都像你预期那样严格。仍要主动选择类型。

## 3. 整数运算与溢出

```cpp
#include <iostream>
#include <limits>

int main() {
    int maximum = std::numeric_limits<int>::max();
    std::cout << "max int = " << maximum << '\n';

    // maximum + 1 是未定义行为，绝不能依赖回绕结果。
}
```

有符号整数溢出是未定义行为。无符号整数运算按模 `2^N` 回绕，但把负数转成无符号仍可能产生意外。

安全地检查加法：

```cpp
#include <limits>

bool checked_add(int a, int b, int& result) {
    if ((b > 0 && a > std::numeric_limits<int>::max() - b) ||
        (b < 0 && a < std::numeric_limits<int>::min() - b)) {
        return false;
    }
    result = a + b;
    return true;
}
```

生产代码可以使用 `std::midpoint` 做中点计算，或第三方安全整数库；关键是不要在溢出后再检查结果。

### 整数和浮点数不要混着猜

```cpp
int x = 7;
int y = 2;
double a = x / y;        // 3.0，除法已完成
double b = static_cast<double>(x) / y;  // 3.5
```

需要转换时用 `static_cast`。C 风格 `(double)x` 不够显眼，也不容易搜索。

## 4. 浮点数的正确期待

浮点数不能精确表示所有十进制小数：

```cpp
#include <cmath>
#include <iostream>

int main() {
    double x = 0.1 + 0.2;
    std::cout << x << '\n';              // 常见输出 0.3
    std::cout << (x == 0.3) << '\n';     // 通常为 false

    const bool close = std::abs(x - 0.3) < 1e-12;
    std::cout << close << '\n';
}
```

比较浮点数应使用适合业务量级的容差，或者使用 `std::nextafter`、专门的 ULP 比较工具。对于金额，优先使用最小货币单位整数（如“分”）或精确十进制类型，而不是 `double`。

## 5. 字符、字符串与文本

```cpp
char letter = 'A';
const char* literal = "hello";
std::string owned = "hello";

owned += " world";
```

- `char` 是单个字符值。
- `"hello"` 是静态存储期字符数组，末尾有 `'\0'`。
- `std::string` 拥有字符缓冲区，能增长、复制和自动释放。
- `std::string_view` 只观察已有字符序列，不拥有内存。

字符串视图常用于只读函数参数：

```cpp
#include <string_view>

bool is_greeting(std::string_view text) {
    return text.starts_with("hello");
}
```

危险在于生命周期：

```cpp
std::string_view bad_view;
{
    std::string temporary = "temporary";
    bad_view = temporary;
} // temporary 被销毁
// bad_view 现在悬空，读取它是未定义行为
```

如果函数需要保存文本，保存 `std::string`；如果只是在调用期间读取，`std::string_view` 可以避免拷贝。

## 6. 输入输出不是数据库

`std::cin >> value` 成功时返回流；失败会设置 failbit，并可能让后续读取全部失败。必须检查结果：

```cpp
#include <iostream>

int main() {
    int age{};
    if (!(std::cin >> age)) {
        std::cerr << "请输入整数\n";
        return 1;
    }
    if (age < 0) {
        std::cerr << "年龄不能为负数\n";
        return 2;
    }
    std::cout << "age=" << age << '\n';
}
```

如果用户输入 `abc`，程序不应继续用未初始化的 `age` 计算。清理坏输入并重新读取是交互式程序的需求，不是标准库自动行为。

读取一整行：

```cpp
#include <iostream>
#include <string>

std::string line;
if (std::getline(std::cin, line)) {
    std::cout << "[" << line << "]\n";
}
```

混用 `>>` 和 `getline` 时，`>>` 会留下换行符。下一次 `getline` 可能立即读到空行。学习阶段先分开读取模式，或在确有必要时用 `std::ws` 丢弃前导空白。

## 7. `const` 与 `constexpr`

```cpp
const double tax_rate = 0.13;       // 运行期不可改
constexpr int square_size = 8;      // 编译期常量

constexpr int square(int x) {
    return x * x;
}

static_assert(square(5) == 25);
```

规律：

- 局部不变值用 `const`。
- 需要编译期参与数组大小、模板参数或 `static_assert` 时用 `constexpr`。
- 能加 `const` 的函数参数/成员函数通常更容易推理。
- 不要为了“显得现代”把普通局部变量都写成 `auto`；明确的 `int count{}` 更有信息量。

## 8. 类型的边界：`sizeof` 与对象表示

```cpp
#include <iostream>

struct Point {
    int x;
    int y;
};

int main() {
    std::cout << sizeof(Point) << '\n';
    std::cout << sizeof(int) << '\n';
}
```

不要假设结构体大小等于成员大小之和。编译器可能为对齐插入填充字节。文件格式和网络协议需要显式序列化，不能直接把任意对象内存写进文件。

## 9. 第一套风格规则

- 变量声明时初始化：`int count{};`。
- 使用 `std::size_t` 或容器的 `size_type` 表示大小，避免负数转无符号。
- 用 `static_cast<T>` 做有意的类型转换。
- 参数是否应可空、是否拥有资源，决定用值、引用、`std::string_view` 还是智能指针。
- 名字表达领域含义：`remaining_attempts` 比 `n` 好，`total_price_cents` 比 `price` 更明确。
- 先写错误路径，再写正常路径；错误路径通常更容易被忽略。

## 练习 1：温度转换器

读取摄氏温度 `double`，输出华氏温度和开尔文温度，各保留两位小数（暂时可用 `<iomanip>` 实现）。

要求：

- 输入失败时报错并返回非零。
- 摄氏温度低于绝对零度（约 -273.15）时拒绝。
- 用 `constexpr` 定义转换常量。

公式：

```text
F = C * 9 / 5 + 32
K = C + 273.15
```

## 练习 2：安全求和

实现 `bool safe_add(int, int, int&)`。对 `INT_MAX + 1`、`INT_MIN - 1` 返回 `false`，其余情况返回 `true` 并写入结果。不能先执行溢出加法再判断。

## 练习 3：文本边界

编写 `bool is_valid_username(std::string_view)`：

- 长度 3 到 16。
- 只能包含 ASCII 字母、数字和下划线。
- 不能以数字开头。

思考：`std::string_view` 能否避免临时字符串的生命周期问题？调用者在什么情况下可能传入悬空视图？

## 自测

1. `int x = 7 / 2;` 的结果是多少？为什么？
2. 为什么 `std::string_view` 不能在所有场合替代 `std::string`？
3. 有符号整数溢出与无符号整数回绕有何区别？
4. 为什么 `sizeof(Struct)` 可能大于成员大小之和？
5. `const` 和 `constexpr` 的区别是什么？

## 下一步

在[第 3 章](03_control_flow_and_functions.md)中，用控制流和函数把小型脚本组织成可测试的模块。
