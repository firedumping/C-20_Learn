# 日志分析器

这是课程的 C++20 综合项目。实现按“解析、聚合、报告、CLI”分层，适合练习纯函数、所有权、错误建模、Ranges 和格式化。

## 输入格式

每行必须恰好包含六个以空格或制表符分隔的字段：

```text
2026-10-25T10:15:30Z INFO request completed status=200 duration_ms=37
```

- 时间戳必须是 `YYYY-MM-DDTHH:MM:SSZ`，年份不早于 1970。
- 级别必须是 `DEBUG`、`INFO`、`WARN` 或 `ERROR`。
- 状态码必须为 100 到 599。
- 耗时必须是非负十进制整数。
- 解析器验证公历日期、每月天数和闰年，但不解析时区或本地化文本。

## 构建与运行

在仓库根目录：

```powershell
cmake --preset dev
cmake --build --preset dev
.\build\projects\log_analyzer\log_analyzer.exe projects\log_analyzer\sample.log
```

严格模式：

```powershell
.\build\projects\log_analyzer\log_analyzer.exe --strict projects\log_analyzer\sample.log
```

Linux/macOS 下可执行文件名没有 `.exe`。CMake 生成器不同，实际输出目录可能略有不同。

## 错误码

| 退出码 | 含义 |
|---:|---|
| 0 | 成功 |
| 1 | 参数错误 |
| 2 | 文件无法打开或读取失败 |
| 3 | 严格模式发现无效行 |

默认模式会统计无效行并继续；严格模式仍会输出完整报告，但最终返回 3。

## 测试

```powershell
ctest --preset dev --output-on-failure
```

测试覆盖合法行、无效字段、历法与闰年、状态码、耗时、空输入、CRLF 流式读取、聚合和 CLI。

## 已知取舍

- 文件按行流式处理，不会把整份日志读入内存。
- 时间仍以字符串保存；会验证公历日期和闰年，但不处理时区换算。
- `std::format` 需要较新的标准库；若工具链不支持，先替换为格式测试和输出适配层，不要同时改动解析与领域模型。

## 建议扩展

1. 给流式读取增加最大行长和额外诊断上下文。
2. 使用 `std::chrono` 替换手工日期验证，并支持时区明确的统计桶。
3. 增加 JSON 输出，同时保持人类可读输出不变。
4. 对百万行日志建立基准，比较内存和时间。
5. 将 `ParseError` 转成带字段位置的诊断。
6. 在工具链支持时把 `analyzer` 与 `report` 重构为模块。
