# 第 1 章：工具链与第一个 C++20 程序

## 学习目标

学完本章，你应当能够：

- 说清源文件、翻译单元、目标文件和可执行文件之间的关系。
- 使用 GCC、Clang 或 MSVC 的 C++20 模式编译程序。
- 区分编译错误、链接错误和运行时错误。
- 用一个最小 CMake 项目构建并运行程序。
- 理解为什么现代 C++ 要把类型、所有权和错误处理一起设计。

## 1. 从文本到程序

C++ 程序不是“编辑器直接运行文本”。一次典型构建包含四个阶段：

1. **预处理**：展开 `#include`、处理宏和条件编译。
2. **编译**：每个源文件独立变成一个目标文件（`.o` 或 `.obj`）。
3. **汇编**：编译器后端把中间表示变成机器相关指令。
4. **链接**：链接器把目标文件和库连接成可执行文件。

一个 `.cpp` 文件连同它包含的所有头文件，经过预处理后形成一个翻译单元。不同源文件之间有声明和定义的配合关系：

```cpp
// math.hpp
int add(int a, int b);       // 声明：告诉编译器“存在这个函数”

// math.cpp
#include "math.hpp"
int add(int a, int b) {      // 定义：给出唯一实现
    return a + b;
}

// main.cpp
#include "math.hpp"
int main() {
    return add(2, 3) == 5 ? 0 : 1;
}
```

头文件通常只放声明和必要定义，不应放普通函数的定义。否则多个翻译单元包含它时，可能违反单一定义规则（ODR）。

### 三类常见错误

| 阶段 | 典型信息 | 含义 |
|---|---|---|
| 编译 | `expected ';'`、`no matching function` | 当前翻译单元无法通过语法或类型检查 |
| 链接 | `undefined reference`、`LNK2019` | 声明找到了，但没有对应定义 |
| 运行时 | 崩溃、错误结果、越界访问 | 程序已启动，但逻辑或内存状态错误 |

遇到错误时先看第一个错误，不要被后续级联错误淹没。编译器往往在真正问题之后才报出一串不相关错误。

## 2. 第一个 C++20 程序

```cpp
#include <iostream>
#include <string>

int main() {
    std::cout << "你的名字：";

    std::string name;
    if (!(std::cin >> name)) {
        std::cerr << "没有读到名字\n";
        return 1;
    }

    std::cout << "你好，" << name << "！\n";
    return 0;
}
```

逐项解释：

- `#include <iostream>` 引入控制台输入输出声明。
- `std::` 表示标准库命名空间，避免与其他库的 `cout`、`string` 冲突。
- `std::string` 自己管理字符数据的生命周期。
- `std::cin >> name` 读取一个以空白分隔的单词；表达式本身可以用于判断输入是否成功。
- `main` 返回 `0` 表示成功，非零值表示失败。

`using namespace std;` 在小型教学示例中常见，但真实项目通常避免它，尤其不要放在头文件的全局作用域。

## 3. 编译命令

### GCC / Clang

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion hello.cpp -o hello
./hello
```

或者：

```bash
clang++ -std=c++20 -Wall -Wextra -Wpedantic hello.cpp -o hello
```

建议在开发阶段启用警告，并尝试把警告视为错误：

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror hello.cpp -o hello
```

### MSVC

在 “x64 Native Tools Command Prompt for VS 2022” 中：

```bat
cl /std:c++20 /EHsc /W4 /permissive- /utf-8 hello.cpp
hello.exe
```

`/EHsc` 启用标准 C++ 异常模型。`/utf-8` 对含中文的源文件很重要。

只看编译器版本：

```bash
g++ --version
clang++ --version
cl
```

如果命令行找不到编译器，不等于编译器没安装。Windows 上应从开发者命令提示符启动，或配置 Visual Studio、MinGW、LLVM 的 `bin` 目录。

## 4. CMake 的最小模型

手写编译命令适合一个文件。多个源文件时，需要构建系统描述依赖：

```cmake
cmake_minimum_required(VERSION 3.25)
project(hello LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hello hello.cpp)

if(MSVC)
    target_compile_options(hello PRIVATE /W4 /permissive-)
else()
    target_compile_options(hello PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()
```

配置和构建：

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

关键概念：

