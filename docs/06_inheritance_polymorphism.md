# 第 6 章：继承、多态与组合

## 学习目标

- 理解 `virtual` 动态分派和虚析构函数。
- 区分接口继承和实现继承。
- 理解 `override`、`final` 和基类引用/指针的所有权风险。
- 知道切片、对象切片和基类析构问题。
- 优先用组合和值语义表达“有一个”，只在合适时使用继承。

## 1. 继承表达什么

```cpp
class Shape {
public:
    virtual ~Shape() = default;
    virtual double area() const = 0;
};

class Circle final : public Shape {
public:
    explicit Circle(double radius) : radius_{radius} {}

    double area() const override {
        return 3.141592653589793 * radius_ * radius_;
    }

private:
    double radius_;
};
```

用途是让一组类型共享接口：

```cpp
#include <memory>
#include <vector>

double total_area(const std::vector<std::unique_ptr<Shape>>& shapes) {
    double total = 0.0;
    for (const auto& shape : shapes) {
        total += shape->area();
    }
    return total;
}
```

要点：

- `virtual` 函数可以被派生类重写。
- 纯虚函数 `= 0` 让 `Shape` 成为抽象类，不能直接构造。
- `override` 让编译器检查签名是否真的重写了基类函数。
- `final` 禁止继续派生或重写。
- 多态基类应提供 `virtual ~Base()` 或 `protected` 非虚析构，通常用前者。

## 2. 为什么基类析构必须虚

```cpp
class Base {
public:
    ~Base() = default; // 非虚
};

class Derived : public Base {
public:
    ~Derived() {
        // 释放派生类资源
    }
};

void bad() {
    Base* p = new Derived;
    delete p; // 未定义行为：可能只调用 Base::~Base
}
```

如果对象通过基类指针删除，基类析构是入口。只有虚析构才能继续调用派生类析构。

现代代码中这种所有权应由 `std::unique_ptr<Base>` 持有；但智能指针也无法修复非虚析构。设计多态基类时必须确定删除方式。

## 3. 重载、重写和隐藏

- **重载**：同一作用域中同名但参数不同。
- **重写**：派生类函数与基类虚函数签名兼容。
- **隐藏**：派生类同名函数遮蔽基类同名集合。

```cpp
class Base {
public:
    virtual void log(int) const {}
};

class Derived : public Base {
public:
    void log(double) const {} // 想重写但参数不同：错误/隐藏
    using Base::log;          // 若确实要暴露基类重载
};
```

始终写 `override`。它能阻止签名不匹配导致的“看起来是重写，实际没有”。

## 4. 对象切片

```cpp
void print(Shape shape); // 按值传基类

Circle circle{1.0};
print(circle); // 只复制 Shape 子对象，Circle 部分丢失
```

多态对象通过值传递会切片。接口应使用 `Shape&`、`Shape const&` 或明确的安全值类型。

容器保存多态对象也不能写：

```cpp
std::vector<Shape> shapes; // 抽象类不能被构造
```

通常用：

```cpp
std::vector<std::unique_ptr<Shape>> shapes;
```

或者更好的值方案：`std::variant<Circle, Rectangle, ...>`。

## 5. 组合优先

需要“日志格式化器”和“日志接收器”时，不要让 `FileLogger` 继承 `Formatter`，因为它不是一种格式化器。它使用格式化器：

```cpp
class Formatter {
public:
    virtual ~Formatter() = default;
    virtual std::string format(std::string_view message) const = 0;
};

class Sink {
public:
    virtual ~Sink() = default;
    virtual void write(std::string_view text) = 0;
};

class Logger {
public:
    Logger(std::unique_ptr<Formatter> formatter, std::unique_ptr<Sink> sink)
        : formatter_{std::move(formatter)}, sink_{std::move(sink)} {}

    void log(std::string_view message) {
        sink_->write(formatter_->format(message));
    }

private:
    std::unique_ptr<Formatter> formatter_;
    std::unique_ptr<Sink> sink_;
};
```

