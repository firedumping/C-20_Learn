# 第 10 章：异常安全、智能指针与错误建模

## 学习目标

- 区分用法错误、业务失败、环境错误和编程错误。
- 理解异常传播、栈展开和 RAII 的配合。
- 正确选择值、引用、`unique_ptr`、`shared_ptr` 和 `weak_ptr`。
- 写出异常安全的复制赋值和容器操作。
- 知道何时使用 `optional`、错误码、`expected` 风格的返回值和异常。

## 1. 错误不是一种东西

至少区分：

- **输入/用法错误**：负库存、无效尺寸、非法命令。通常可以事先验证。
- **正常业务失败**：用户名已存在、库存不足、查询无结果。通常是返回值的一部分。
- **环境错误**：文件不存在、网络断开、内存不足。可能跨越多个层传播。
- **编程错误**：空指针解引用、违反不变量、越界访问。应通过测试和断言尽早发现。

不要用异常表示每个“没找到”。例如 `find` 没找到是正常结果，用迭代器或 `optional` 更合适。

## 2. 异常与 RAII

```cpp
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

std::string read_file(const std::string& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"cannot open " + path};
    }

    std::string data{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    };
    if (!input.eof() && input.fail()) {
        throw std::runtime_error{"cannot read " + path};
    }
    return data;
}
```

异常从函数中抛出时，局部对象按逆序销毁。文件流、字符串、容器和智能指针的析构函数仍会执行。这是 RAII 与异常协同工作的核心。

### 栈展开

```cpp
void f() {
    std::string text = "resource";
    throw std::runtime_error{"failure"};
    // text 的析构仍会调用
}
```

如果析构函数在栈展开期间又抛出异常，整个进程通常会终止。因此析构函数通常应 `noexcept`，内部失败只能记录日志或吞掉，不能随意向调用者抛出。

## 3. 异常安全保证

### 基本保证

异常发生后：

- 不泄漏资源。
- 对象处于有效状态。
- 具体内容可能已被部分修改。

### 强保证

操作有“事务语义”：要么成功，要么对象看起来没变。

复制并交换：

```cpp
#include <vector>

class Registry {
public:
    void replace(std::vector<int> new_values) {
        values_.swap(new_values);
    }

private:
    std::vector<int> values_;
};
```

参数 copy 在函数体前完成。构造参数失败时，`values_` 不变。`swap` 不抛异常，因此整体具备强保证。

### 不抛异常保证

用 `noexcept` 标记。一个函数若要获得该保证，其调用的所有操作都必须满足。不要为了让容器愿意移动就滥用 `noexcept`。

## 4. 智能指针的所有权模型

### `std::unique_ptr<T>`

默认的动态独占所有权工具：

```cpp
#include <memory>
#include <string>

auto name = std::make_unique<std::string>("Ada");
*name += " Lovelace";
```

- 不能复制。
- 可以移动。
- 同一时刻只有一个 owner。
- 对象离开作用域时自动释放。

工厂函数：

```cpp
std::unique_ptr<Shape> make_circle(double radius) {
    return std::make_unique<Circle>(radius);
}
```

调用者接收独占所有权，作用域结束自动删除。

### `std::shared_ptr<T>`

多个 owner 共同延长生命周期：

```cpp
auto config = std::make_shared<Config>();
auto worker_a = config;
auto worker_b = config;
```

最后一个 `shared_ptr` 销毁时对象才释放。控制块记录引用计数，分配和原子计数有成本。

常见误用：

- 把每个 `new` 都改成 `shared_ptr`。
- 函数只借用对象时却传 `shared_ptr<T>`。
- 用 `shared_ptr` 解决循环依赖。

共享所有权会延长对象寿命。能使用引用或 `unique_ptr` 时，不要用 `shared_ptr`。

### `std::weak_ptr<T>`

观察由 `shared_ptr` 管理的对象，不增加强引用数，避免循环引用：

```cpp
#include <memory>

struct Node {
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> previous;
};
```

使用前必须 `lock()`：

```cpp
if (auto locked = weak.lock()) {
    locked->do_work();
}
```

如果强引用已经为零，`lock()` 返回空，说明对象已不再存活。

## 5. `make_unique` 和 `make_shared`

优先使用：

```cpp
auto value = std::make_unique<int>(42);
auto object = std::make_shared<Widget>(arg1, arg2);
```

优势：

