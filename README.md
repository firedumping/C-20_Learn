# C++20 从初学者到精通

这是一套面向 C++20 的系统课程，目标不是背语法，而是逐步建立四项能力：

1. 能准确解释对象的类型、值、生命周期和所有权。
2. 能用标准库、泛型与并发写出现代 C++，而不是写“带类的 C”。
3. 能处理异常安全、性能、可测试性和工具链问题。
4. 能独立设计和交付一个中型 C++20 项目。

课程默认读者已经会使用编辑器，但没有 C++ 基础。全程要求 C++20 模式；个别编译器可能尚未完整实现模块和协程，遇到时以当前工具链的成熟替代方案完成练习。

## 学习方式

每章都按同一顺序组织：

1. 先读“学习目标”，能用自己的话复述概念后再编码。
2. 手敲示例，不使用复制粘贴；对每段示例预测输出。
3. 完成章节末尾的练习。先只读需求，不看参考实现。
4. 完成 `labs/` 中对应练习，让程序自行检查结果。
5. 每章结束填写自测题；不能解释的条目就是下一轮复习重点。

建议每周投入 6 到 9 小时。核心课程通常需要 16 周，而不是 16 天；第 17～19 章是面向真实项目的进阶附录。

## 核心 16 周路线

| 周次 | 主题 | 教程 | 配套练习 |
|---|---|---|---|
| 1 | 工具链、编译模型、第一个程序 | [01](docs/01_getting_started.md) | Lab 01 |
| 2 | 类型、表达式、字符串与输入输出 | [02](docs/02_types_and_io.md) | Lab 02 |
| 3 | 控制流、函数、调试 | [03](docs/03_control_flow_and_functions.md) | Lab 03 |
| 4 | 作用域、对象生命周期、引用和指针 | [04](docs/04_lifetime_references_pointers.md) | Lab 04 |
| 5 | 类、不变量、RAII | [05](docs/05_classes_and_raii.md) | Lab 05 |
| 6 | 继承、多态与组合的取舍 | [06](docs/06_inheritance_polymorphism.md) | Lab 06 |
| 7 | STL 容器、迭代器、算法、lambda | [07](docs/07_stl_and_algorithms.md) | Lab 07 |
| 8 | 模板、类型推导与泛型设计 | [08](docs/08_templates_and_deduction.md) | Lab 08 |
| 9 | 值类别、移动语义、完美转发 | [09](docs/09_move_semantics.md) | Lab 09 |
| 10 | 异常安全、智能指针、错误建模 | [10](docs/10_errors_and_ownership.md) | Lab 10 |
| 11 | Concepts、Ranges、`<format>`、`<span>` 等 C++20 特性 | [11](docs/11_cpp20_features.md) | Lab 11 |
| 12 | 线程、`jthread`、原子操作、停止令牌 | [12](docs/12_concurrency.md) | Lab 12 |
| 13 | CMake、测试、检查工具、性能方法 | [13](docs/13_engineering.md) | Lab 13 |
| 14 | 模块与协程：原理、现状、选型 | [14](docs/14_modules_and_coroutines.md) | Lab 14 |
| 15 | 综合项目的架构与迭代 | [15](docs/15_capstone_guide.md) | Capstone 第 1、2 阶段 |
| 16 | 项目验收、性能复盘、掌握度评估 | [16](docs/16_mastery_checklist.md) | Capstone 第 3、4 阶段 |

## 进阶附录

建议在完成核心项目后追加约 2 周。每章都包含独立练习和验收问题。

| 顺序 | 重点 | 教程 | 对应缺口 |
|---|---|---|---|
| 17 | 多翻译单元、ODR、转换、对象表示、ABI | [17](docs/17_language_contracts.md) | 从单文件代码进入库和真实工程 |
| 18 | 文件、Unicode、严格解析、可移植二进制格式 | [18](docs/18_data_boundaries.md) | 输入输出与数据边界 |
| 19 | PMR、抽象选择、有界并发、测试与性能决策 | [19](docs/19_expert_decisions.md) | 从“会写”进入技术选型 |

