# 第 15 章：综合项目：日志分析器

## 1. 项目目标

在 `projects/log_analyzer/` 中实现一个小型命令行日志分析器。输入为文本日志，每行格式：

```text
2026-10-25T10:15:30Z INFO request completed status=200 duration_ms=37
2026-10-25T10:15:31Z WARN request completed status=503 duration_ms=812
garbage line
```

程序应输出：

- 总行数。
- 可解析日志数。
- 无效行数。
- 状态码计数。
- 平均请求耗时。
- 按小时统计的请求数。

项目的价值不在于功能规模，而在于系统地处理：

- 输入边界和错误。
- 所有权和生命周期。
- C++20 范围、格式化和结构化数据。
- 可测试的纯函数。
- CLI 错误码。
- 性能基线。

## 2. 架构

```text
main.cpp
  -> parse_cli
  -> read_lines
  -> parse_line (纯函数)
  -> aggregate
  -> report
```

建议模块：

- `log_record`：解析后的值对象。
- `parse_line`：文本到 `optional<LogRecord>` 或结果类型。
- `aggregate`：从记录集合计算统计。
- `report`：`std::format` 输出。
- `main`：处理文件、用户参数和最终错误码。

解析函数不打印日志，纯函数容易测试。应用层决定是否跳过坏行、输出警告或失败退出。

## 3. 数据模型

建议的起始结构：

```cpp
struct LogRecord {
    std::string timestamp;
    std::string level;
    int status{};
    int duration_ms{};
};

struct Statistics {
    std::size_t total_lines{};
    std::size_t valid_lines{};
    std::size_t invalid_lines{};
    std::map<int, std::size_t> status_counts;
    std::map<std::string, std::size_t> hourly_counts;
    double average_duration_ms{};
};
```

思考：

- `timestamp` 是否应保存为字符串？如果需要按真实时间排序/时区换算，应使用 `std::chrono` 解析；第一版可以保存时间字符串并在验证后提取小时。
- `duration_ms` 是否可能为负？解析时验证。
- 超大输入是否逐行处理？参考实现采用流式聚合，内存主要随状态数量而非总行数增长。

## 4. 分阶段交付

### 阶段 A：能正确解析

要求：

- 每行只接受定义明确的格式。
- 缺少字段、非法状态码、非数字耗时都算无效行。
- 解析函数不修改全局状态。
- 用表驱动测试覆盖合法和非法样例。

测试用例至少包括：

- 正常 200。
- 503。
- 非法状态码。
- 负耗时。
- 末尾多个空格。
- 空行。
- 完全无关的文本。

### 阶段 B：正确聚合

要求：

- 空输入不除零。
- 无效行不参与状态码和耗时统计。
- 小时统计基于已验证时间。
- 状态码映射排序稳定。

### 阶段 C：稳定 CLI

```text
log_analyzer [--strict] <path>
```

`--strict` 模式下，只要出现一行无效日志就返回错误；默认模式计入 `invalid_lines` 并继续。

错误码建议：

```text
0 成功
1 参数错误
2 文件无法打开/读取
3 严格模式下出现无效行
```

### 阶段 D：性能与质量

- 用 1,000,000 行合成数据测试。
- 参考实现已经逐行处理；可另写一个刻意全部读入内存的版本，用相同输入比较峰值内存。
- 运行高警告配置。
- 如果工具支持，运行 ASan/UBSan。
- 给出内存和时间基线，而不是只声称“很快”。

## 5. 解析器设计

不要过早使用正则表达式解决一切。固定字段格式可以先按空白分割：

```text
[timestamp] [level] request completed status=NNN duration_ms=NNN
```

核心检查：

1. 恰好 6 个 token，多一个或少一个都算格式错误。
2. token 1 是非空时间字符串（第一版可只检查长度/结构）。
3. token 2 是允许的日志级别。
4. token 3、4 是固定文本。
5. `status=` 后面是 100 到 599 的整数。
6. `duration_ms=` 后面是非负整数。
7. 不允许静默接受解析失败。

解析函数返回失败原因时，可以使用：

```cpp
struct ParseFailure {
    enum class Kind { wrong_field_count, bad_timestamp, bad_level, bad_status, bad_duration };
    Kind kind;
};

std::variant<LogRecord, ParseFailure> parse_line(std::string_view line);
```

这比只返回 `optional` 更适合学习错误建模。也可以设计自己的轻量 `Result<T>`，但不要复制一个半成品的 `std::expected` 带来隐藏缺陷。

## 6. 所有权设计

- `parse_line` 接收 `std::string_view`，只在调用期间读取。
- 返回的 `LogRecord` 拥有自己的 `timestamp` 和 `level`。
- 聚合器接收 `span<const LogRecord>` 或使用流式输入。
- 不要在 `LogRecord` 中保存指向输入缓冲区的 `string_view`，除非输入缓冲区存活时间明确更长。
- 统计结果按值返回，避免返回局部引用。

## 7. 测试策略

先测试纯函数：

```cpp
parse_line("") -> failure
parse_line(valid_line) -> record with status 200
aggregate(empty) -> valid zero report
aggregate(valid records) -> expected average and counts
```

然后测试 I/O 边界：

- 文件不存在。
- 文件存在但为空。
- 只有无效行。
- 严格模式遇到第一条无效行时是否停止。
- 非严格模式是否继续。

最后测试命令行：

- 缺少参数。
- 多余参数。
- 不支持的选项。
- 正常路径。

## 8. 报告设计

示例：

```text
Summary
  lines:   1000
  valid:   995
  invalid: 5

Status codes
  200: 900
  503: 95

Average duration: 42.37 ms

Requests by hour
  2026-10-25T10: 300
  2026-10-25T11: 695
```

输出格式应在 README 中固定，测试不要依赖大量易变文本。需要机器可读输出时，再增加 JSON/CSV，而不是反复改动人类可读格式。

## 9. 可选的 C++20 扩展

- 用 `std::span<const LogRecord>` 实现聚合接口。
- 用 Ranges 过滤和转换记录。
- 用 Concepts 约束报告器接口。
- 用 `std::format` 生成报告。
- 用 `std::jthread` 并行处理多个文件（只有在输入规模证明有收益时）。
- 用模块重构解析器和报告器（工具链支持时）。

## 10. 完成定义

项目至少应满足：

- 源码可通过 CMake 在 C++20 模式构建。
- 成功和失败路径都有测试。
- 错误信息包含具体原因。
- 没有依赖未定义行为通过测试。
- README 写明输入格式、构建、运行和错误码。
- 有至少一个性能基线。
- 能解释每个容器选择的复杂度。

## 自测

1. 解析器和 CLI 为什么应分离？
2. 为什么 `LogRecord` 不应默认保存 `string_view`？
3. 空输入的平均值如何处理？
4. 严格模式与非严格模式的错误语义如何区分？
5. 哪些优化必须基于实测数据？

## 下一步

[第 16 章](16_mastery_checklist.md)提供最终验收和掌握度评估。
