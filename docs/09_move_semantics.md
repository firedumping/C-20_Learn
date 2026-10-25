# 第 9 章：值类别、移动语义与完美转发

## 学习目标

- 区分左值、将亡值和纯右值。
- 理解移动构造/移动赋值真正转移的是什么。
- 知道 `std::move` 本质上是类型转换，不移动任何东西。
- 理解转发引用和引用折叠。
- 避免对 const 对象、临时对象或不支持移动的类型做无效“优化”。
- 让移动后的对象保持合法可析构状态。

## 1. 为什么需要移动

复制一个拥有大缓冲区的对象通常要分配内存并复制元素：

```cpp
std::vector<int> source(1'000'000, 7);
std::vector<int> destination = source; // 复制一百万个元素
```

如果 `source` 即将销毁，继续复制就浪费。移动允许把内部资源的所有权直接转给新对象：

```cpp
std::vector<int> source(1'000'000, 7);
std::vector<int> destination = std::move(source);
```

标准库 `vector` 的移动通常只转移指针、大小和容量，复杂度常数级。但语言不保证移动一定比复制快，也不保证移动一定不抛异常；要看具体类型。

## 2. 值类别

用简化但实用的方式理解：

- **左值（lvalue）**：有可识别的持久身份，通常可取地址。例如命名变量 `x`。
- **将亡值（xvalue）**：仍是对象，但明确允许把资源转走。例如 `std::move(x)`。
- **纯右值（prvalue）**：没有持久身份的临时值。例如 `42`、`x + 1` 的结果。

泛称：

- glvalue = lvalue 或 xvalue。
- rvalue = prvalue 或 xvalue。

```cpp
int x = 42;
int& lref = x;               // 左值引用绑定左值
// int&& bad = x;            // 错误：不能把左值绑定到右值引用
int&& rref = std::move(x);   // 绑定将亡值类的表达式
```

`std::move(x)` 的返回类型是 `T&&`，它并**没有**执行任何内存转移。真正的移动发生在构造函数或赋值运算符被选中并执行时。

## 3. 移动构造和移动赋值

一个简化缓冲区：

```cpp
#include <cstddef>
#include <utility>

class Buffer {
public:
    explicit Buffer(std::size_t size)
        : data_{new int[size]}, size_{size} {}

    ~Buffer() {
        delete[] data_;
    }

    Buffer(const Buffer& other)
        : data_{new int[other.size_]}, size_{other.size_} {
        for (std::size_t i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
    }

    Buffer& operator=(const Buffer& other) {
        if (this == &other) {
            return *this;
        }
        Buffer copy{other};
        swap(copy);
        return *this;
    }

    Buffer(Buffer&& other) noexcept
        : data_{other.data_}, size_{other.size_} {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    void swap(Buffer& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
    }

private:
    int* data_{};
    std::size_t size_{};
};
```

要点：

- 移动构造“偷走”指针，把源对象的指针置空。
- 移动后的源对象必须仍能安全析构、赋值，至少满足该类文档中承诺的操作。
- 移动赋值要释放当前资源，防止泄漏。
- 自赋值检查在这种转移模式下有必要。
- 实际项目优先使用 `std::vector` 等成员实现 Rule of Zero，不建议初学阶段在日常代码中手写 `new[]`。

### 为什么移动操作常标记 `noexcept`

例如 `std::vector` 扩容时，只有元素的移动构造明确不抛异常，容器才能安全地使用移动而不是复制。否则移动途中抛异常可能无法恢复。一个会分配或可能抛异常的操作不应随便标 `noexcept`。

## 4. `std::move` 的正确使用位置

接收按值参数时：

```cpp
class Person {
public:
    explicit Person(std::string name) : name_{std::move(name)} {}

private:
    std::string name_;
};
```

调用者传左值时复制一次，传右值时移动进入参数；然后参数再移动到成员。不要写：

```cpp
Person(const std::string& name) : name_{std::move(name)} {} // 错：不能从 const 移动
```

这里只能复制，`std::move` 只是产生 `const std::string&&`，仍会选择复制构造。

函数返回值通常不要写 `return std::move(value);`：

```cpp
std::string make_name() {
    std::string name = "Ada";
    return name; // 编译器可执行 NRVO 或隐式移动
}
```

显式 `std::move` 可能阻止命名返回值优化（NRVO）。只在确实需要把某个对象转换为右值表达式时使用。