- `target` 是构建系统的一等公民。警告、宏、包含目录最好绑定到 target。
- `PRIVATE` 表示使用要求不传播给依赖该 target 的其他 target。
- `INTERFACE` 表示该 target 本身不编译源文件，只向使用者传播要求。
- 不要用全局 `include_directories` 和 `add_definitions` 解决所有问题，长期会让依赖关系不可见。

本课程的根目录已经提供 `CMakeLists.txt` 和 `CMakePresets.json`。在根目录运行：

```powershell
cmake --preset dev
cmake --build --preset dev
```

## 5. C++20 兼容性要看“编译器 + 标准库”

C++20 是编译器前端和标准库共同实现的。看到 `-std=c++20` 能通过，不代表所有 C++20 库功能都存在。

重点检查：

- Concepts、三路比较和指定初始化器：主流新工具链通常可用。
- Ranges：GCC 13、Clang 17、MSVC 2022 通常较完整，但边界功能可能仍有差异。
- `std::format`：GCC 13+、Clang 17+、MSVC 2019 16.10+ 较可用；老版本常需额外库。
- 协程：语言机制存在，但生成器/任务库仍可能需要自己实现或选择第三方库。
- 模块：支持程度差异最大。教程会讲原理，但基础练习不强制依赖模块。

在程序中查看语言版本：

```cpp
#include <iostream>

int main() {
    if (__cplusplus >= 202002L) {
        std::cout << "C++20 language mode\n";
    } else {
        std::cout << "Not in C++20 mode: " << __cplusplus << '\n';
    }
}
```

MSVC 还需要 `/Zc:__cplusplus` 才会正确报告 `__cplusplus`。本课程 CMake 配置已经加上。

## 6. 调试器与开发循环

写 C++ 最有效的工作循环是：

1. 写出最小的可失败样例。
2. 编译并开启警告。
3. 用断言或测试固定预期行为。
4. 用调试器检查，而不是无限加 `cout`。
5. 修复后重跑完整测试。

Visual Studio Code 可安装 C/C++ 扩展并用 CMake Tools；Visual Studio 可以直接“打开文件夹”。关键是构建配置使用 Debug，并生成调试符号。

GDB 常用命令：

```text
break main
run
next
step
print variable
backtrace
continue
```

LLDB 常用命令：

```text
breakpoint set --name main
run
next
step
frame variable
bt
continue
```

## 7. 一个容易误解的示例

```cpp
#include <iostream>

int main() {
    int value = 7 / 2;
    double exact = 7.0 / 2;
    std::cout << value << ' ' << exact << '\n';
}
```

输出是：

```text
3 3.5
```

`7` 和 `2` 都是整数，所以整数除法先发生，结果才是 `3`。把任意一边变成浮点数，才会得到浮点除法。

这类问题不是“编译器有 bug”，而是类型规则在起作用。后续章节会系统解释类型、转换和对象生命周期。

## 练习 1：最小编译链

不使用 CMake，创建 `hello.cpp`，读取一个名字并输出：

```text
Hello, <名字>!
```

要求：

1. 使用 `std::string`，输入不超过 64 个非空白字符时正常工作。
2. 输入失败时向 `std::cerr` 输出错误并返回非零值。
3. 用 C++20 模式编译，打开全部常用警告。

## 练习 2：修复链接错误

把以下两个文件真正编译并链接。记录一次“只编译、不链接”的命令，以及一次完整链接命令。

```cpp
// answer.hpp
int answer();

// main.cpp
#include "answer.hpp"
#include <iostream>
int main() { std::cout << answer() << '\n'; }

// answer.cpp
#include "answer.hpp"
int answer() { return 42; }
```

如果你故意删除 `answer.cpp`，应得到链接错误而非编译错误。解释为什么。

## 练习 3：兼容性探测

写一个程序，打印以下内容并记录你的工具链结果：

- `__cplusplus` 的值。
- 是否能包含 `<format>`。
- 是否能包含 `<ranges>`。
- 是否能使用 `std::jthread`。

不要把“不能编译”当作学习失败；这是建立工具链认知的一部分。

## 自测

1. 为什么每个源文件可以独立编译？
2. 声明和定义的区别是什么？
3. `undefined reference` 最可能发生在哪个阶段？
4. 为什么不能把普通函数定义随便放进头文件？
5. `target_compile_options` 比全局编译选项好在哪？

## 下一步

进入[第 2 章：类型、表达式和输入输出](02_types_and_io.md)。从这一章开始，所有示例都尽量只依赖 C++20 标准库。
