# 第 4 章：作用域、生命周期、引用与指针

## 学习目标

- 区分名字的作用域与对象的生命周期。
- 理解自动存储期、动态存储期和静态存储期。
- 正确使用引用、指针和 `const`。
- 识别悬空引用、悬空指针和数组越界。
- 理解“所有权”不是指针类型自动带来的承诺。

## 1. 名字、对象、值

```cpp
int main() {
    int count = 3;   // 名字 count 绑定到一个 int 对象
    count = 4;       // 修改对象的值
    int& alias = count; // alias 是对象的别名
    alias = 5;       // 修改同一个对象
}
```

名字 `count` 的作用域是它可见的代码区域；对象 `count` 的生命周期是从构造到销毁的时间。两者经常同时开始和结束，但不是同一概念。对象通过不同名字、引用或指针被访问，可以活得比某个名字更久。

## 2. 存储期

### 自动存储期

局部变量通常有自动存储期，离开作用域时销毁：

```cpp
void f() {
    int local = 10;
} // local 结束生命周期
```

### 动态存储期

用 `new` 创建的对象会一直存活，直到匹配的 `delete`：

```cpp
int* p = new int{42};
delete p;
p = nullptr;
```

手工 `new/delete` 很容易在异常路径泄漏。现代 C++ 优先使用值语义容器和智能指针，见[第 10 章](10_errors_and_ownership.md)。

### 静态/线程存储期

全局变量、命名空间作用域变量和 `static` 局部变量具有较长生命周期：

```cpp
const char* greeting() {
    static const char text[] = "hello";
    return text; // text 在程序结束前一直存在
}
```

静态局部变量在现代 C++ 中的初始化是线程安全的，但多线程访问其可变状态仍需同步。

## 3. 引用

```cpp
int value = 10;
int& ref = value;

ref = 20;
```

引用是对象的别名，创建时必须绑定，通常不能改成绑定另一个对象。引用本身不表示“拥有对象”；函数接受 `T&` 时，通常假设调用者在调用期间保证对象存活。

### `const` 引用

```cpp
void print(const std::string& text);
```

优点：

- 不复制大对象。
- 函数不能通过该引用修改对象。
- 可以绑定临时对象，并在调用表达式结束前保持其生命周期。

但不要返回自动变量的 `const&`：

```cpp
const std::string& bad() {
    std::string text = "temporary";
    return text; // 返回悬空引用
}
```

## 4. 指针

```cpp
int value = 7;
int* ptr = &value;
*ptr = 8;

if (ptr != nullptr) {
    std::cout << *ptr << '\n';
}
```

指针可以为空，也可以重定向：

```cpp
int other = 9;
ptr = &other;
```

一个指针值合法不代表它指向的对象仍然活着。生命周期结束后，指针值可能仍在，但解引用是未定义行为。

```cpp
int* dangling = nullptr;
{
    int temp = 1;
    dangling = &temp;
}
// *dangling 未定义行为
```

### 指针本身是否可修改、对象是否可修改

`const` 放在 `*` 的哪一侧，限制的对象不同：

```cpp
int first = 10;
int second = 20;

const int* pointer_to_const = &first;
// *pointer_to_const = 11; // 错误：不能通过指针修改对象
pointer_to_const = &second; // 可以：指针本身可以改指向

int* const constant_pointer = &first;
*constant_pointer = 11;     // 可以：对象可修改
// constant_pointer = &second; // 错误：指针本身不能改指向

const int* const fixed_view = &first;
// 对象不能通过它修改，指针也不能改指向
```

记忆方法：

| 写法 | `*ptr` 是否可写 | `ptr` 是否可改指向 |
|---|---:|---:|
| `int*` | 是 | 是 |
| `const int*` | 否 | 是 |
| `int* const` | 是 | 否 |
| `const int* const` | 否 | 否 |

从右向左读更清楚：`int* const` 是“const pointer to int”；`const int*` 是“pointer to const int”。

## 5. 数组、指针算术和 `std::span`

数组在表达式中经常退化为指向首元素的指针：

