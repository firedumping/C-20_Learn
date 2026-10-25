# 第 13 章：工程化：构建、测试、分析与性能

## 学习目标

- 用 CMake target 管理依赖和编译属性。
- 写出可重复、可诊断的自动化测试。
- 使用警告、sanitizer、静态分析定位常见缺陷。
- 用基准测试而不是直觉优化。
- 建立错误日志、输入验证和持续集成意识。

## 1. 项目结构

一个可维护的中型项目通常分离：

```text
project/
├── CMakeLists.txt
├── include/project/
│   └── public_api.hpp
├── src/
│   ├── domain.cpp
│   └── main.cpp
├── tests/
│   └── tests.cpp
└── benchmarks/
    └── benchmark.cpp
```

原则：

- `include/` 放公开头文件。
- `src/` 放实现和内部头文件。
- `tests/` 只通过公开接口验证行为，必要时测试内部实现。
- 一个 target 负责一个可独立构建的单元。
- 不把一个巨型 `all.cpp` 当作项目结构。

## 2. CMake target 设计

```cmake
add_library(log_core
    src/analyzer.cpp
    src/report.cpp
)

target_include_directories(log_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(log_core PUBLIC cxx_std_20)
target_link_libraries(log_core PRIVATE course_warnings)

add_executable(log_app src/main.cpp)
target_link_libraries(log_app PRIVATE log_core)

add_executable(log_tests tests/tests.cpp)
target_link_libraries(log_tests PRIVATE log_core)

add_test(NAME log_tests COMMAND log_tests)
```

`PUBLIC` 表示依赖和使用者都需要；`PRIVATE` 表示仅当前 target；`INTERFACE` 表示只传播给使用者。包含目录、宏和编译选项都尽量绑定 target。

不要在子目录中写：

```cmake
include_directories(${CMAKE_SOURCE_DIR}/include)
add_definitions(-DSOME_FLAG)
```

它们作用于当前目录及子目录中的 target，容易产生隐式耦合。

## 3. 测试金字塔

### 单元测试

验证一个函数或类的局部行为：

- 输入边界。
- 错误路径。
- 对象不变量。
- 资源是否释放。

### 集成测试

验证多个模块协作：

- 文件解析到报告生成。
- 网络请求到缓存更新。
- 并发队列的生产者/消费者。

### 端到端测试

验证完整命令行行为或用户流程。数量应少，因为它们慢且定位困难。

### 回归测试

每个发现过的真实缺陷都应有测试固定行为。测试名应描述条件和结果：

```cpp
void withdraw_more_than_balance_returns_false_and_keeps_balance()
void parse_empty_input_returns_nullopt()
```

## 4. 断言和测试的基本结构

本课程使用简单自定义检查，减少外部依赖：

```cpp
#include <iostream>
#include <string_view>

bool check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}
```

真实项目可以使用 Catch2、GoogleTest、doctest 等。选择框架后仍要遵守：

- 每个测试独立，不依赖执行顺序。
- 测试数据明确。
- 失败信息包含实际值和期望值。
- 不测试实现细节，除非细节是合同。
- 不依赖网络、系统时间和全局状态做脆弱测试。

### CTest 显示 `Not Run` 怎么排查

`Not Run` 通常不是断言失败，而是 CTest 找不到测试可执行文件。顺序应当是：

1. 运行 `cmake --build --preset dev --verbose`，先确认构建真正成功。
2. 如果 Ninja 在测试链接前失败，先修第一个编译/链接错误。
3. 检查 CTest 注册的命令路径和生成器输出目录。
4. 最后才运行 `ctest --preset dev --output-on-failure`。

两个容易遇到的 C++ 冲突：

- 全局命名空间 `labs` 会与 C 标准库函数 `::labs(long)` 冲突。
- 测试辅助函数叫 `equal` 时，标准库类型的实参可能通过 ADL 找到 `std::equal`，导致错误重载被选中。

诊断代码问题时保留编译器原始错误，而不是只看 CTest 的汇总。第一个编译错误通常才是根因，后续几十条错误可能都是它的级联结果。

## 5. 输入验证与日志

程序边界先验证：

```cpp
std::optional<Options> parse_options(int argc, char** argv) {
    if (argc != 2) {
        return std::nullopt;
    }
    Options options{};
    options.path = argv[1];
    if (options.path.empty()) {
        return std::nullopt;
    }
    return options;
}
```

日志应回答：

- 发生了什么？
- 哪个输入/对象/文件导致？
- 可执行的下一步是什么？
- 是否包含敏感信息？

不要只写：

```text
error
```

改写为：

```text
cannot open input file 'access.log': permission denied
```

对于库，优先返回可处理的错误；`std::cerr` 是应用层职责。不要把日志打印当作错误处理。

