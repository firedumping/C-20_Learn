# 第 5 章：类、不变量与 RAII

## 学习目标

- 用类封装“数据 + 有效状态规则 + 操作”。
- 理解构造函数、析构函数、成员初始化顺序和对象生命周期。
- 用 RAII 让资源释放绑定到对象生命周期。
- 理解 Rule of Zero、Rule of Five 和复制语义。
- 写出满足基本异常安全的类。

## 1. 类不是带函数的结构体

下面的结构允许任意状态，调用者必须自己记得每项规则：

```cpp
struct Account {
    double balance;
};

Account a{0.0};
a.balance = -100.0; // 银行账户可能不允许透支
```

类设计的关键是**不变量**：对象在每个公开操作完成后都必须保持为真的条件。例如：

- 余额不小于零。
- 名称非空。
- 容器容量不小于大小。

把数据设为 `private`，公开函数负责维护不变量：

```cpp
#include <stdexcept>

class Account {
public:
    explicit Account(double initial_balance)
        : balance_{initial_balance} {
        if (initial_balance < 0.0) {
            throw std::invalid_argument{"initial balance must be non-negative"};
        }
    }

    void deposit(double amount) {
        if (amount <= 0.0) {
            throw std::invalid_argument{"deposit must be positive"};
        }
        balance_ += amount;
    }

    bool withdraw(double amount) {
        if (amount <= 0.0 || amount > balance_) {
            return false;
        }
        balance_ -= amount;
        return true;
    }

    double balance() const noexcept {
        return balance_;
    }

private:
    double balance_;
};
```

注意细节：

- 构造函数先执行成员初始化列表，再执行函数体；因此负余额检查写在函数体时，对象已经短暂存在，但尚未完成构造。更严谨的设计可在成员初始化前用辅助函数验证，或让类型系统表达“非负金额”。
- `deposit` 失败不会修改状态。
- `withdraw` 先检查再修改，失败时状态不变。
- `balance()` 是 `const` 成员函数，表示不修改逻辑状态。
- 这里为教学使用 `double`；生产金额通常使用最小货币单位整数或十进制类型。

## 2. 构造与析构

```cpp
#include <iostream>

class Tracer {
public:
    explicit Tracer(int id) : id_{id} {
        std::cout << "construct " << id_ << '\n';
    }

    ~Tracer() {
        std::cout << "destroy " << id_ << '\n';
    }

private:
    int id_;
};
```

对象创建顺序：

1. 分配存储。
2. 按声明顺序初始化基类（若有）。
3. 按成员声明顺序初始化成员，而不是按初始化列表书写顺序。
4. 执行构造函数体。

销毁顺序与构造顺序相反。若有多个成员：

```cpp
class Pair {
public:
    Pair() : b_{2}, a_{1} {}

private:
    int a_;
    int b_;
};
```

实际初始化顺序由**成员声明顺序**决定，因此仍然是先 `a_` 后 `b_`，与初始化列表中的书写顺序无关。把 `b_` 写在前面容易误导；开启警告后，编译器通常会指出这个问题。

## 3. RAII：把资源生命周期绑定到对象

RAII 是 Resource Acquisition Is Initialization 的缩写。真正的含义更接近：

- 构造成功 = 资源已获得。
- 析构 = 资源可靠释放。
- 对象的生命周期就是资源的使用窗口。

标准库的 `std::string`、`std::vector`、`std::fstream` 和 `std::unique_ptr` 都遵循 RAII。你不需要手动释放它们内部的资源。

### 文件 RAII 的动机

容易出错的版本：

```cpp
#include <cstdio>

bool write_bad(const char* path, const char* data) {
    std::FILE* file = std::fopen(path, "w");
    if (!file) {
        return false;
    }

    const bool ok = std::fputs(data, file) >= 0;
    if (!ok) {
        return false; // 忘记 fclose
    }

    std::fclose(file);
    return true;
}
```

现代 C++ 通常直接使用流：

```cpp
#include <fstream>

bool write_text(const char* path, const std::string& data) {
    std::ofstream output{path, std::ios::trunc};
    if (!output) {
        return false;
    }
    output << data;
    return output.good();
}
```

即使提前返回或抛出异常，`std::ofstream` 的析构函数也会关闭文件。

### 自己实现资源管理类

```cpp
#include <cstdio>
#include <stdexcept>

class File {
public:
    explicit File(const char* path)
        : handle_{std::fopen(path, "rb")} {
        if (!handle_) {
            throw std::runtime_error{"cannot open file"};
        }
    }

    ~File() {
        std::fclose(handle_);
    }

    File(const File&) = delete;
    File& operator=(const File&) = delete;

    File(File&& other) noexcept : handle_{other.handle_} {
        other.handle_ = nullptr;
    }

    File& operator=(File&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                std::fclose(handle_);
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

private:
    std::FILE* handle_;
};
```

这是 Rule of Five 的一个教学版本。真实项目应优先封装成标准库已有类型，或使用 `std::unique_ptr<T, Deleter>` 管理自定义资源，减少手写移动赋值。

## 4. Rule of Zero / Three / Five

