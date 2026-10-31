# 第 7 章：STL 容器、迭代器、算法与 lambda

## 学习目标

- 根据访问模式、复杂度和失效规则选择容器。
- 理解迭代器和范围的基本接口。
- 用标准算法替代手写循环。
- 掌握 lambda 捕获、`mutable` 和生命周期风险。
- 正确使用 `find`、`sort`、`transform`、`accumulate`、`erase_if` 等工具。

## 1. 容器选择表

| 容器 | 典型用途 | 随机访问 | 中间插入/删除 | 失效特征 |
|---|---|---:|---:|---|
| `std::vector` | 默认连续序列 | O(1) | O(n) | 扩容使全部引用/迭代器失效 |
| `std::array` | 编译期固定大小 | O(1) | 不支持 | 大小固定 |
| `std::deque` | 两端高效插入 | O(1) | 中间 O(n) | 规则比 vector 复杂 |
| `std::list` | 已知位置频繁插入 | 不支持 | O(1) 链接操作 | 未删除元素通常保持有效 |
| `std::map` | 有序键值，树结构 | 不支持 | O(log n) | 未删除元素通常稳定 |
| `std::unordered_map` | 平均查找 O(1) | 不支持 | 平均 O(1) | rehash 可能使迭代器失效 |
| `std::set` | 有序唯一键 | 不支持 | O(log n) | 未删除元素通常稳定 |

默认先考虑 `std::vector`。它缓存友好、接口简单、遍历快。只有测量或明确需求才选择树、哈希容器或链表。

## 2. `std::vector` 的基本操作

```cpp
#include <vector>

std::vector<int> values{3, 1, 4};
values.push_back(1);
values.emplace_back(5);

for (const int value : values) {
    // 读
}

values.pop_back();
```

`emplace_back` 在原位置构造对象，可能避免临时对象；它不是在所有情况下都优于 `push_back`。对于简单类型，两者成本往往没有区别。

不要用 `operator[]` 越界。需要边界检查时用 `.at()`，测试或外部输入边界尤其有用。

### `reserve` 不是万能加速器

```cpp
std::vector<int> values;
values.reserve(1000);
```

如果事先知道元素数量，`reserve` 可避免多次扩容。对不必要的大容量调用 `reserve` 反而浪费内存。

## 3. 迭代器与范围

```cpp
#include <algorithm>
#include <vector>

std::vector<int> values{1, 2, 3};

auto it = std::find(values.begin(), values.end(), 2);
if (it != values.end()) {
    *it = 20;
}
```

迭代器像广义指针，提供 `*`、`++` 和比较操作。`begin()` 指向首元素，`end()` 指向“尾后”，不能解引用。

大多数算法接受 `[first, last)`：

- 包含 `first`。
- 不包含 `last`。
- 空范围满足 `first == last`。

## 4. 标准算法优先

### 统计与转换

```cpp
#include <algorithm>
#include <numeric>
#include <vector>

std::vector<int> values{1, 2, 3, 4, 5};

const auto even_count = std::count_if(
    values.begin(), values.end(),
    [](int value) { return value % 2 == 0; });

const int sum = std::accumulate(values.begin(), values.end(), 0);
```

### 原地变换

```cpp
#include <algorithm>

std::ranges::transform(values, values.begin(),
                       [](int value) { return value * 2; });
```

当输入输出范围重叠时，需要确认算法是否允许，以及你的编译器是否支持对应范围版本。不确定时先用临时容器保存结果。

### 排序

```cpp
#include <algorithm>

std::ranges::sort(values);
```

排序要求元素满足严格弱序。比较器 `less(a,b)` 和 `less(b,a)` 不应同时为真。

## 5. lambda：局部的可调用对象

```cpp
auto add = [](int a, int b) {
    return a + b;
};

int threshold = 10;
auto above = [threshold](int value) {
    return value > threshold;
};
```

捕获方式：

```cpp
[=]    // 按值捕获用到的自动变量（容易隐式捕获）
[&]    // 按引用捕获（生命周期风险高）
[x]    // 明确按值捕获 x
[&x]   // 明确按引用捕获 x
[this] // 捕获当前对象
```

项目规范常要求显式捕获，因为它让捕获成本与生命周期更容易检查。

### 引用的生命周期

```cpp
auto make_bad() {
    int value = 42;
    return [&value] { return value; }; // 返回后 value 不存在
}
```

把引用捕获的 lambda 存到回调队列或异步任务时，必须保证引用对象活得足够久。需要延长时间时按值捕获，或者使用明确的共享所有权。

### `mutable`

```cpp
int calls = 0;
auto counter = [calls]() mutable {
    return ++calls;
};
```

这里的 `calls` 是 lambda 对象中的副本，`mutable` 允许修改副本；原始变量不变。

## 6. 遍历并删除