- 不需要显式 `new`。
- 分配与指针创建关系清晰。
- `make_shared` 可把对象和控制块放在一次分配中，但弱引用存在时对象释放会延迟。

自定义删除器：

```cpp
std::unique_ptr<std::FILE, decltype(&std::fclose)>
file{std::fopen("data.bin", "rb"), &std::fclose};
```

如果句柄为空，仍需在调用前检查。

## 6. 引用、指针与所有权

函数接口应表达借用还是转移：

```cpp
void inspect(const Widget& widget);              // 借用，不拥有
Widget take_ownership(Widget widget);            // 按值取得所有权
void set_widget(std::unique_ptr<Widget> w);      // 转移独占所有权
void observe(const Widget* maybe_widget);         // 可空借用
```

智能指针类型出现在参数中，表达的是接口合同，不只是内存安全。如果函数只读，不应该要求 `shared_ptr`，否则测试和调用都变复杂。

裸指针作为非拥有观察者仍然合法，但必须加上生命周期说明。团队常通过命名、注释或强类型来区分。

## 7. 选择错误表示方式

### `std::optional<T>`

适合“可能没有值”，但不需要说明原因：

```cpp
std::optional<int> parse_int(std::string_view text);
```

缺点是没有错误原因。调用者只看到空值。

### 返回错误码

适合错误是正常控制流、需要高性能或跨 ABI 的场景：

```cpp
enum class ParseError { empty, invalid_char, out_of_range };
```

可以配合结果类型使用。C++20 本身没有 `std::expected`（C++23 引入），课程项目会提供一个简化结果类型。

### 异常

适合无法在局部合理处理、需要沿调用栈传播的环境错误。异常应包含上下文，捕获者按能处理的层级处理。

```cpp
try {
    auto data = read_file(path);
    process(data);
} catch (const std::runtime_error& error) {
    std::cerr << "处理失败：" << error.what() << '\n';
    return 1;
}
```

不要捕获后什么也不做，也不要 `catch (...)` 后继续使用无效状态。

## 8. 异常中立与传播

一个函数不需要知道如何处理异常，但应保证资源安全，不吞掉异常，也不破坏不变量：

```cpp
void save_all(const std::vector<Record>& records) {
    std::ofstream out{"records.txt"};
    if (!out) {
        throw std::runtime_error{"cannot create output"};
    }
    for (const auto& record : records) {
        out << record << '\n';
    }
    if (!out) {
        throw std::runtime_error{"write failed"};
    }
}
```

这是“异常中立”：不捕获，但资源仍有 RAII 保护。

## 9. 设计与测试检查表

- 每个动态资源都有明确 owner。
- 失败路径不会留下半修改状态。
- 公共 API 清楚说明可空性、所有权和异常。
- 析构函数不抛出。
- 不在所有权不必要时使用 `shared_ptr`。
- 对异常路径写测试，而不只测成功路径。
- 捕获异常时保留上下文，不在最内层吞掉。
- 用 sanitizer 和泄漏检查工具验证生命周期。

## 练习 1：错误分类

为以下情况选择 `optional`、异常或错误类型，并说明理由：

1. 配置文件的端口不是数字。
2. 用户查询不存在的商品。
3. 磁盘已满导致写日志失败。
4. 用负数构造 `Circle`。
5. 访问容器越界索引。

## 练习 2：修复所有权

重写以下接口，使“是否为空”和“是否拥有”更清楚：

```cpp
Widget* find_widget(const std::vector<std::unique_ptr<Widget>>& widgets,
                    std::string_view name);
```

要求：

- 查询只借用对象。
- 找不到时可以返回空。
- 调用者不能删除容器中的对象。

## 练习 3：强异常安全

实现 `Inventory::set_all(std::vector<Item>)`，要求：

- 输入验证失败时库存保持原样。
- 成功时整体替换。
- 容器扩容、元素复制或移动抛出异常时不泄漏。

写明你依赖的复制/移动操作条件。

## 自测

1. 栈展开过程中 RAII 如何保证资源释放？
2. `unique_ptr` 和 `shared_ptr` 分别表达什么所有权？
3. 为什么 `shared_ptr` 可能形成循环，如何用 `weak_ptr` 解决？
4. 基本保证和强保证有什么区别？
5. 哪些业务失败不适合抛异常？

## 下一步

[第 11 章](11_cpp20_features.md)集中介绍 C++20 的语言和标准库改进。
