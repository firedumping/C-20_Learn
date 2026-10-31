# 自动检查练习

全部练习位于 `starter/labs.cpp` 与 `starter/templates.hpp`。自动测试位于 `tests.cpp`，共享接口位于 `api.hpp`。完成后构建并运行：

```powershell
cmake --build --preset dev
.\build\labs_starter.exe
```

Linux/macOS 可执行文件没有 `.exe`。每次运行会输出通过数量和具体失败项。不要在未理解失败原因时修改测试。

参考实现在 `solutions/`，关键判断与常见错误见 [answer_key.md](answer_key.md)。

## Lab 01：安全解析与问候

实现 `parse_integer`：

- 接受可选正负号。
- 至少有一位十进制数字，不接受空格、尾部字符或空串。
- 正确处理 `INT_MIN` 和 `INT_MAX`。
- 绝不通过可能溢出的整数乘法判断上界。

实现 `format_greeting`，把 `{"Ada", 36}` 变成：

```text
Hello, Ada! Age: 36
```

验收点：100 位数字的输入也必须安全返回空值。

## Lab 02：安全算术与用户名

实现 `safe_add`：

- 在加法前检查正向上溢和负向下溢。
- 失败时不依赖 `result` 的旧值。

实现 `is_valid_username`：

- 长度为 3 到 16。
- 只允许 ASCII 字母、数字和下划线。
- 不能以数字开头。

提示：`std::numeric_limits<int>::max() - rhs` 本身只有在条件保证不会溢出时才有意义。先判断 `rhs` 的符号。

## Lab 03：分支、循环与二分

实现 `fizzbuzz`、`is_prime` 和有序范围上的 `binary_search`。

`fizzbuzz` 的输入小于等于 0 时抛出 `std::invalid_argument`。`binary_search` 返回一个匹配位置，空范围和找不到返回 `std::nullopt`。

`is_prime(INT_MAX)` 的测试要求循环中不能把 `int` 乘法溢出。使用 `long long` 保存检查上界。

另外实现 `cleanup_scores(std::vector<int>)`：

- 保留 0 到 100（含边界）的分数。
- 删除负数和大于 100 的分数。
- 使用 C++20 的 `std::erase_if`，不要手写索引循环。
- 返回清理后的容器，并保持剩余元素的相对顺序。

完整原理和多容器示例见[第 3 章](../docs/03_control_flow_and_functions.md#安全方案-2用-stderase_if-批量删除)。

## Lab 04：借用序列

实现 `sum(std::span<const int>)` 和 `average(std::span<const int>)`。

- 空范围的 `average` 返回 `std::nullopt`。
- 返回值不能引用局部对象。
- 思考调用者把 span 保存超过原容器生命周期时会发生什么。

## Lab 05：不变量与账户

实现 `Temperature`：

- 存储摄氏度。
- 拒绝非有限值和低于 -273.15 的值。
- `fahrenheit()` 返回华氏温度。

实现 `BankAccount`：

- 初始余额不能为负。
- `deposit` 只接受正数，并防止余额溢出。
- `withdraw` 失败返回 `false`，余额不变。

练习重点不是异常本身，而是每个公开操作完成后对象都必须满足不变量。

## Lab 06：多态与所有权

实现 `Circle`、`Rectangle` 和 `total_area`。

- 尺寸必须有限且大于 0。
- 基类通过 `unique_ptr<Shape>` 拥有对象。
- 重写函数不切片，不在接口中使用裸拥有指针。

扩展：实现 `std::variant` 版本，比较动态分配和值语义的差异。

## Lab 07：算法和词频

实现 `top_words`：

1. 只保留 ASCII 字母、数字和下划线。
2. 大写字母转小写。
3. 规范化后为空的 token 忽略。
4. 按次数降序；次数相同时按单词升序。
5. 应用 `limit`。

思考：如果输入是一个完整文件而不是已分词数组，你会怎样处理标点和 Unicode？

## Lab 08：Concepts 与模板

在 `starter/templates.hpp` 中实现：

- `Addable`：`lhs + rhs` 合法且结果可转换为 `T`。
- `ArithmeticValue`：只允许算术类型。
- `add_values` 和 `square_value`。

不能只检查 `value + value` 合法就算完成，因为加法结果可能无法转回 `T`。Concept 应表达调用函数真正需要的合同。

## Lab 09：移动语义

实现 move-only `MoveOnlyBuffer`：

- 移动构造把元素所有权转移给目标。
- 移动后源对象大小为 0，并且仍可安全析构。
- 移动赋值先释放目标旧资源，再接管新资源。
- 自移动不破坏对象。

当前参考 API 使用 `std::vector<int>` 作为内部存储。完成后额外挑战：改成裸 `int*` + 长度，并手写 Rule of Five。

## Lab 10：错误建模与工厂

实现 `parse_nonnegative_int`：

- 空串返回 `ParseError::empty`。
- 非数字返回 `non_digit`。
- 超出 `int` 范围返回 `out_of_range`。
- 成功返回 `int`。

实现 `make_temperature`，返回 `unique_ptr<Temperature>`。构造合法性由 `Temperature` 保证，工厂不应返回半构造对象。

## Lab 11：Ranges 与 `std::format`

实现 `first_even_squares`：

- 过滤偶数。
- 计算平方。
- 只取前 `limit` 个。
- 返回拥有元素的 `std::vector<int>`。

实现 `format_price_row`：

```text
{name:<10}{quantity:>5} {total:>8.2f}
```

这里的冒号是格式说明的写法，不是输出中的字符。工具链不支持 `<format>` 时，在笔记中记录具体错误并说明替代方案。

## Lab 12：并发求和与停止令牌

实现 `parallel_sum`：

- 至少一个 worker。
- 空输入返回 0。
- 把索引区间分给多个 `std::jthread`。
- 每个线程只写自己的结果，避免无同步共享写入。
- 合并结果前确保所有线程已 join。

实现 `count_until_stopped`：

- 每次循环检查 `stop_token`。
- 已请求停止时返回 0。
- 不请求停止时最多运行 `maximum` 次。

不要用“我运行了一百次没出错”证明线程安全。

## Lab 13：可测试的领域对象

实现 `TodoList`：

- 空标题抛 `std::invalid_argument`。
- 添加合法标题。
- 删除时返回是否找到。
- 删除后大小正确更新。

思考：如果要支持重复标题，接口和删除语义需要怎样修改？当前 `remove` 只删除第一个匹配项。

## Lab 14：从协程直觉到状态机

实现 `SquareGenerator`：

- 第一次 `next()` 返回 0。
- 每次返回当前整数的平方。
- 到 `limit` 前返回 N 个值。
- 完成后继续调用返回 `std::nullopt`。

这个类不是 C++ 协程，而是与 `co_yield` 生成器行为等价的状态机。再阅读[第 14 章](../docs/14_modules_and_coroutines.md)，画出一个真正的协程状态图。

## 提交前检查

- `labs_solution` 全部通过。
- 你的 `labs_starter` 全部通过。
- 构建日志没有新警告。
- 能解释每个测试为什么需要，而不是只让代码通过。
- 至少用一种工具检查过越界或悬空访问。