### Rule of Zero

如果没有自己拥有的裸资源，让成员类型负责：

```cpp
class User {
public:
    User(std::string name, int age) : name_{std::move(name)}, age_{age} {}

private:
    std::string name_;
    int age_{};
};
```

编译器生成的复制和移动操作按成员执行，通常正是需要的。绝大多数类应追求 Rule of Zero。

### Rule of Five

如果明确实现了以下任意一个，通常要检查全部五个：

1. 析构函数。
2. 复制构造。
3. 复制赋值。
4. 移动构造。
5. 移动赋值。

不是必须全部提供，而是必须有意识地决定复制/移动是否允许，以及如何保持资源唯一性。

### 复制和移动的成本

```cpp
std::vector<int> copy = original;             // 复制元素
std::vector<int> moved = std::move(original); // 转移缓冲区；
                                               // original 仍为有效但未指定状态
```

移动通常 O(1) 转移内部分配，但对 `std::array` 等固定大小对象可能逐元素移动。不要以为写了 `std::move` 就一定更快。

## 5. 成员函数与 `const` 正确性

```cpp
class Counter {
public:
    void increment() {
        ++count_;
    }

    int value() const noexcept {
        return count_;
    }

private:
    int count_{};
};
```

`value()` 末尾的 `const` 表示不能通过该函数修改对象的非 `mutable` 成员。`const Counter` 只能调用 `const` 成员函数。

不要用 `const_cast` 绕过这个规则。如果逻辑上真的需要缓存，明确标记 `mutable`，并思考线程安全。

## 6. 构造函数设计与 `explicit`

```cpp
class Meters {
public:
    explicit Meters(double value) : value_{value} {}
    double value() const noexcept { return value_; }

private:
    double value_;
};
```

`explicit` 阻止 `void f(Meters); f(3.0);` 这类隐式转换。对单参数构造函数，默认加 `explicit`，除非你明确想允许转换，例如某些强类型包装器。

参数很多时：

```cpp
struct ConnectionOptions {
    std::string host;
    int port{8080};
    bool use_tls{true};
};

ConnectionOptions options{
    .host = "example.com",
    .port = 443,
};
```

这是 C++20 的指定初始化器。字段必须按声明顺序书写，且不能混用未指定的区间语法；编译器会强制这些规则。

## 7. 基本异常安全保证

常见等级：

- **不泄漏**：异常发生后资源仍被释放。
- **基本保证**：异常发生后对象仍处于有效状态，可能内容改变。
- **强保证**：操作要么完全成功，要么对象保持原状态。
- **不抛异常**：`noexcept` 操作保证不抛异常。

实现强保证的常用方法是先构造临时值，再一次性交换：

```cpp
class Names {
public:
    void replace_all(std::vector<std::string> new_names) {
        names_.swap(new_names);
    }

private:
    std::vector<std::string> names_;
};
```

参数复制发生在函数体之前；如果构造成员出错，成员没有被修改。最终 `swap` 通常不抛异常。

## 8. 接口设计小例子

考虑一个表示“库存”的类。需要回答：

- 库存能是负数吗？
- 数量能超过某个最大界限吗？
- 添加一个不存在的商品应创建，还是失败？
- 减少数量失败时是否部分修改？

一个清楚的接口可能是：

```cpp
enum class StockResult { success, insufficient };

class Inventory {
public:
    StockResult remove(std::string_view sku, int amount);
    void add(std::string sku, int amount);
    int quantity(std::string_view sku) const;

private:
    std::unordered_map<std::string, int> quantities_;
};
```

错误的输入不应抛异常还是返回值？这取决于错误是否属于正常业务流。无效参数、资源耗尽等可用异常；库存不足、查询未命中通常是可预期结果，可用错误码或 `std::optional`。

## 练习 1：`Temperature`

实现值类型 `Temperature`：

- 内部存储摄氏度。
- 可以从摄氏和华氏构造，接口明确、无隐式转换。
- 低于绝对零度的值抛出 `std::invalid_argument`。
- 提供 `celsius()`、`fahrenheit()`，都是 `const noexcept`。
- 不暴露可导致不变量的写入操作。

## 练习 2：`FileHandle`

用 RAII 封装 `std::FILE*`：

- 构造失败抛出异常。
- 禁止复制。
- 支持移动。
- 析构释放。
- 移动后的源对象可安全析构。

先写出对象各操作后的不变量，再编码。

## 练习 3：强异常安全的 `replace_all`

实现一个字符串容器类，提供 `replace_all(std::vector<std::string>)`。要求：

- 新值构造或复制失败时，原对象不变。
- 成功后整体替换。
- 写出你依赖的异常安全前提。

## 自测

1. 类的“不变量”是什么？
2. 成员初始化顺序由什么决定？
3. Rule of Zero 为什么通常优于手写 Rule of Five？
4. 移动后的对象还能做什么？
5. 什么是强异常安全保证？

## 下一步

[第 6 章](06_inheritance_polymorphism.md)讨论多态，但重点会比较继承与组合的成本，而不是把所有类型都塞进继承树。
