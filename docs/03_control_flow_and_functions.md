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

### 逻辑运算的短路求值

内置 `&&` 只有在左边为真时才计算右边；`||` 只有在左边为假时才计算右边：

```cpp
struct Session {
    bool active;
};

bool can_use(const Session* session) {
    return session != nullptr && session->active;
}
```

如果 `session` 是空指针，`session != nullptr` 为假，右侧的 `session->active` 不会执行。因此这里不会解引用空指针。

这个性质常用于“先检查前提，再执行需要该前提的操作”：

```cpp
if (index < values.size() && values[index] != 0) {
    // 左条件保证索引合法，右侧访问才执行
}
```

注意：

- 不要把 `&&` 写成按位 `&`；后者不会短路，两边都会求值。
- 短路依赖从左到右的条件顺序，不能随意交换两边。
- 不要在条件里塞入难以追踪的多个副作用。复杂逻辑应提取成有名字的函数。
- 普通函数调用的多个实参没有这种“左边失败就跳过右边”的无条件保证，除非语言规则明确指定顺序。

条件表达式也可以用三元运算符直接产生值：

```cpp
const int score = 83;
const char* result = score >= 60 ? "pass" : "fail";
```

当两种结果的类型能够形成共同类型时，它适合短小、对称的选择；多层三元嵌套通常不如 `if` 清晰。

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

### 值、引用与只读访问

只看不改时，使用 `const auto&`：

```cpp
#include <iostream>
#include <string>
#include <vector>

int main() {
    const std::vector<std::string> names{"Ada", "Linus", "Grace"};

    for (const auto& name : names) {
        std::cout << name << '\n';
    }
}
```

`const auto&` 表示“以引用方式观察元素，但不要通过它修改”。它避免了复制字符串，同时清楚表达只读意图。

确实要修改容器中的元素时，使用 `auto&`：

```cpp
#include <iostream>
#include <vector>

int main() {
    std::vector<int> values{1, 2, 3};

    for (auto& value : values) {
        value *= 10;
    }

    for (const int value : values) {
        std::cout << value << ' ';
    }
}
```

输出：

```text
10 20 30
```

如果写成 `for (auto value : values)`，每次得到的是副本；修改副本不会改变原容器。若元素是小整数，副本成本很低；若元素是 `std::string`，则可能产生不必要的复制。

### 为什么遍历时不能随意插入或删除

范围 `for` 的底层大致等价于先取得开始迭代器和结束迭代器，再逐个前进：

```cpp
for (auto first = values.begin(), last = values.end();
     first != last;
     ++first) {
    auto& value = *first;
    // 循环体
}
```

这里的 `first` 和 `last` 都是迭代器。它们不是永远有效的对象编号，而依赖容器当前的内存布局和内部结构。

例如对 `std::vector`，下面的逻辑是错误的：

```cpp
std::vector<int> values{1, 2, 3, 4};

for (int value : values) {
    if (value % 2 == 0) {
        values.push_back(value * 10);
    }
}
```

`push_back` 可能导致重新分配：旧缓冲区释放，所有元素搬到新缓冲区。此时范围 `for` 保存的迭代器可能悬空，继续前进是未定义行为。即使暂时没有重新分配，循环的结束迭代器也固定在循环开始时，新增元素是否应被处理并不明确。

直接在范围 `for` 中删除也有同样问题：

```cpp
for (int value : values) {
    if (value < 0) {
        // 无法从 value 直接取得要删除的迭代器；
        // 即使另找迭代器再 erase，也可能让当前循环迭代器失效。
    }
}
```

失效规则与容器和操作有关：

- `std::vector` 扩容会使所有元素引用和迭代器失效。
- `std::map` 插入通常不使已有元素迭代器失效，但删除当前元素仍会破坏当前遍历位置。
- `std::unordered_map` 重新哈希可能使迭代器失效。
- `std::array` 大小固定，不能插入或删除元素。

所以不要只记一句“都失效”，而要记住：**遍历期间修改容器前，先确认该容器、该操作和当前迭代器的失效规则。**

### 安全方案 1：只修改元素，不改变容器大小

改变元素值通常是安全的：

```cpp
#include <algorithm>
#include <vector>

std::vector<int> scores{105, 80, -5};

for (auto& score : scores) {
    score = std::clamp(score, 0, 100);
}
```

这不会插入、删除或重新分配元素。除非元素自身的操作又间接修改了容器，否则现有迭代器结构不受影响。

### 安全方案 2：用 `std::erase_if` 批量删除

C++20 为常见容器提供统一的 `std::erase_if(container, predicate)`：

```cpp
#include <iostream>
#include <vector>

int main() {
    std::vector<int> values{-3, 7, -1, 12, 0, -8};

    const auto removed = std::erase_if(values, [](int value) {
        return value < 0;
    });

    std::cout << "removed=" << removed << '\n';
    for (const int value : values) {
        std::cout << value << ' ';
    }
}
```

