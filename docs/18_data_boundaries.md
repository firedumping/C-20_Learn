# 第 18 章：文件、文本与二进制数据边界

## 学习目标

- 正确管理流状态、文件打开、读取和写入失败。
- 使用 `std::filesystem` 处理路径和目录，同时区分“查询失败”与“不存在”。
- 理解文本字节、字符集和 Unicode 的边界。
- 使用 `std::from_chars` 做严格数字解析。
- 设计显式、定长、字节序明确的二进制格式。
- 知道何时应使用成熟序列化库，而不是自定义协议。

## 1. 流是带状态的对象

流操作失败不会自动抛出异常；失败状态存储在流对象中。

```cpp
#include <fstream>
#include <string>

bool write_text(const std::string& path, const std::string& text) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output) {
        return false;
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.close();
    return static_cast<bool>(output);
}
```

关键状态：

- `good()`：目前没有失败标志。
- `fail()`：操作失败，例如格式错误或打开失败。
- `bad()`：底层 I/O 严重失败。
- `eof()`：读到输入末尾。

`eof()` 并不总意味着读取成功。循环结束后要区分“正常到达 EOF”和“因格式错误提前停止”。

### 读取文本行

```cpp
#include <fstream>
#include <iostream>
#include <string>

bool print_lines(const std::string& path) {
    std::ifstream input{path};
    if (!input) {
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        std::cout << line << '\n';
    }

    return input.eof() && !input.bad();
}
```

流对象采用 RAII：离开作用域自动关闭文件。不要依赖手动 `close()` 才能保证异常路径释放资源。

## 2. `std::filesystem`

`std::filesystem` 是 C++17 引入、C++20 项目仍常用的文件系统库：

```cpp
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

void list_files(const fs::path& directory) {
    std::error_code error;
    for (fs::directory_iterator it{directory, error}, end;
         !error && it != end;
         it.increment(error)) {
        if (it->is_regular_file(error)) {
            std::cout << it->path() << '\n';
        }
    }
    if (error) {
        std::cerr << error.message() << '\n';
    }
}
```

要点：

- `fs::path` 处理平台分隔符和路径拼接，不要手工用 `'/'` 拼接所有系统路径。
- 使用带 `std::error_code` 的重载来区分可恢复错误。
- `exists(path)` 为假不一定表示路径不存在；也可能是权限或 I/O 错误。
- 遍历目录时文件可能被其他进程修改，操作之间的状态不是事务。
- 删除、覆盖前先验证解析后的目标。递归删除不是普通错误处理练习。

### 原子或安全替换

常见模式是先写同目录临时文件，成功关闭后再替换目标：

```text
write data.tmp
flush/close and verify success
rename data.tmp -> data
```

但“替换已存在目标”在不同平台上语义不同。若程序要求崩溃安全的原子替换，必须根据目标平台验证 `rename` 行为、文件同步和目录同步。不要只根据函数名字猜测保证。

## 3. 文本、字节和 Unicode

标准 `char` 只是一个字节类型，不代表 Unicode 字符。`std::string` 是字节序列，UTF-8 是其中一种常见编码。

```cpp
const char8_t* utf8_text = u8"你好";
```

C++20 中 `u8` 字面量的元素类型是 `char8_t`，不能无条件当作 `const char*`。与普通 `std::string` 交互时需要明确转换边界：

```cpp
#include <string>

std::u8string utf8 = u8"你好";
```

需要区分：

- **字节长度**：`std::string::size()`。
- **码点数量**：Unicode scalar values 的数量。
- **用户可见字符**：可能由多个码点、组合符或 emoji 序列构成。

标准库没有完整提供 Unicode 规范化、字素簇分割和双向文本处理。一般业务应使用 ICU、utf8proc 等成熟库。不要用 `isalpha` 和固定长度截断来处理任意 Unicode 文本。

### 大小写与区域

`std::tolower(char)` 受字符集和区域设置影响。对 ASCII 协议字段可以明确只处理 `A-Z`；对自然语言文本则需要 Unicode 库和明确 locale 策略。不要在没有需求时 `setlocale` 修改整个进程行为。

## 4. 严格数字解析

`std::from_chars` 不分配、不依赖全局 locale，并能报告未消费字符：