完整原理和多种容器示例见[第 3 章](03_control_flow_and_functions.md#为什么遍历时不能随意插入或删除)，这里重点区分几个容易混淆的算法。

### 错误方式：删除后继续使用当前迭代器

```cpp
for (auto it = values.begin(); it != values.end(); ++it) {
    if (*it < 0) {
        values.erase(it); // it 失效，下一步 ++it 错
    }
}
```

`erase(it)` 会使 `it` 失效。循环表达式随后又执行 `++it`，就是在操作失效迭代器。

### `std::erase_if`：删除并缩小容器

```cpp
#include <iostream>
#include <vector>

int main() {
    std::vector<int> values{3, -1, 7, -5, 2};

    const auto removed = std::erase_if(values, [](int value) {
        return value < 0;
    });

    std::cout << "removed=" << removed << '\n';
}
```

输出：

```text
removed=2
```

对容器使用 `erase_if` 时，调用后容器本身已经缩小。`std::vector`、`std::map`、`std::set`、`std::list` 和对应无序容器在 C++20 都提供该类接口，但别忘记包含各自容器头文件。

### `std::remove_if`：只搬移，不删除

传统算法 `std::remove_if` 返回“新逻辑结尾”，但容器大小不会改变：

```cpp
#include <algorithm>
#include <iostream>
#include <vector>

int main() {
    std::vector<int> values{3, -1, 7, -5, 2};

    const auto new_end = std::remove_if(
        values.begin(), values.end(),
        [](int value) { return value < 0; });

    std::cout << "size before erase=" << values.size() << '\n';
    values.erase(new_end, values.end());
    std::cout << "size after erase=" << values.size() << '\n';
}
```

输出：

```text
size before erase=5
size after erase=3
```

`remove_if` 之后，`[new_end, values.end())` 中仍可能保存已移动元素留下的值，但状态未指定，不能依赖这些值还原被删除对象。因此它不能单独完成删除，必须配合容器的 `erase`，这就是旧式的 remove-erase idiom。

### 手动迭代器删除

如果删除前还要执行额外操作，使用 `erase` 的返回迭代器：

```cpp
for (auto it = values.begin(); it != values.end();) {
    if (*it < 0) {
        it = values.erase(it);
    } else {
        ++it;
    }
}
```

这段代码是自包含的：删除时不执行额外的 `++it`。它适合逻辑较复杂、不能直接表达成单一谓词的场合。

### 把元素移动到另一个容器

如果需要保留被删除的元素，而不是销毁它们，不应使用 `erase_if`：

```cpp
std::vector<int> kept;
std::vector<int> removed;

for (const int value : values) {
    if (value < 0) {
        removed.push_back(value);
    } else {
        kept.push_back(value);
    }
}
```

这段代码没有边遍历边修改原容器，行为最容易推理。数据量较大时，再考虑 `std::partition` 或先分区后移动等优化。

## 7. 关联容器

```cpp
#include <map>
#include <string>

std::map<std::string, int> counts;
for (const std::string& word : words) {
    ++counts[word];
}
```

`operator[]` 在键不存在时会插入默认值。这是计数方便的地方，也是排查意外插入时的陷阱。只想查询时用：

```cpp
auto it = counts.find(word);
if (it != counts.end()) {
    int value = it->second;
}
```

或：

```cpp
if (counts.contains(word)) {
    // C++20
}
```

`unordered_map` 的平均复杂度依赖哈希质量。键的哈希函数必须和相等比较一致：两个相等的键必须有相同哈希。

## 8. 视图接口与字面量

C++17 引入类模板实参推导，C++20 继续增强范围接口：

```cpp
#include <map>
#include <string>
#include <string_view>

std::map<std::string, int, std::less<>> counts;
// 可用 std::string_view 查找（依赖异构查找配置）
```

接口设计时可以使用 `std::string_view` 只读查看文本，但返回所有权时返回 `std::string`。不要为了“少一次复制”让 API 的合同变模糊。

## 9. 算法组合示例

统计每个单词出现次数，再按次数降序、单词升序输出：

```cpp
#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <string_view>
#include <vector>

std::vector<std::pair<std::string, int>>
word_frequencies(const std::vector<std::string>& words) {
    std::map<std::string, int> counts;
    for (std::string_view word : words) {
        std::string normalized;
        normalized.reserve(word.size());
        for (char ch : word) {
            normalized.push_back(
                static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
        ++counts[normalized];
    }

    std::vector<std::pair<std::string, int>> result(
        counts.begin(), counts.end());

    std::ranges::sort(result, [](const auto& lhs, const auto& rhs) {
        if (lhs.second != rhs.second) {
            return lhs.second > rhs.second;
        }
        return lhs.first < rhs.first;
    });
    return result;
}
```

这里的比较器是严格弱序：频率不同时降序；频率相同则按单词升序。

## 练习 1：词频统计

输入任意多行文本，按 ASCII 大小写不敏感统计单词频率，输出频率最高的 10 个词。

要求：

- 标点如何切分要明确。
- 用 `std::string_view` 或 `std::string` 的合理接口。
- 编写空输入、无字母输入和相同频率排序的样例。

## 练习 2：容器选择

为以下需求选择容器并解释：

1. 需要频繁按索引读取 10,000 个测量值。
2. 不断从头部和尾部取出任务。
3. 需要按键有序遍历配置。
4. 大部分操作是按唯一字符串 ID 查找，无需排序。
5. 元素数量固定为 16，且每个元素独立构造。

## 练习 3：用算法重写循环

给定 `std::vector<int>`，统计正数个数、计算平方和、移除所有负数、把结果按升序排序。尽量使用算法和 lambda，并保证输入范围与输出范围不产生不必要冲突。

## 自测

1. 为什么 `end()` 不能解引用？
2. `std::vector` 扩容会使哪些对象失效？
3. `map::operator[]` 和 `map::find` 的行为差别是什么？
4. lambda 按引用捕获后，什么时候会悬空？
5. 比较器为什么必须满足严格弱序？

## 下一步

[第 8 章](08_templates_and_deduction.md)会解释 `auto`、模板推导和 Concepts 之前的泛型基础。