输出：

```text
removed=3
7 12 0
```

阅读顺序：

1. 传入容器本身，而不是开始/结束迭代器。
2. lambda 返回 `true` 的元素会被删除。
3. 返回值是删除元素的数量。
4. 函数返回后容器已经缩小，不需要再调用一次 `erase`。

`std::erase_if` 同样适用于关联容器：

```cpp
#include <iostream>
#include <map>
#include <string>

int main() {
    std::map<std::string, int> stock{
        {"apple", 0},
        {"pear", 4},
        {"kiwi", 0},
    };

    std::erase_if(stock, [](const auto& entry) {
        return entry.second == 0;
    });

    for (const auto& [name, count] : stock) {
        std::cout << name << '=' << count << '\n';
    }
}
```

输出：

```text
pear=4
```

这个接口表达的是“遍历整个容器并删除所有满足条件的元素”，比手写循环更短，也更不容易写错。它由各容器的头文件提供，例如上例必须包含 `<map>`，不能只包含 `<algorithm>`。

如果删除条件只是“值等于某个目标”，还有更直接的 `std::erase`：

```cpp
std::vector<int> values{7, 2, 7, 9};
const auto removed = std::erase(values, 7);
// removed == 2，values == {2, 9}
```

区分：

- `std::erase(container, value)`：删除所有等于 `value` 的元素。
- `std::erase_if(container, predicate)`：删除所有让谓词返回 `true` 的元素。
- 成员 `map.erase(key)`：按键删除，失败时返回 0，不删除读取时也不应意外插入新键。

### 安全方案 3：需要检查元素时用迭代器删除

如果删除前还需要执行其他逻辑，或者每次删除后要立即处理返回位置，可以显式控制迭代器：

```cpp
#include <iostream>
#include <vector>

int main() {
    std::vector<int> values{1, 2, 3, 4, 5, 6};

    for (auto it = values.begin(); it != values.end();) {
        if (*it % 2 == 0) {
            it = values.erase(it); // erase 返回新的当前位置
        } else {
            ++it;                  // 只有没删除时才前进
        }
    }

    for (const int value : values) {
        std::cout << value << ' ';
    }
}
```

输出：

```text
1 3 5
```

关键规则：

- 删除后使用 `erase` 返回的新迭代器。
- 删除时不要同时执行 `++it`，否则会跳过被搬到当前位置的元素。
- 对 `std::vector` 大量删除时，这种逐个 `erase` 可能反复搬移元素，复杂度可能达到 O(n²)；这种场景优先使用 `erase_if`。

### 安全方案 4：先收集键，结束遍历后再修改

如果删除条件依赖多个容器，或者需要先读取完整旧状态，可以先记录要删除的键：

```cpp
#include <iostream>
#include <map>
#include <string>
#include <vector>

int main() {
    std::map<std::string, int> stock{
        {"apple", 0},
        {"pear", 4},
        {"kiwi", 0},
    };

    std::vector<std::string> sold_out;
    for (const auto& [name, count] : stock) {
        if (count == 0) {
            sold_out.push_back(name);
        }
    }

    // 此时已经结束遍历，可以安全修改 stock。
    for (const std::string& name : sold_out) {
        stock.erase(name);
    }

    for (const auto& [name, count] : stock) {
        std::cout << name << '=' << count << '\n';
    }
}
```

这个方案适合“先分析完整容器，再统一提交修改”的场景。代价是需要额外保存键，并增加一次遍历。

### 需要插入元素时怎么办

不要一边范围遍历原容器一边向它插入。先收集新增元素，再一次性插入：

```cpp
#include <iostream>
#include <vector>

int main() {
    std::vector<int> values{1, 2, 3};
    std::vector<int> additions;

    for (const int value : values) {
        if (value % 2 != 0) {
            additions.push_back(value * 10);
        }
    }

    values.insert(values.end(), additions.begin(), additions.end());

    for (const int value : values) {
        std::cout << value << ' ';
    }
}
```

输出：

```text
1 2 3 10 30
```

这个写法保证遍历期间原容器大小和存储结构不变，新增元素也不会被本轮遍历意外处理。

### 选择哪种写法

| 需求 | 推荐写法 | 原因 |
|---|---|---|
| 修改已有元素 | `for (auto& value : values)` | 不改变容器大小 |
| 删除所有满足条件的元素 | `std::erase_if` | 表达直接，封装失效细节 |
| 删除前需要逐项处理 | `it = values.erase(it)` | 能控制删除后的下一位置 |
| 多容器联合决策 | 先收集键，再统一删除 | 遍历结论来自完整旧状态 |
| 增加元素 | 先收集，再 `insert` | 避免遍历中重分配 |

不要为了省一次临时容器，就把程序置于未定义行为中。先写正确、可读的版本，只有在性能测量证明临时存储是瓶颈时，才设计更复杂的增量算法。