## 6. 编译器警告与 sanitizer

开发配置建议：

```bash
g++ -std=c++20 -O1 -g \
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
    -fsanitize=address,undefined \
    main.cpp -o app
```

或 Clang：

```bash
clang++ -std=c++20 -O1 -g \
    -Wall -Wextra -Wpedantic \
    -fsanitize=address,undefined \
    main.cpp -o app
```

MSVC 常用：

```bat
cl /std:c++20 /EHsc /W4 /permissive- /Zi /fsanitize=address main.cpp
```

常见 sanitizer：

- AddressSanitizer（ASan）：越界、use-after-free、部分泄漏。
- UndefinedBehaviorSanitizer（UBSan）：整数溢出、空指针等未定义行为。
- ThreadSanitizer（TSan）：数据竞争。
- MemorySanitizer（MSan）：未初始化读取（Clang 生态较常见）。

工具不是所有平台/编译器都可用，且 sanitizer 会降低速度。它们用于测试，不用于最终性能结论。

## 7. 静态分析

Clang-Tidy：

```bash
clang-tidy src/analyzer.cpp -- -std=c++20 -Iinclude
```

常用检查：

- 生命周期和悬空指针。
- 不必要的复制。
- 不安全的 C API 使用。
- 异常规范和虚函数问题。

MSVC：

```bat
cl /analyze /std:c++20 /EHsc src\analyzer.cpp
```

静态分析会产生建议，不应机械地一条条“消警告”。先判断提议是否改善语义，再修改。

## 8. 调试器工作流

1. 固定一个最小可复现输入。
2. 在关键路径设置断点。
3. 检查调用栈和对象生命周期。
4. 在变量发生变化的位置继续单步。
5. 检查条件变量、线程和锁的持有情况。
6. 修复后加入回归测试。

对于内存问题，先在 Debug + ASan 中运行；不要只看 Release 下的随机表现。

## 9. 性能分析

优化顺序：

1. 定义指标：延迟、吞吐、内存、启动时间。
2. 建立稳定基准。
3. 使用 profiler 找热点。
4. 修改一个变量。
5. 重跑相同基准。
6. 记录收益和回归。

错误做法是凭感觉改 `reserve`、`inline`、`std::move` 和容器类型。不同硬件、输入规模、编译选项会改变结论。

一个简单基准框架：

```cpp
#include <chrono>
#include <iostream>

template <class Fn>
void time_it(const char* name, Fn&& fn) {
    const auto start = std::chrono::steady_clock::now();
    fn();
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed =
        std::chrono::duration<double, std::milli>{end - start};
    std::cout << name << ": " << elapsed.count() << " ms\n";
}
```

## 10. 持续集成

CI 至少应：

- 在干净环境配置和构建。
- 运行单元测试。
- 使用高警告级别。
- 对支持的平台运行 sanitizer。
- 构建 Release，避免只在 Debug 下通过。
- 不把构建产物提交到仓库。

一个最小 GitHub Actions 风格流程会依次执行 `cmake --preset dev`、构建和 `ctest`。具体平台工具链由项目决定，关键是命令可重复、失败可见。

## 11. 代码评审清单

- API 是否表达所有权、可空性和错误方式？
- 是否有未初始化或越界访问？
- 失败路径是否保持不变量？
- 是否存在悬空引用/视图？
- 容器失效规则是否满足？
- 并发访问是否都有同步策略？
- 算法复杂度是否与数据规模匹配？
- 测试是否覆盖边界和回归？
- 格式化输出是否稳定？
- 是否有可删掉的抽象？

## 练习 1：建立测试

为一个小型 `TodoList` 类编写测试：

- 名称不能为空。
- 添加合法任务成功。
- 删除不存在的任务失败。
- 删除后再查询返回空。

先用最直接的自定义断言，不引入第三方框架。

## 练习 2：用 sanitizer 找问题

构造一个故意越界的程序，开启 ASan 运行，记录输出。然后修复它并确认测试仍通过。解释为什么“当前没有崩溃”不能证明程序正确。

## 练习 3：基准问题

对一个文本统计程序建立两个输入规模：

- 1,000 行。
- 1,000,000 行。

记录内存、时间和正确性。比较 `std::map` 与 `std::unordered_map` 的结果时确保输出顺序一致，否则性能差异可能混入了错误工作。

## 自测

1. CMake 的 `PUBLIC`、`PRIVATE`、`INTERFACE` 有什么区别？
2. 单元测试和集成测试分别适合验证什么？
3. ASan 能发现哪些问题？不能替代什么？
4. 为什么未经测量的优化不可靠？
5. 库函数应该打印错误还是返回错误？

## 下一步

[第 14 章](14_modules_and_coroutines.md)讨论两项影响 C++ 工程结构的 C++20 特性。