```cpp
int values[]{10, 20, 30};
int* first = values;
int second = first[1];
```

一旦变成指针，长度信息就丢失了。不要用裸指针和手写长度作为现代接口的第一选择。C++20 的 `std::span` 可以表达连续序列：

```cpp
#include <span>

int sum(std::span<const int> values) {
    int result = 0;
    for (int value : values) {
        result += value;
    }
    return result;
}
```

`std::span` 不拥有元素，也不检查元素是否仍然存活。它只是“首地址 + 长度”的轻量视图。调用者不能把局部数组的 span 返回并长期保存。

## 6. 引用与指针怎么选

优先顺序：

1. 值语义：函数确实需要自己的对象副本。
2. `const T&`：只读借用大型对象，调用期间存在。
3. `T&`：需要修改调用者对象。
4. `std::span<T>` / `std::string_view`：借用连续序列或文本。
5. 智能指针：需要表达共享或独占的动态所有权。
6. 裸指针：只作为非拥有观察者，且必须清楚可空性和生命周期。

“必须用指针”不是一个完整理由。先问：谁拥有对象？对象能活多久？指针是否允许为空？

## 7. 引用失效：容器和扩容

```cpp
#include <vector>

std::vector<int> values{1, 2, 3};
int& first = values.front();

values.push_back(4); // 可能重新分配内存
// first 现在可能是悬空引用
```

如果 `push_back` 导致扩容，旧元素可能被搬到新缓冲区。对容器的引用、指针和迭代器都可能失效。使用 `values.front()` 重新获取，或在扩容前预留足够容量，能减少这种风险。

规则不是“永远不要保存引用”，而是“操作后确认失效规则”。标准库的不同容器有不同失效条件。

## 8. `nullptr` 与空值表达

C++11 起使用 `nullptr`，不要用 `NULL` 或 `0` 当空指针：

```cpp
int* p = nullptr;
if (p) {
    // 非空
}
```

函数参数如果是“可能没有对象”，可使用指针、`std::optional<T>` 或明确的接口设计。对于多态对象，可能使用 `std::unique_ptr<Base>` 表达“没有就为空”。

## 9. 生命周期分析模板

看到一个对象时，按下面四问检查：

1. 它在哪里被创建？
2. 谁负责让它结束？
3. 结束发生在哪些路径上（正常返回、异常、提前返回）？
4. 哪些指针/引用/视图可能在结束后仍被使用？

例如：

```cpp
std::string_view choose_name(bool use_short) {
    std::string short_name = "A";
    std::string long_name = "Alexander";
    return use_short ? short_name : long_name;
}
```

这个函数必然返回悬空视图。修复方式是返回值 `std::string`，或让视图指向生命周期由调用者保证的对象。

## 练习 1：生命周期追踪

为下面代码画出对象创建和销毁顺序，并预测输出：

```cpp
#include <iostream>

struct Tracer {
    const char* name;
    Tracer(const char* n) : name(n) { std::cout << "create " << name << '\n'; }
    ~Tracer() { std::cout << "destroy " << name << '\n'; }
};

int main() {
    Tracer a{"a"};
    {
        Tracer b{"b"};
    }
    Tracer c{"c"};
}
```

## 练习 2：修复悬空视图

修复 `choose_name`，提供两个版本：

1. 返回拥有文本的 `std::string`。
2. 返回 `std::string_view`，但由调用者传入生命周期足够的字符串。

解释两个版本的接口合同。

## 练习 3：安全统计

实现：

```cpp
int sum(std::span<const int> values);
double average(std::span<const int> values);
```

讨论空 span 的平均值应返回什么，以及为什么不能返回引用指向局部结果。

## 自测

1. 作用域和生命周期有什么区别？
2. 为什么返回局部变量的引用危险？
3. `std::string_view` 是否拥有字符数据？
4. 为什么 `std::vector` 扩容后保存的引用可能失效？
5. 哪些问题在编译时无法由编译器发现？

## 下一步

[第 5 章](05_classes_and_raii.md)将把“资源在构造时获得、在析构时释放”的原则落实为类设计。