本节的五种安全策略都放在了可运行示例
[`examples/container_mutation.cpp`](../examples/container_mutation.cpp) 中。构建后会执行断言，可以直接修改谓词和数据做实验。

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

### `break`、`continue` 与嵌套循环

`break` 立即结束当前所在的循环或 `switch` 分支；`continue` 立即跳到下一次迭代：

```cpp
#include <iostream>

int main() {
    int total = 0;

    for (int value : {3, -1, 8, 120, 2}) {
        if (value < 0) {
            continue; // 跳过负数
        }
        if (value > 100) {
            break; // 结束整个 for 循环
        }
        total += value;
    }

    std::cout << "total=" << total << '\n';
}
```

输出：

```text
total=11
```

执行过程：

1. `3` 累加，得到 3。
2. `-1` 触发 `continue`，本轮剩余语句跳过。
3. `8` 累加，得到 11。
4. `120` 触发 `break`，循环结束，后面的 `2` 不再处理。

`break` 只跳出**最内层**循环：

```cpp
for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
        if (row == 1 && column == 1) {
            break; // 只结束内层 column 循环，外层仍继续
        }
    }
}
```

如果需要一次退出两层循环，通常把搜索逻辑放进函数并直接 `return`，比增加多个布尔标志更清楚：

```cpp
bool contains_pair(int target) {
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            if (row * 3 + column == target) {
                return true;
            }
        }
    }
    return false;
}
```

### `do-while`：至少执行一次

普通 `while` 先判断条件，条件一开始为假时循环体一次也不执行；`do-while` 先执行循环体，再判断：

```cpp
#include <iostream>

int main() {
    int value = 0;
    do {
        std::cout << "run once\n";
        ++value;
    } while (value < 0);
}
```

输出：

```text
run once
```

`do-while` 适合“至少进行一次操作，再决定是否继续”的交互流程。普通计数和范围遍历不要为了少写一行而使用它。

### 循环选择表

| 需求 | 推荐语句 |
|---|---|
| 已知次数或标准计数方式 | `for` |
| 先判断条件再执行 | `while` |
| 至少执行一次再判断 | `do-while` |
| 逐个访问范围元素 | 范围 `for` |
| 读到 EOF 或解析失败 | `while (std::cin >> value)` |

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

### 递归函数

递归函数直接或间接调用自己。写递归必须同时具备：

1. **基本情况**：不继续递归，直接返回结果。
2. **递归步骤**：让问题规模严格变小。
3. **终止证明**：参数变化最终一定到达基本情况。

```cpp
#include <iostream>

unsigned long long factorial(unsigned int value) {
    if (value <= 1U) {
        return 1ULL; // 基本情况
    }
    return static_cast<unsigned long long>(value) *
           factorial(value - 1U); // 参数变小
}

int main() {
    std::cout << factorial(5) << '\n';
}
```

输出：

```text
120
```

调用过程：

```text
factorial(5)
-> 5 * factorial(4)
-> 5 * 4 * factorial(3)
-> ...
-> 5 * 4 * 3 * 2 * factorial(1)
-> 120
```

递归不是自动优化。每一层调用通常需要保存参数、局部变量和返回位置，调用太深可能耗尽栈空间。尾递归也不在 C++ 标准中保证被优化。能直接写成清晰循环，或输入规模可能非常大时，应优先考虑迭代版本。

递归中不要把 `value--` 和 `value - 1` 混淆：

```cpp
// factorial(value--); // 先把同一个旧值传给自己，递归没有缩小
factorial(value - 1U);  // 传入更小的副本
```

递归适合树、图、分治等天然分层的问题；对于“从 1 加到 n”这类线性问题，循环通常更直接。

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

## 练习 5：批量清理容器

完成自动检查练习 `cleanup_scores(std::vector<int>)`：

- 保留 `[0, 100]` 中的所有分数。
- 删除负数和大于 100 的分数。
- 使用 `std::erase_if`。
- 检查空容器、全部删除、全部保留和边界值 `0`、`100`。

再写一个 `std::map<std::string, int>` 版本，删除库存为 0 的商品。分别比较 `erase_if` 和“先收集键再删除”的可读性与额外成本。

## 自测

1. `for (const auto& item : values)` 和 `for (auto item : values)` 的语义差别是什么？
2. 范围 `for` 为什么不能安全地直接向 `std::vector` 插入元素？
3. `std::optional` 适合表达什么样的失败？
4. 为什么返回局部变量的引用是错误？
5. 如何把“读取输入”和“业务规则”拆成可测试函数？
6. `std::erase_if`、`remove_if` 和 `it = erase(it)` 各解决什么问题？
7. 为什么在迭代器删除循环中，删除分支不能再次执行 `++it`？
8. 递归函数必须具备哪两个部分，为什么尾递归仍可能耗尽栈空间？

## 下一步

[第 4 章](04_lifetime_references_pointers.md)会解释为什么引用、指针和对象生命周期是后续 RAII 的基础。