## 目录

```text
.
├── docs/                 分章教程
├── labs/
│   ├── README.md         14 个练习的完整题目
│   ├── starter/          留空待实现的练习入口
│   ├── solutions/        参考实现和自动检查
│   └── answer_key.md     关键思路、常见错误和复盘答案
├── projects/
│   └── log_analyzer/     C++20 综合项目
├── CMakeLists.txt
├── CMakePresets.json
└── scripts/
```

## 环境要求

任选一套较新的工具链：

- Windows: Visual Studio 2022 17.10+，安装“使用 C++ 的桌面开发”。
- Windows/MinGW: GCC 13+ 或 LLVM/Clang 17+。
- Linux: GCC 13+ 或 Clang 17+。
- macOS: Apple Clang 16+ 或 Homebrew LLVM 18+。

同时安装 CMake 3.25+；预设文件使用 Ninja 生成器，因此还需要 Ninja 1.11+。如果使用其他生成器，可按文末的手工 CMake 命令构建。`std::format`、模块和协程的支持程度与标准库版本有关，参见[第 1 章](docs/01_getting_started.md)的兼容性说明。

## 构建

如果已经安装 Ninja，并激活了 Visual Studio 2022 的开发者环境，使用 Ninja 预设：

```powershell
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Windows 上也可以直接使用 Visual Studio 2022 生成器，不需要手工激活编译环境：

```powershell
cmake --preset dev-vs
cmake --build --preset dev-vs
ctest --preset dev-vs --output-on-failure
```

如果使用单配置生成器，也可以手工构建：

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

运行练习（Ninja 预设）：

```powershell
.\build\labs_starter.exe
.\build\labs_solution.exe
```

Visual Studio 预设的输出位于 `build-vs\Debug\`。Linux、macOS 或 WSL 下可执行文件没有 `.exe` 后缀，例如运行 `./build/labs_starter`。CMake 的配置输出会给出可执行文件位置。

GCC/Clang 下可用独立 preset 运行 ASan/UBSan：

```powershell
cmake --preset sanitize
cmake --build --preset sanitize
ctest --preset sanitize --output-on-failure
```

MSVC 的 sanitizer preset 启用 AddressSanitizer；UBSan 的可用性取决于工具链。

## 使用练习的正确方法

`labs/starter/labs.cpp` 中每个练习都是独立函数。阅读 `labs/README.md` 的验收条件后，只改动指定函数。完成后构建并运行 `labs_starter`。自动检查会报告首个失败项。

卡住时按以下顺序处理：

1. 写出输入、预期输出和最关键的不变量。
2. 用最小样例手工执行代码。
3. 开启 `-Wall -Wextra -Wpedantic -Wconversion` 或 MSVC `/W4`。
4. 使用调试器查看调用栈、变量值和对象生命周期。
5. 仍然无法定位时，再阅读 `labs/solutions/` 和 `labs/answer_key.md`。

参考实现不是唯一答案。如果选择不同设计，必须能说明所有权、错误处理、复杂度和可测试性。

## 通过标准

完成课程不是“看过全部章节”，而是同时满足：

- 14 个基础 Lab 全部通过，并使用 `-Wall -Wextra` 之类的高警告级别构建。
- 综合项目至少有稳定输入格式、自动化测试、错误处理路径和性能基线。
- 能解释每条关键资源的所有者、释放时机，以及异常出现时是否泄漏。
- 能给出至少一次算法和数据结构的复杂度分析。
- 能用调试器或 sanitizer 定位一个真实缺陷。
- 能不查资料写出 Concepts、Ranges、移动构造、异常安全的类和一个安全的线程示例。
- 能拆分多文件库接口，并解释 ODR、所有权、转换和二进制格式的合同。
- 能说明为什么选择某个容器、抽象方式或分配策略，而不只是写出可运行代码。

先从[第 1 章](docs/01_getting_started.md)开始。不要跳过所有权与生命周期章节；绝大多数“C++ 很难”的问题都来自这里。
