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

## 7. 文件输入输出

标准库的文件流位于 `<fstream>`：

- `std::ifstream`：只读输入文件。
- `std::ofstream`：只写输出文件。
- `std::fstream`：同时读写，只有确实需要时才使用。

文件流同样采用 RAII。对象离开作用域时自动关闭文件，因此不需要在每条返回路径上手动调用 `close()`。

### 写入文本文件

```cpp
#include <fstream>
#include <string>

bool write_text(const std::string& path, const std::string& text) {
    std::ofstream output{path, std::ios::out | std::ios::trunc};
    if (!output) {
        return false; // 文件无法创建或打开
    }

    output << text;
    if (!output) {
        return false; // 写入过程中失败
    }

    output.close();
    return static_cast<bool>(output); // 检查关闭/flush 是否失败
}
```

`std::ios::out | std::ios::trunc` 表示“输出并清空已有内容”。`ofstream` 默认通常已经采用这个模式，但显式写出来更能说明会覆盖旧文件。

调用示例：

```cpp
#include <iostream>

int main() {
    if (!write_text("greeting.txt", "hello\nC++20\n")) {
        std::cerr << "cannot write greeting.txt\n";
        return 1;
    }
}
```

程序成功执行后，当前工作目录中会出现：

```text
hello
C++20
```

注意：**相对路径相对于程序运行时的当前工作目录，而不是源文件所在目录。** 从 `build/` 目录运行程序时，`greeting.txt` 通常也会出现在 `build/` 中。

### 追加写入

追加日志时使用 `std::ios::app`：

```cpp
bool append_line(const std::string& path, const std::string& line) {
    std::ofstream output{path, std::ios::out | std::ios::app};
    if (!output) {
        return false;
    }

    output << line << '\n';
    output.close();
    return static_cast<bool>(output);
}
```

`app` 会把每次写入放到文件末尾，不会像 `trunc` 那样清空旧内容。

模式速查：

| 模式 | 含义 | 常见用途 |
|---|---|---|
| `std::ios::in` | 打开用于读取 | `ifstream` 默认 |
| `std::ios::out` | 打开用于写入 | `ofstream` |
| `std::ios::trunc` | 清空已有内容 | 覆盖生成报告 |
| `std::ios::app` | 每次写到末尾 | 日志追加 |
| `std::ios::ate` | 打开后定位到末尾，但仍可移动位置 | 特殊随机读写 |
| `std::ios::binary` | 禁用文本换行翻译 | 二进制文件 |

模式可以组合，但不要把 `trunc` 和 `app` 混在一起猜测结果。明确业务是要“覆盖”还是“追加”。

### 逐行读取文本文件

```cpp
#include <fstream>
#include <iostream>
#include <string>

bool print_lines(const std::string& path) {
    std::ifstream input{path};
    if (!input) {
        return false; // 文件不存在、路径错误或没有权限
    }

    std::string line;
    while (std::getline(input, line)) {
        std::cout << line << '\n';
    }

    return input.eof() && !input.bad();
}
```

`while (std::getline(input, line))` 把“读取是否成功”直接放进循环条件。不要写成：

```cpp
// 错误：检查 eof 太早，可能多处理一次或处理空行
while (!input.eof()) {
    std::getline(input, line);
    // 此时没有立即检查 getline 是否成功
}
```

标准做法是读取后立刻用返回值判断，或把读取作为循环条件。

### 流状态：`good`、`fail`、`bad`、`eof`

文件流不会默认抛异常，而是记录状态：

| 状态 | 含义 |
|---|---|
| `good()` | 当前没有失败标志 |
| `fail()` | 打开失败、格式读取失败或操作失败 |
| `bad()` | 底层 I/O 严重错误 |
| `eof()` | 已尝试读到输入末尾 |

重要区别：

- 到达 EOF 不一定是错误。
- `fail()` 可能同时因 EOF 被设置。
- 读取格式错误后，流通常保持失败状态，后续读取会继续失败。
- 需要再次使用流时，先处理错误并调用 `clear()` 清除状态；`clear()` 不会修复文件或恢复丢失的数据。

检查文件能否正常处理，至少需要区分三种情况：

```text
文件打不开
读取中途发生 I/O 错误
正常到达文件末尾
```

### 写入时检查错误

```cpp
#include <fstream>
#include <string>
#include <vector>

bool save_records(const std::string& path,
                  const std::vector<std::string>& records) {
    std::ofstream output{path, std::ios::out | std::ios::trunc};
    if (!output) {
        return false;
    }

    for (const std::string& record : records) {
        output << record << '\n';
        if (!output) {
            return false;
        }
    }

    output.close();
    return static_cast<bool>(output);
}
```

写入失败可能在循环中途发生。发现失败后必须停止，不要把不完整文件当作成功结果。

### 文本模式与二进制模式

文本模式针对字符行设计；二进制模式禁止平台对换行等字节做文本转换：

```cpp
std::ifstream input{"data.bin", std::ios::in | std::ios::binary};
std::ofstream output{"copy.bin", std::ios::out | std::ios::binary | std::ios::trunc};
```

二进制读取必须检查实际读到的字节数：

```cpp
#include <array>
#include <cstddef>
#include <fstream>
#include <optional>

std::optional<std::array<std::byte, 4>>
read_four_bytes(const std::string& path) {
    std::ifstream input{path, std::ios::in | std::ios::binary};
    if (!input) {
        return std::nullopt;
    }

    std::array<std::byte, 4> buffer{};
    input.read(reinterpret_cast<char*>(buffer.data()),
               static_cast<std::streamsize>(buffer.size()));
    if (input.gcount() != static_cast<std::streamsize>(buffer.size())) {
        return std::nullopt;
    }
    return buffer;
}
```

`input.gcount()` 表示最近一次 `read` 实际读了多少字节。即使文件只有两个字节，也可能只设置 EOF，而不应先假设数组已完整填充。

二进制模式不等于“可以直接写结构体内存”。字段对齐、字节序、版本和长度仍是格式协议的一部分，详见[第 18 章](18_data_boundaries.md)。

### 文件路径和常见错误

- 打开后立即检查 `if (!input)`，不要假设文件一定存在。
- 输出时明确覆盖还是追加。
- 不要一边读取文件一边用 `trunc` 打开同一个文件。
- 日志常用 `'\n'`；`std::endl` 还会强制 flush，循环中频繁使用可能影响性能。
- 流对象析构会关闭文件，但若需要检测关闭失败，应显式 `close()` 后检查状态。
- 外部文件名可能包含编码和平台差异；需要路径操作时使用第 18 章的 `std::filesystem`。
- 不要在未验证结果前把写到一半的文件标记为成功。

### 完整文件处理流程

一个可靠的文件处理函数通常按以下顺序：

1. 打开文件并检查是否成功。
2. 逐行或逐块处理，并检查每次读取结果。
3. 对业务数据做范围验证。
4. 写入临时结果并检查每次写入。
5. 成功关闭后再提交或替换目标文件。
6. 失败时返回明确错误，不输出“成功”。

仓库中的
[`examples/file_io.cpp`](../examples/file_io.cpp)
演示了写入、关闭、重新读取、错误检查和临时文件清理。

## 8. `const` 与 `constexpr`

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

## 9. 类型的边界：`sizeof` 与对象表示

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

## 10. 第一套风格规则

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
