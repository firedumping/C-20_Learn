# 第 3 章：控制流、函数与调试

## 学习目标

- 用 `if`、`switch`、循环和提前返回表达清晰的分支。
- 知道何时使用范围 `for`，以及如何避免遍历期间修改容器。
- 设计输入、输出和错误路径清楚的小函数。
- 理解值传递、引用参数和返回值的基本成本。
- 能用断言、调用栈和最小复现定位逻辑错误。

## 1. 条件分支

```cpp
#include <iostream>

int main() {
    int score{};
    if (!(std::cin >> score)) {
        std::cerr << "请输入数字\n";
        return 1;
    }

    if (score < 0 || score > 100) {
        std::cerr << "分数超出范围\n";
        return 2;
    }

    if (score >= 90) {
        std::cout << "A\n";
    } else if (score >= 80) {
        std::cout << "B\n";
    } else if (score >= 70) {
        std::cout << "C\n";
    } else {
        std::cout << "D\n";
    }
}
```

条件应该先处理非法状态，再处理正常状态。这样读代码时不必在脑内维护“如果进入这里，说明前面都成立”。

不要在同一个 `if` 中塞入三四个副作用。把复杂条件提取成有名字的函数：

```cpp
bool can_publish(int score, bool submitted, bool banned) {
    return score >= 60 && submitted && !banned;
}
```

## 2. `switch` 与枚举

```cpp
enum class Command { add, remove, list };

void handle(Command command) {
    switch (command) {
    case Command::add:
        // add
        break;
    case Command::remove:
        // remove
        break;
    case Command::list:
        // list
        break;
    }
}
```

`enum class` 不会隐式转换成整数，也不会把不同枚举的名字混入同一作用域。它比无作用域 `enum` 更安全。

每个 `case` 都不要忘记终止。若确实需要贯穿，使用显式注释 `[[fallthrough]];`，让意图可见。

## 3. 循环与迭代

```cpp
#include <iostream>
#include <vector>

int main() {
    std::vector<int> values{3, 5, 8};

    for (int value : values) {
        std::cout << value << ' ';
    }
    std::cout << '\n';
}
```

只读遍历用 `const auto&`：

```cpp
for (const auto& value : values) {
    // 只读
}
```

修改元素用 `auto&`：

```cpp
for (auto& value : values) {
    value *= 2;
}
```

如果遍历过程中可能插入或删除元素，范围 `for` 的迭代器/引用可能失效。常见做法是收集要修改的键，循环结束后统一修改，或使用标准算法的 `erase_if`。

### `while` 与“读直到失败”

```cpp
#include <iostream>
#include <vector>

int main() {
    std::vector<int> values;
    int value{};
    while (std::cin >> value) {
        values.push_back(value);
    }
    std::cout << "count=" << values.size() << '\n';
}
```

这个循环能处理任意数量输入，并在 EOF 或格式错误时停止。若需要区分 EOF 与格式错误，应在循环后检查 `std::cin.eof()`。

## 4. 函数是抽象边界

好的函数有三个特征：

- 输入和输出明确。
- 有单一可说明的职责。
- 能独立测试。

```cpp
#include <optional>
#include <string>
#include <string_view>

std::optional<int> parse_positive_int(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    int value{};
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        value = value * 10 + (ch - '0');
    }

    if (value <= 0) {
        return std::nullopt;
    }
    return value;
}
```

这里返回 `std::optional<int>` 表达“成功时有一个正整数，失败时没有值”，比返回 `-1` 更清楚。注意上面没有检查整数上界，生产代码应使用成熟的解析函数并检查溢出。

## 5. 参数与返回值

小类型按值传：

```cpp
int twice(int value) {
    return value * 2;
}
```

大对象只读用 `const T&`（或适合时使用视图）：

```cpp
std::size_t count_words(const std::string& text);
```

需要修改调用者对象时用非 const 引用：

```cpp
void append_header(std::string& output);
```

参数语义比“引用快”更重要。`const T&` 表示函数借用对象且不拥有；`T&` 表示可能修改；`T` 表示函数获得自己的值。

返回值通常直接返回对象，让复制消除或移动完成优化：

```cpp
std::vector<int> make_values() {
    return {1, 2, 3};
}
```

不要返回指向局部变量的引用或指针。

## 6. 函数重载与默认参数

```cpp
#include <string>

std::string format_name(std::string first, std::string last);
std::string format_name(std::string first);

void log(std::string_view message, int level = 0);
```

重载依据参数类型和数量解析。不要仅凭返回类型区分函数；返回类型不参与重载决议。

默认参数只写在声明处，通常不在定义处重复。参数很多时，考虑配置结构体或 builder 模式，而不是给十个参数设默认值。

## 7. 错误路径和诊断

一个稳定的命令行程序至少要：

- 验证输入格式。
- 验证业务范围。
- 向 `std::cerr` 输出可操作信息。
- 返回不同错误码（如果调用者需要区分）。
- 不留下半完成输出文件或已部分修改的状态。

调试时，先把错误输入缩小到一个最小样例：

```text
输入：3 1 4 x 5
预期：读入 3 个数字后停止，或报告格式错误
实际：程序崩溃
```

然后用调试器查看 `std::cin` 状态、当前索引和容器边界。

## 8. 控制流的风格清单

- 优先提前返回，减少嵌套。
- 分支条件有业务名称。
- 使用 `const` 明确不变量。
- 循环的终止条件和索引边界写在注释旁边。
- 一个函数失败时不能只打印错误后继续用无效数据计算。
- 用 `std::optional` 表达可缺失结果；不要返回魔法值。
- 不要把输入解析、业务规则和输出格式全部塞进 `main`。

## 练习 1：FizzBuzz（有约束版）

读取正整数 `n`，输出 1 到 `n`：

- 3 的倍数为 `Fizz`。
- 5 的倍数为 `Buzz`。
- 同时为 15 的倍数为 `FizzBuzz`。
- 其余输出数字。

要求先验证输入，并实现一个可独立测试的函数：

```cpp
std::string fizzbuzz(int n);
```

## 练习 2：统计单词

读取多行文本直到 EOF，统计：

- 总行数。
- 非空行数。
- 以空白分隔的单词数。

建议把“按空白统计数量”抽成函数。先明确连续多个空格、制表符和空行的行为。

## 练习 3：质数判定

实现 `bool is_prime(int n)`：

- `n < 2` 返回 `false`。
- 只检查到 `i * i <= n`，但避免 `i * i` 溢出。
- 对 `2`、`3`、大整数和负数写出测试。

然后读取一个整数，输出 `prime` 或 `not prime`。

## 练习 4：二分查找的边界

实现 `std::optional<std::size_t> binary_search(const std::vector<int>&, int)`，要求对空数组、单元素数组、目标不存在和重复元素至少有明确策略。

写出每轮循环后的不变量，例如“目标若存在，一定在 `[left, right)`”。

## 自测

1. `for (const auto& item : values)` 和 `for (auto item : values)` 的语义差别是什么？
2. 什么时候不应该在遍历容器时插入/删除元素？
3. `std::optional` 适合表达什么样的失败？
4. 为什么返回局部变量的引用是错误？
5. 如何把“读取输入”和“业务规则”拆成可测试函数？

## 下一步

[第 4 章](04_lifetime_references_pointers.md)会解释为什么引用、指针和对象生命周期是后续 RAII 的基础。