## 5. 移动不等于“对象消失”

```cpp
std::string source = "hello";
std::string destination = std::move(source);

source.clear();          // 通常安全
source = "again";        // 通常安全
```

移动后的标准库对象处于“有效但未指定状态”。你可以销毁、赋值，以及执行文档允许的操作；不要假设它仍然是空字符串或特定长度。

## 6. 转发引用与引用折叠

```cpp
template <class T>
void relay(T&& value) {
    consume(std::forward<T>(value));
}
```

这里 `T&&` 不一定是右值引用。如果传给 `relay` 的是 `int&`：

- `T` 推导为 `int&`。
- `T&&` 经过引用折叠成为 `int&`。

如果传入 `int` 临时值：

- `T` 推导为 `int`。
- `T&&` 是 `int&&`。

规则：

```text
&  &  -> &
&  && -> &
&& &  -> &
&& && -> &&
```

`std::forward<T>` 保留实参原本的值类别。如果全部写 `std::move(value)`，传入左值时也会被错误地当作右值转走。

### 识别转发引用

只有形如 `T&&` 且 `T` 是当前函数模板的推导参数时，才是转发引用：

```cpp
template <class T> void f(T&&);      // 转发引用
template <class T> void g(std::vector<T>&&); // 右值引用
void h(int&&);                       // 右值引用
```

## 7. 完美转发的实际用途

包装构造函数：

```cpp
#include <memory>
#include <utility>

template <class T, class... Args>
std::unique_ptr<T> make_object(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}
```

这会保留参数的值类别，但不绕过类型检查。实际写代码时优先使用标准库已有的 `std::make_unique`、`std::make_shared`、`std::emplace_back`。

## 8. 常见误用

### 返回局部对象的 `std::move`

```cpp
std::vector<int> make() {
    std::vector<int> result;
    return std::move(result); // 不必要，可能影响 NRVO
}
```

直接 `return result;`。

### 对 const 成员执行“移动”

```cpp
class Holder {
public:
    Holder(const std::string& text) : text_{std::move(text)} {}
private:
    std::string text_;
};
```

`text` 是 `const std::string&`，无法调用可修改的移动构造，最终仍复制。

### 移动容器元素后继续依赖旧元素

```cpp
std::string first = std::move(names[0]);
// names[0] 仍存在，但内容处于未指定状态
```

如果容器之后还要把 `names[0]` 当原值使用，先复制或重新赋值。

## 9. 设计规则

- 默认使用值语义和标准容器，让它们实现移动。
- 自己管理裸资源时才考虑 Rule of Five。
- 移动构造和移动赋值通常是 `noexcept`，前提是内部操作确实不抛异常。
- `std::move` 表示“我允许把这个对象当作将亡值”，不是“马上释放”。
- `std::forward` 只用于转发引用模板。
- 移动后的对象必须满足类的不变量和安全销毁要求。
- 性能问题要测量，不要因每个临时对象都写 `std::move` 而降低可读性。

## 练习 1：预测重载

```cpp
#include <iostream>
#include <utility>

void show(int&) { std::cout << "lvalue\n"; }
void show(int&&) { std::cout << "rvalue\n"; }

int main() {
    int x = 1;
    show(x);
    show(2);
    show(std::move(x));
    show(x + 3);
}
```

预测输出，并解释每个实参的值类别。

## 练习 2：实现移动类型

实现一个 move-only 的 `DynamicArray`：

- 拥有连续整数数组。
- 禁止复制。
- 支持移动和析构。
- 移动后源对象可安全销毁。
- 提供 `size()` 和 `operator[]` 的 const 访问。

先写测试覆盖空数组、自移动、移动赋值和析构。

## 练习 3：修复转发

```cpp
template <class T>
void relay(T&& value) {
    sink(std::move(value));
}
```

当 `value` 实际绑定左值时会发生什么？用 `std::forward<T>` 修复，并写两条调用路径测试。

## 自测

1. `std::move` 本身做了什么？
2. 为什么移动后的对象仍然必须可析构？
3. 为什么移动构造常标记 `noexcept`？
4. 转发引用与普通右值引用如何区分？
5. 什么时候应该使用 `std::forward`？

## 下一步

[第 10 章](10_errors_and_ownership.md)会把这些机制用于异常安全和智能指针所有权设计。