```cpp
#include <charconv>
#include <optional>
#include <string_view>

std::optional<int> parse_int(std::string_view text) {
    int value = 0;
    const auto result =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}
```

要求“整段都是数字”时，必须检查 `ptr` 是否到达末尾。只检查 `ec` 会接受 `123abc` 的前缀。

`std::to_chars` 可用于无分配格式化；复杂对齐和人类可读报告仍可用 `std::format`。

## 5. 二进制格式必须有明确合同

不能直接把对象内存写进文件并期待跨平台读取。一个可移植记录必须定义：

- 固定宽度整数，例如 `std::uint32_t`。
- 每个字段的字节序。
- 长度字段的含义和最大值。
- 版本号。
- 校验或截断检测。
- 未知字段/未知版本的兼容策略。

下面的格式明确采用 little-endian：

```cpp
#include <cstdint>
#include <optional>
#include <span>
#include <string>

void append_u32_le(std::string& output, std::uint32_t value) {
    output.push_back(static_cast<char>(value & 0xFFu));
    output.push_back(static_cast<char>((value >> 8) & 0xFFu));
    output.push_back(static_cast<char>((value >> 16) & 0xFFu));
    output.push_back(static_cast<char>((value >> 24) & 0xFFu));
}

std::optional<std::uint32_t>
read_u32_le(std::span<const std::uint8_t> input) {
    if (input.size() < 4) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(input[0]) |
           (static_cast<std::uint32_t>(input[1]) << 8) |
           (static_cast<std::uint32_t>(input[2]) << 16) |
           (static_cast<std::uint32_t>(input[3]) << 24);
}
```

写入和读取必须使用同一个版本化合同。字符串则应先写长度，再写字节；读取长度后先验证剩余字节是否足够，防止恶意输入导致巨大分配。

### 什么时候用库

以下情况不要从零发明协议：

- 需要 JSON、YAML 或 XML 的标准语法。
- 需要向前/向后兼容、schema 演进和跨语言。
- 格式涉及压缩、加密、校验或复杂容器。
- 文件会由不可信来源提供。

可选择成熟的新版 JSON 库或 Protocol Buffers/FlatBuffers 等格式，并把第三方依赖封装在项目边界内。C++ 标准库主要提供字节和流，不提供通用 JSON 序列化。

## 6. 不可信输入

文件不是天然可信的。解析时必须限制：

- 最大行长度。
- 最大字段数和单字段长度。
- 最大记录数量。
- 长度字段对应的剩余字节。
- 整数解析范围和溢出。
- 路径是否被允许离开指定目录。

对不可信二进制数据，`reinterpret_cast` 到结构体指针后直接读取字段是未定义或实现定义行为，并可能触发对齐和生命周期问题。应按显式字节逐步解码。

## 练习 1：流式文本统计

实现 `analyze_file(path)`：

- 不把整个文件读入内存。
- 统计行数、最长行长度和无效 UTF-8 字节序列提示。
- 打开/读取失败要有不同错误码。
- 对空文件不除零。

基础版可只处理 ASCII；扩展版必须使用成熟 UTF-8/Unicode 库。

## 练习 2：可移植配置记录

定义版本化二进制格式：

```text
magic      4 bytes
version    u16 little-endian
name_size  u32 little-endian
name       name_size bytes (UTF-8)
score      i32 little-endian
```

实现写入和读取，并测试：

- 截断的首部。
- 声明的 `name_size` 超过剩余数据。
- 错误 magic。
- 超大长度。
- 正常 round-trip。

## 练习 3：目录清单

使用 `std::filesystem` 输出指定目录下普通文件的大小和路径：

- 符号链接策略必须明确。
- 权限错误不能当作空目录。
- 输出顺序稳定。
- 不跟随目录符号链接形成循环。

## 自测

1. `eof()` 和读取成功有什么区别？
2. 为什么不直接写 `reinterpret_cast<const char*>(&object)`？
3. `std::string::size()` 是否等于用户看到的字符数？
4. `from_chars` 为什么还要检查返回的 `ptr`？
5. 版本化和长度上限为什么是二进制协议的一部分？

## 下一步

[第 19 章](19_expert_decisions.md)整理分配、抽象、并发和工程选型方面的进阶知识。