这里 `Logger` 组合了两个能力。依赖注入让测试能使用内存 sink 和固定 formatter。

但不要刚开始就为每个变化建立接口。如果只有一种实现，值类型和模板往往更简单。

## 6. 动态多态与静态多态

动态多态：

- 通过虚函数和继承实现。
- 运行时决定调用哪个重写函数。
- 适合插件式、运行时扩展、稳定的类型擦除边界。
- 有间接调用和对象指针/引用要求。

静态多态：

- 通过模板或 Concepts 实现。
- 编译时确定具体类型。
- 通常更容易内联，没有虚调用开销。
- 会导致模板实例化，接口错误信息可能更长。

示例：

```cpp
#include <concepts>

template <class T>
concept HasArea = requires(const T& value) {
    { value.area() } -> std::convertible_to<double>;
};

template <HasArea T>
double area_of(const T& shape) {
    return shape.area();
}
```

如果不需要运行时存放不同类型的对象，静态多态通常是更自然的起点。

## 7. `std::variant`：封闭式多态

当类型集合固定且数量少，可以使用值语义的 `std::variant`：

```cpp
#include <variant>
#include <type_traits>

struct Circle { double radius; };
struct Rectangle { double width; double height; };

using Shape = std::variant<Circle, Rectangle>;

double area(const Shape& shape) {
    return std::visit([](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, Circle>) {
            return 3.14159 * value.radius * value.radius;
        } else {
            return value.width * value.height;
        }
    }, shape);
}
```

优势是值语义、容易放在容器中、无需手动管理生命周期。代价是类型集合在编译时固定，并且 `visit` 可能让代码更模板化。

## 8. 包含多态对象的接口设计

多态函数通常不应接收裸指针，除非参数明确可为空且调用者保证生命周期：

```cpp
double area(const Shape& shape);               // 非空、借用
double maybe_area(const Shape* shape);         // 可空、借用
std::unique_ptr<Shape> make_shape();           // 转移独占所有权
std::shared_ptr<Shape> share_shape();          // 共享所有权，需谨慎
```

`shared_ptr` 不是“更安全的 new”。只有当多个主体确实需要共同延长生命周期时才使用，否则所有权会变得难以追踪。

## 9. 接口继承的规则

- 公有继承表示“is-a”，并承诺满足基类合同。
- 不要为了复用一段实现而公有继承。
- 基类析构要支持删除方式。
- 有虚函数的类通常不要让复制成为隐式行为；可能需要禁用或明确克隆。
- 虚函数不宜过多暴露实现细节。
- 重写函数保持 `override` 和与合同一致的异常规范。

## 练习 1：形状系统

实现 `Shape`、`Circle`、`Rectangle`：

- `area()`、`perimeter()`。
- 多态基类有虚析构。
- 不能构造非法尺寸。
- 通过 `vector<unique_ptr<Shape>>` 计算总面积。

然后写一个 `std::variant` 版本，比较接口和调用位置。

## 练习 2：插件式通知

设计 `Notifier` 接口和 `EmailNotifier`、`ConsoleNotifier`。再设计一个 `MultiNotifier`，组合多个通知器。

要求：

- 先写测试替身，验证消息被正确格式化。
- 不要通过公有继承让 `MultiNotifier` 继承某一个具体通知器。
- 讨论失败的通知是否应阻止其他通知器执行。

## 练习 3：识别切片

下面的代码有哪些问题？给出修复：

```cpp
struct Base { virtual void f(); };
struct Derived : Base { void f() override; };

void use(Base value) {
    value.f();
}
```

## 自测

1. 为什么多态基类通常需要虚析构？
2. `override` 解决了什么问题？
3. 对象切片发生在什么时候？
4. 什么情况下组合优于继承？
5. `std::variant` 与虚函数多态各有什么取舍？

## 下一步

[第 7 章](07_stl_and_algorithms.md)将把容器、迭代器、算法和 lambda 串成一条数据处理链。
