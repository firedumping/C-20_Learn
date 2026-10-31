# 练习参考说明

这里重点解释判断依据，不替代 `solutions/` 中的完整代码。先自己实现，再对照。

## Lab 01

整数解析不能先计算出任意大的 `long long` 再检查，因为超长输入本身就会溢出。每次乘以 10 之前检查：

```cpp
if (value > (limit - digit) / 10) {
    return std::nullopt;
}
```

正数的最大绝对值是 `INT_MAX`，负数的绝对值可以是 `INT_MAX + 1`。可用 `long long` 保存 `INT_MAX + 1`。

## Lab 02

安全加法分成两个方向：

```cpp
if ((rhs > 0 && lhs > INT_MAX - rhs) ||
    (rhs < 0 && lhs < INT_MIN - rhs)) {
    return false;
}
```

不要在检查前执行 `lhs + rhs`。用户名检查应先验证长度和首字符，再验证每个字符。

## Lab 03

二分不变量可写成：“目标若存在，其位置一定在 `[left, right)`”。中间位置用：

```cpp
left + (right - left) / 2
```

虽然 `std::size_t` 下溢行为与有符号溢出不同，这种写法仍能让边界意图清楚。质数循环使用 `long long divisor`，避免 `divisor * divisor` 在 `int` 中溢出。

清理分数可以直接把谓词交给标准库：

```cpp
std::erase_if(scores, [](int score) {
    return score < 0 || score > 100;
});
```

谓词返回 `true` 表示删除。`erase_if` 直接修改传入的 vector，不需要再调用一次容器 `erase`。

## Lab 04

`sum` 的 `int` 累加器本身也可能溢出。当前测试没有覆盖这一点，成熟版本应增加 `checked_sum` 或返回更大类型。`average` 先处理空 span，避免除零和无效返回值。

## Lab 05

`Temperature` 必须拒绝 NaN 和无穷，因为 `value < -273.15` 无法过滤 NaN。账户的先检查后修改保证失败时不改变余额。`deposit` 在修改前检查上溢。

## Lab 06

常见错误：

- 公有继承后基类析构不是虚函数。
- 用 `std::vector<Shape>` 导致不能实例化或对象切片。
- 用 `Shape*` 后忘记释放。
- 把 `-1` 当作“无效半径”而不是在构造时拒绝。

参考实现通过 `virtual ~Shape()`、纯虚接口和 `unique_ptr` 解决所有权与多态删除。

## Lab 07

规范化规则必须明确。本练习规定只保留 ASCII 字母、数字和下划线，因此 `"apple!"` 变成 `"apple"`，而 `"你好"` 不会成为有效单词。如果需要 Unicode，应引入专门的文本/Unicode 库，而不是假设一个 `char` 是一个字符。

排序比较器必须满足严格弱序。先比较次数，再比较单词，两个分支不能同时为真。

## Lab 08

有效的约束：

```cpp
template <class T>
concept Addable = requires(const T& lhs, const T& rhs) {
    { lhs + rhs } -> std::convertible_to<T>;
};
```

直接要求 `value + value` 合法还不够，因为函数返回 `T`，需要结果能够转换回 `T`。`std::string` 满足该约束，普通整数也满足。

## Lab 09

移动后源对象为空是当前测试的合同，不是语言对所有类型的要求。标准库对象只保证“有效但未指定状态”。如果手写裸资源：

- 移动后把源指针置空，避免双重释放。
- 移动赋值先处理自赋值。
- 释放旧资源，再接管新资源。
- 移动操作要判断是否真的满足 `noexcept`。

更推荐让 `std::vector` 或 `std::unique_ptr` 成员承担资源管理。

## Lab 10

返回 `std::variant<int, ParseError>` 表示“成功值或具体错误”，调用者必须处理两种情况。`optional` 只适合不需要错误原因的情况。

`make_temperature` 通过 `std::make_unique` 构造对象。如果构造函数抛异常，不会返回半成品，也不会泄漏。

## Lab 11

### 核心思路

Ranges 管线：

```cpp
auto result_view = values
    | std::views::filter(is_even)
    | std::views::transform(square)
    | std::views::take(limit);
```

如果函数返回容器，必须把视图中的元素复制到 vector；直接返回视图会悬空，因为 `values` 是参数外部对象。

格式化字段宽度的概念：

- `{:<10}`：宽度 10，左对齐。
- `{:>5}`：宽度 5，右对齐。
- `{:.2f}`：固定两位小数。

### 兼容性

`<format>` 老标准库可能缺失。此时可以暂时使用 `<iomanip>` 输出，但不要声称两种实现的格式化规则完全相同。

## Lab 12

线程写入 `partials[i]` 的不同元素是安全的，因为每个元素是独立的内存位置，且 `partials` 不再扩容。但它们不是“多个写者写同一个对象”，若改成单个共享 `total +=` 就必须同步。

`jthread` 的析构会请求停止并 join。代码中显式 `clear()` 让 join 发生在函数末尾使用 `partials` 之前，意图更直接。

## Lab 13

`remove` 先用 `find_if` 找到第一个匹配项，检查 `end()` 后再 `erase`。删除使被删位置及其后迭代器失效，但函数立即返回，不继续使用旧迭代器。

如果业务要求支持重复标题，可改成删除全部、返回删除数量，或者给每个任务独立 ID。接口必须反映选择。

## Lab 14

`next()` 的外部合同：

1. 若 `current < limit`，计算平方。
2. 增加 `current`，使下一次调用从新位置继续。
3. 否则返回 `nullopt`。

这正是按需生成值得状态转移。真正的 C++20 协程还需要 promise、awaiter、挂起/恢复和生命周期管理，不能仅用这个类代替。

## 统一复盘问题

1. 每个函数的输入能否为空或非法？
2. 哪些对象拥有内存，哪些只借用？
3. 哪个算术操作可能溢出？
4. 失败后对象是否仍满足不变量？
5. 是否存在并发共享写？
6. 视图返回后，底层序列是否仍存活？
7. 测试是否覆盖边界，而不只覆盖正常样例？
