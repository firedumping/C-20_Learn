# 面向 C++20 的操作系统系统教程

> 配套项目：`EdgeBot：端侧设备控制与可观测性终端`  
> 建议位置：`/home/ring/C-20_Learn/career_bootcamp/02_edgebot_os_tutorial.md`  
> 版本：2026-10-28

## 学习前说明与假设

本教程面向已经会写小型 C++ 程序、但没有系统学习过操作系统的转行读者。最终目标是：**能用 C++20 编写包含多个线程、互斥量和条件变量的安全程序，并解释程序在操作系统中的基本运行过程。** 学完后，你不需要掌握内核源码，但应能解释 EdgeBot 中任务如何跨线程投递、队列为何需要同步、阻塞等待为何不持续占用 CPU，以及关闭时为何必须唤醒并汇合线程。

本文采用以下假设：

- 学习和运行环境是 WSL2 中的 Ubuntu 24.04，项目放在 Linux 文件系统中的 `/home/ring/C-20_Learn`，而不是较慢的 `/mnt/g` Windows 挂载目录。
- 已安装 GCC 13.3、GDB 13 或更新版本、CMake 3.28 和 Ninja 1.11。本文的 C++20 并发示例用 GCC 13.3 验证。
- 示例使用 Linux/POSIX 能力时会明确标注；纯 C++20 示例仍可能在 Windows 原生环境编译，但线程调度、内部实现和观察工具不同。
- `strace` 在当前环境中不是预装工具。涉及它时只作为可选观察手段，可先跳过，不影响学习并发主线。
- 现有 `02_edgebot_project.md` 是项目设计稿。本教程写成独立的 `02_edgebot_os_tutorial.md`，不覆盖原稿。

编译并发程序的基本命令是：

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic -pthread source.cpp -o program
./program
```

`-pthread` 是 GCC/Clang 在 Linux 上链接线程支持时常用的编译选项；它不是 C++ 标准规定的语法。示例输出中的线程交错顺序可能不同，这不表示示例“不稳定”，只有违反代码声明的输出条件才是错误。

## 目录

0. [学习地图](#第-0-章学习地图)  
1. [问题引入：日志为什么丢了](#第-1-章问题引入日志为什么丢了)  
2. [从源文件到进程](#第-2-章从源文件到进程)  
3. [线程、栈与调度](#第-3-章线程栈与调度)  
4. [数据竞争与互斥量](#第-4-章数据竞争与互斥量)  
5. [条件变量与阻塞队列](#第-5-章条件变量与阻塞队列)  
6. [事件循环、系统调用与文件描述符](#第-6-章事件循环系统调用与文件描述符)  
7. [取消、超时与优雅关闭](#第-7-章取消超时与优雅关闭)  
8. [综合实践：MiniEdge](#第-8-章综合实践miniedge)  
9. [验收清单](#第-9-章验收清单)  
10. [附录：术语、对照、排错与答案](#附录术语表)

## 第 0 章：学习地图

**学习目标与时间**：建立全局地图，能分别指出“语言、编译器/标准库、操作系统、硬件”负责什么。预计 0.5～1 小时。  
**前置知识**：会编译并运行一个 C++ 程序。

你最终要完成的能力不是背操作系统名词，而是能对自己的并发代码回答四个问题：

1. 哪些数据会被多个线程访问，谁拥有它？
2. 哪些操作必须互斥，线程之间靠什么建立先后关系？
3. 线程在等待时，CPU 和内核大致处于什么状态？
4. 程序退出时，哪些对象仍有等待者，如何唤醒和汇合？

依赖关系如下：

```mermaid
flowchart LR
    A[源文件到进程] --> B[线程与调度]
    B --> C[数据竞争与互斥量]
    C --> D[条件变量与阻塞队列]
    D --> E[事件循环与系统调用]
    E --> F[取消、超时与关闭]
    F --> G[EdgeBot MiniEdge]
```

本章先给出几个贯穿全文的直觉。之后每章首次使用术语时仍会重新解释。

| 术语 | 第一次接触时的解释 | 以后可继续深入 |
|---|---|---|
| 操作系统（Operating System, OS） | 管理 CPU、内存、文件和设备，并向程序提供受控服务的软件系统 | 内核结构、设备驱动 |
| 进程（Process） | 运行中的程序实例，拥有独立的虚拟地址空间和资源集合 | 进程创建、隔离、信号 |
| 线程（Thread） | 进程内可被调度执行的一条控制流；同一进程的线程通常共享地址空间 | 线程本地存储、调度策略 |
| 虚拟内存（Virtual Memory） | 程序看到的地址空间，由 OS 和 CPU 映射到物理内存 | 页表、换页、缺页异常 |
| 栈（Stack） | 随函数调用自动增长和回收的一块内存区域，常用于局部变量和调用信息 | 栈帧布局、栈溢出 |
| 堆（Heap） | 运行期间按需申请、需要明确生命周期管理的动态内存区域 | 分配器、碎片、映射 |
| 系统调用（System Call） | 用户程序请求内核执行受限操作的入口，例如读写文件、创建线程 | 中断、权限检查、兼容层 |
| 调度（Scheduling） | OS 决定哪个可运行线程下一步使用 CPU 的过程 | 调度策略、优先级反转 |
| 上下文切换（Context Switch） | 保存一个执行流的必要状态，再恢复另一个执行流的状态 | 寄存器、地址空间切换成本 |
| 缓存（Cache） | CPU 中比主存更快但更小的存储层，缓存主存内容以降低访问延迟 | 缓存一致性、伪共享 |
| 文件描述符（File Descriptor, FD） | Linux 中表示已打开文件、终端、管道或套接字等资源的整数句柄 | 复用、关闭语义、`epoll` |

每章结束后的能力是：第 2 章解释程序如何成为进程；第 3 章安全创建和汇合线程；第 4 章消除数据竞争；第 5 章实现有界阻塞队列；第 6 章解释并实现带优先级的事件循环；第 7 章让等待中的程序可停止；第 8 章把知识组合成 EdgeBot 的核心切片。

## 第 1 章：问题引入：日志为什么丢了

**学习目标与时间**：用现象引出并发与 OS 问题，不要求立即修好。预计 1 小时。  
**前置知识**：能读懂 C++ 类、函数和容器。

假设 EdgeBot 收到传感器数据后，一个线程把数据放进 `std::vector`，另一个线程读取并写入日志。开发时程序“跑起来没问题”，偶尔却出现日志只记录了一半、序号跳跃，甚至进程崩溃。于是有人加了互斥量，程序又偶尔卡在退出阶段。

你此时可能会有疑问：C++ 代码明明按行写了，为什么运行结果还能乱？`std::cout` 打印正常，为什么数据仍然可能丢失？线程 A 已经通知线程 B，为什么 B 还在等？

先把程序分成四层观察：

```text
C++ 源码语义
   ↓ 编译器、链接器
可执行文件
   ↓ OS 装载、创建栈和地址空间
进程中的线程
   ↓ 调度器、系统调用、同步原语
内核与硬件的实际执行
```

C++ 标准规定语言层面的线程、互斥量和条件变量应具有什么语义；标准库实现负责把它们落到本机接口；Linux 负责创建可调度实体、让等待线程睡眠并在资源可用时唤醒；CPU 负责真正执行指令。代码中如果有两个线程不受同步保护地读写同一对象，C++ 标准会把这种**数据竞争（Data Race）**视为未定义行为（Undefined Behavior, UB）。UB 的意思是标准不规定结果，不是“本次恰好输出正确就合法”。

本章案例暂时不展开修复，只留下三个判断：

- “运行一次正确”只是观测结果，不是安全检查。
- 加锁只能解决它保护到的数据；条件是否成立、何时通知仍要单独设计。
- 线程结束和对象析构有顺序要求；漏掉等待会让程序在退出时行为不受控。

**本章总结**：并发问题同时涉及语言规则、标准库同步工具和 OS 调度，不能只看源码行的先后。  
**自测题**：1. 为什么一次正确运行不能证明程序没有数据竞争？2. 哪一层最可能决定两个线程谁先运行？

## 第 2 章：从源文件到进程

**学习目标与时间**：能解释编译、链接、装载、进程、虚拟内存、栈、堆、系统调用和 FD。预计 3 小时。  
**前置知识**：会使用 `g++` 运行单个源文件。

### 从问题出发

只写 `hello.cpp` 时，程序为什么能知道终端在哪里？局部变量放在哪里？两个进程能否直接写同一个物理地址？只观察 C++ 源码无法回答这些问题。

### 概念与操作系统背景

编译把 C++ 源文件变成目标文件；链接把目标文件和标准库等组合成可执行文件；OS 启动程序时创建**进程**并装载代码。进程看到的地址不是物理内存条上的绝对地址，而是**虚拟地址**。虚拟内存让每个进程拥有相对独立的地址视图，并允许 OS 按页管理保护、映射和共享。

常见区域如下。具体布局、地址和大小由平台、编译选项和运行时决定：

| 区域 | 典型内容 | 生命周期边界 |
|---|---|---|
| 代码区 | 机器指令、只读常量 | 通常随进程 |
| 全局/静态区 | 全局对象、静态对象 | 静态存储期 |
| 栈 | 函数局部变量、调用信息 | 随作用域/调用返回；线程有各自栈 |
| 堆 | `new` 或标准库为动态对象取得的内存 | 由程序或 RAII 管理 |

**系统调用**是用户态程序进入内核的受控入口。`read`、`write`、`open`、`close` 是 Linux/POSIX 接口；C++ 标准库的 `std::ifstream` 最终也会依赖某个实现提供的系统能力，但标准不规定它内部一定调用哪一个系统调用。**文件描述符**是 Linux 进程表中指向打开资源的非负整数。它不一定是具体文件，也可能是终端、管道或套接字。

### 最小可运行示例

这是 Linux 专用观察示例。`/proc/self/status` 是 Linux 提供给当前进程的虚拟文件。

```cpp
// process_observe.cpp
#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

int main() {
    int on_stack = 7;
    auto on_heap = std::make_unique<int>(9);

    std::cout << "pid=" << ::getpid() << '\n';
    std::cout << "stack-object=" << static_cast<const void*>(&on_stack) << '\n';
    std::cout << "heap-object=" << static_cast<const void*>(on_heap.get()) << '\n';

    const int fd = ::open("observer.txt",
                          O_CREAT | O_WRONLY | O_TRUNC,
                          0644);
    if (fd < 0) {
        std::perror("open");
        return 1;
    }
    std::cout << "file-descriptor=" << fd << '\n';

    constexpr char text[] = "os";
    if (::write(fd, text, 2) != 2) {
        std::perror("write");
        ::close(fd);
        return 1;
    }
    ::close(fd);

    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.starts_with("VmSize:") ||
            line.starts_with("VmRSS:") ||
            line.starts_with("Threads:")) {
            std::cout << line << '\n';
        }
    }
}
```

构建并运行：

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic -c process_observe.cpp -o process_observe.o
g++ process_observe.o -o process_observe
./process_observe
```

预期能看到一个正整数 `pid`、两个地址、一个小整数 FD，以及若干 `VmSize`、`VmRSS`、`Threads: 1` 行。地址每次可能变化，FD 也通常是小整数但没有固定保证。

### 程序发生了什么

第一步，编译器把源码变成目标文件；第二步，链接器生成可执行文件；第三步，OS 创建进程、建立地址空间并跳转到程序入口；第四步，C++ 运行时完成必要初始化，再调用 `main`。栈对象随 `main` 作用域生存；`make_unique` 从堆取得内存，并在 `unique_ptr` 析构时释放。`open` 返回 FD，`write` 让内核把数据写到对应资源，`close` 释放该进程对 FD 的引用。

`strace -f -e trace=process,openat,close,write ./process_observe` 可观察系统调用；若系统未安装，可执行 `sudo apt install strace`。不要根据 `strace` 的输出推断 C++ 标准要求的内部实现，它展示的是当前 Linux/标准库实现的一条路径。

### 完整用法与边界

真实 EdgeBot 的串口只是“能用 FD 表示的设备”一类例子。业务层应依赖 `Transport` 接口，Linux 适配器再调用 `open`、`read`、`write`、`close`。`read` 返回 0 或错误时，业务不能假定下一次读取一定成功。C++ 标准保证对象生存期和 RAII 析构语义；FD 编号、`/proc`、`unistd.h`、串口 API 均依赖平台。

### 常见错误与排查

| 现象 | 可能根因 | 排查 |
|---|---|---|
| 返回指针后局部变量乱码 | 栈对象已离开作用域 | ASan、检查所有权 |
| FD 越来越多 | 错误路径或异常路径没有关闭 | 检查 RAII、`/proc/<pid>/fd` |
| 文件内容只写了一半 | 忽略 `write` 的短写或错误 | 检查返回值，循环处理允许部分写的接口 |
| 地址与另一台机器不同 | ASLR、布局和平台不同 | 不把地址数值当作跨平台事实 |

**语言与 OS 边界**：对象生命周期由 C++ 规则决定；实际地址、栈大小、页映射、FD 编号和系统调用序列由实现与 OS 决定。  
**本章总结**：程序文件本身不是进程；运行时 OS 给进程提供独立地址视图和资源表，程序通过系统调用使用外部资源。  
**自测题**：1. 可执行文件与进程有什么不同？2. 虚地址为什么不能直接当作物理内存地址？

## 第 3 章：线程、栈与调度

**学习目标与时间**：能创建、汇合 C++20 线程，解释共享地址空间、独立栈、调度和上下文切换。预计 2.5 小时。  
**前置知识**：第 2 章的进程、虚拟内存、栈、系统调用。

### 从问题出发

EdgeBot 需要同时接收传感器数据并执行定时任务。若每件事都串行等待，接收会拖慢控制；若随意创建线程，又可能让数据、对象生命周期和关闭顺序失控。线程到底共享什么，又独有什么？

### 概念

**线程**是进程内可被 OS 调度执行的控制流。同一进程的线程通常共享代码、全局数据、堆和文件描述符；每个线程有自己的调用栈、寄存器和调度状态。**调度器**从可运行线程中选择下一个执行者；发生**上下文切换**时，OS 和 CPU 保存/恢复必要状态。切换时间、选择顺序和优先级规则不是 C++ 标准能保证的内容。

CPU **缓存**是为了缩短访问主存时间而设置的高速存储。即使两个线程看到同一虚拟地址，物理缓存和可见性仍要通过语言同步规则协调。所谓“线程 A 刚写过，B 一定马上看到”不是可靠假设；正确程序用互斥量、条件变量或原子操作建立同步关系。

### 最小可运行示例

```cpp
// two_threads.cpp
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

using namespace std::chrono_literals;

std::mutex output_mutex;

void say(const std::string& name) {
    for (int i = 0; i < 3; ++i) {
        {
            std::scoped_lock lock(output_mutex);
            std::cout << name << " step " << i << '\n';
        }
        std::this_thread::sleep_for(2ms);
    }
}

int main() {
    std::jthread sensor(say, "sensor");
    std::jthread command(say, "command");
}
```

构建：

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic -pthread two_threads.cpp -o two_threads
./two_threads
```

预期共 6 行，但两步之间的顺序不固定。可能出现 `sensor step 0`、`command step 0`、`command step 1`…… 同一行的内容不会互相穿插，因为完整输出受到互斥量保护；行的全局顺序没有保证。

### 程序发生了什么

`std::jthread` 创建一个 C++ 线程对象并启动关联执行流。两个执行流都执行 `say`；它们在互斥量保护下输出。`sleep_for` 使当前线程至少请求暂停一段时间，但实际何时继续运行仍受调度影响。`jthread` 析构时若仍可汇合，会请求停止并等待执行流结束；本示例函数没有检查 `stop_token`，所以停止请求不会提前中断循环，但析构仍会等待任务自然完成。若改用 `std::thread`，必须在析构前显式 `join()` 或 `detach()`。

在常见 Linux/NPTL 实现中，标准线程会以内核可调度的执行实体出现；用 `ps -L -p <pid>` 可看线程 ID，`strace -f` 可看到创建和同步相关系统调用。这些是可观察的实现细节，不是 C++ 标准承诺“每次一定调用某个固定系统调用”。

### 完整用法与 OS 背景

EdgeBot 常采用“生产者线程投递、事件循环线程拥有业务状态”的结构。生产者线程需要独占的栈和局部状态，但仍通过共享队列与事件循环通信；共享队列必须同步。多线程不会自动提高速度：线程创建、调度、锁竞争和缓存通信都有成本。CPU 密集任务也不应阻塞唯一的事件循环线程。

### 常见错误与排查

| 现象 | 根因 | 修复/观察 |
|---|---|---|
| 程序启动后立即 `terminate` | 可汇合的 `std::thread` 在析构时未 `join`/`detach` | 优先用 `jthread`，或显式 `join` |
| 每次输出顺序不同就断言错误 | 把调度顺序当成语言保证 | 只验证允许的集合或稳定不变量 |
| 线程越来越多但任务没变快 | 频繁创建、锁竞争或 I/O 阻塞 | 统计线程数、队列长度和耗时 |
| 主线程返回后任务访问对象 | 对象生命周期短于线程 | 让所有者等待所有使用它的线程 |

**语言与 OS 边界**：C++20 规定 `std::thread`/`std::jthread` 的接口和可汇合规则，以及同步操作建立的可见性；OS 决定实际调度顺序、线程栈大小和底层创建方式。  
**本章总结**：线程共享进程资源，但拥有独立执行状态和栈；安全并发依赖同步和生命周期设计，不依赖“谁看起来先写”。  
**自测题**：1. 同进程两个线程通常共享哪些资源？2. `sleep_for(2ms)` 是否保证两天后一定按原顺序继续？

## 第 4 章：数据竞争与互斥量

**学习目标与时间**：识别数据竞争，使用 RAII 锁保护不变量，理解锁竞争与死锁。预计 3 小时。  
**前置知识**：第 3 章线程、调度、可见性。

### 从问题出发

两个线程各把计数器加 100000 次，最后为什么可能不是 200000？把变量类型改成 `long` 或逐次输出中间值通常仍不能修复问题。

### 错误示例及解释

```cpp
// broken_counter.cpp
#include <iostream>
#include <thread>

long counter = 0;

void add_many() {
    for (int i = 0; i < 100000; ++i) {
        ++counter;
    }
}

int main() {
    std::thread first(add_many);
    std::thread second(add_many);
    first.join();
    second.join();
    std::cout << counter << '\n';
}
```

`++counter` 不是不可分割的一次完整业务操作。两个线程可能都读到旧值，再分别写回新值；并且没有同步的并发读写已经构成数据竞争，属于 UB。程序可能输出 200000，也可能输出较小的数，甚至发生其他无法据标准推断的结果。**一次输出 200000 不能证明修复。**

用 ThreadSanitizer（TSan）观察：

```bash
g++ -std=c++20 -O1 -g -pthread -fsanitize=thread broken_counter.cpp -o broken_counter_tsan
./broken_counter_tsan
```

TSan 通常报告 `data race` 并指出两侧读写位置。精确报告格式依编译器版本变化，不要把某一版本文本当成固定标准输出。若当前 WSL2 内核直接运行时报 `FATAL: ThreadSanitizer: unexpected memory mapping`，可只对这次测试关闭地址随机化：`setarch "$(uname -m)" -R ./broken_counter_tsan`。验证环境中，错误程序甚至打印了 `200000`，TSan 仍报告竞争，这正说明单次结果不能证明正确。

### 正确写法与完整示例

**互斥量（Mutex）** 是同一时刻只允许一个线程进入受保护区的同步对象。互斥量保护的往往不只是一个字段，而是一组必须共同成立的条件，称为**不变量**。例如“总接收数增加时，总字节数也必须一起增加”，这两项应在同一次临界区内更新。

```cpp
// safe_counter.cpp
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

class Counter {
public:
    void add(long amount) {
        std::scoped_lock lock(mutex_);
        value_ += amount;
    }

    long value() const {
        std::scoped_lock lock(mutex_);
        return value_;
    }

private:
    mutable std::mutex mutex_;
    long value_ = 0;
};

int main() {
    Counter counter;
    std::vector<std::jthread> workers;
    for (int thread_number = 0; thread_number < 4; ++thread_number) {
        workers.emplace_back([&counter] {
            for (int i = 0; i < 100000; ++i) {
                counter.add(1);
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }
    std::cout << counter.value() << '\n';
}
```

构建并运行，稳定输出 `400000`。`scoped_lock` 在构造时锁定、在作用域结束时解锁，即使中间抛异常也能释放。读取 `value_` 同样需要锁，否则一个线程无同步读取、另一个线程写入，仍然是数据竞争。`mutable` 允许 `const` 成员函数给锁对象加锁，而逻辑上被保护的值仍通过只读接口暴露。

锁的边界应尽量小，但不能小到破坏不变量。不要在持锁时调用未知回调、长时间 I/O 或等待其他线程，否则其他线程可能被拖住。若一个线程先锁 A 再锁 B、另一个线程先锁 B 再锁 A，就可能形成**死锁（Deadlock）**：双方都持有对方下一步需要的锁。C++17 起可用 `std::scoped_lock lock(a, b)` 一次锁多个互斥量，标准库会采用避免这类循环等待的算法。

### 语言、标准库与 OS 边界

C++ 标准规定：互斥量的成功加锁/解锁建立同步顺序，并规定相关数据竞争行为；它不保证哪个等待线程先获得锁，也不保证锁实现一定直接阻塞线程。Linux 上的标准库可能先短暂自旋再通过内核等待，具体策略依实现和场景。你应依据 `std::mutex` 的语义编写代码，不能依赖某次 `futex` 调用序列。

### 常见错误与排查

| 错误 | 现象 | 修复 |
|---|---|---|
| 只给写加锁、不给读加锁 | TSan 报告混合读写竞争 | 所有共享访问使用同一同步协议 |
| 用锁保护多个字段，却在锁外读其中一项 | 看到部分更新 | 锁住完整不变量 |
| 持锁调用回调，回调再次锁同一互斥量 | 非递归互斥量死锁 | 回调移到锁外，或重新设计接口 |
| 用两把锁但顺序相反 | 偶发永久卡住 | 固定顺序或 `scoped_lock` |
| 把 `volatile` 当线程同步 | 竞争仍存在 | 用 mutex/atomic，按语义选择 |

**本章总结**：数据竞争是 UB，不是可接受的随机结果；互斥量保护的是完整不变量，锁内只做必要工作。  
**自测题**：1. “加锁后不会执行逻辑错误”是否成立？2. 为什么读取共享计数器时也通常要加锁？

## 第 5 章：条件变量与阻塞队列

**学习目标与时间**：用互斥量和条件变量实现有界阻塞队列，正确处理等待谓词、通知与关闭。预计 4 小时。  
**前置知识**：第 4 章锁、不变量、死锁。

### 从问题出发

如果消费者反复轮询“队列空吗”，线程会持续醒来占用 CPU。若消费者调用 `sleep`，生产过快会丢数据，生产过慢又会延迟。怎样让消费者在没有工作时睡眠，有工作时被唤醒？

### 概念与错误方式

**条件变量（Condition Variable）** 是让线程等待“某个条件可能已变化”的同步工具。它本身不保存数据，也不是消息队列。等待者应持有与共享数据关联的互斥量，并在被唤醒后重新检查谓词，即描述等待条件的表达式。标准允许等待在没有对应通知时返回，这称为**虚假唤醒（Spurious Wakeup）**；它也是必须使用谓词循环的原因之一。

典型错误一：只检查一次。

```cpp
if (queue.empty()) {
    condition.wait(lock);   // 被唤醒后没有重新检查
}
auto value = queue.front(); // 此时队列可能仍为空
```

典型错误二：修改条件时没有使用等待者使用的互斥量。这样通知可能发生在等待者检查条件之后、真正睡下之前，造成**丢失唤醒**。正确模式是：数据与条件在同一把锁下更新，修改后通知，等待者在持锁状态下用谓词判断。

本示例还用了 `std::atomic<long long>` 汇总多个消费者的结果。**原子操作（Atomic Operation）** 让对同一个原子对象的更新不可分割；`memory_order_relaxed` 在这里只用于取得最终总数，而程序结束后通过 `join` 汇合。原子对象适合独立计数，不会自动保护“队列长度”和“关闭状态”这类组合不变量，后两者仍由互斥量保护。

### 完整可运行示例

```cpp
// bounded_queue_demo.cpp
#include <atomic>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

template <class T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {}

    bool push(T value) {
        std::unique_lock lock(mutex_);
        changed_.wait(lock, [this] {
            return closed_ || items_.size() < capacity_;
        });
        if (closed_) {
            return false;
        }
        items_.push_back(std::move(value));
        changed_.notify_one();
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock lock(mutex_);
        changed_.wait(lock, [this] {
            return closed_ || !items_.empty();
        });
        if (items_.empty()) {
            return std::nullopt;
        }
        T value = std::move(items_.front());
        items_.pop_front();
        changed_.notify_one();
        return value;
    }

    void close() {
        std::scoped_lock lock(mutex_);
        closed_ = true;
        changed_.notify_all();
    }

private:
    std::size_t capacity_;
    std::deque<T> items_;
    bool closed_ = false;
    std::mutex mutex_;
    std::condition_variable changed_;
};

int main() {
    BoundedQueue<int> queue(4);
    std::atomic<long long> total{0};

    std::vector<std::jthread> producers;
    for (int producer = 0; producer < 2; ++producer) {
        producers.emplace_back([&queue, producer](std::stop_token stop) {
            for (int i = 0; i < 50 && !stop.stop_requested(); ++i) {
                if (!queue.push(producer * 100 + i)) {
                    return;
                }
            }
        });
    }

    std::vector<std::jthread> consumers;
    for (int consumer = 0; consumer < 2; ++consumer) {
        consumers.emplace_back([&queue, &total] {
            while (auto value = queue.pop()) {
                total.fetch_add(*value, std::memory_order_relaxed);
            }
        });
    }

    for (auto& thread : producers) {
        thread.join();
    }
    queue.close();
    for (auto& thread : consumers) {
        thread.join();
    }

    std::cout << total.load() << '\n';
}
```

构建并运行：

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic -pthread bounded_queue_demo.cpp -o bounded_queue_demo
./bounded_queue_demo
```

稳定输出 `7450`，即 `0..49` 与 `100..149` 的总和。各个元素被哪个消费者取走没有保证，因此不要断言处理顺序。

### 程序发生了什么

`push` 在队列已满时等待；`pop` 在队列为空时等待。两处都把锁和谓词交给 `condition_variable`。等待期间，库会释放互斥量，使其他线程能修改队列；被唤醒并确认谓词后，再重新持有锁才返回。这个协作避免消费者空转，同时防止在检查条件和睡眠之间丢失通知。

`close()` 持锁把 `closed_` 设为真并唤醒全部等待者。`push` 看到关闭后失败；`pop` 先消费剩余元素，队列为空后才返回 `nullopt`。这明确实现了“关闭后不再接收，但排队的消费者能排空”的语义。真实事件循环必须选择并测试自己的关闭策略，不能默认所有库都采用这一策略。

### 常见错误与排查

| 现象 | 常见原因 | 检查方法 |
|---|---|---|
| 消费者永远不醒 | 修改谓词没持同一把锁，通知丢失 | 画出“检查—等待—修改—通知”时序 |
| 偶发访问空容器 | 用 `if` 而不是谓词等待 | 改用带 predicate 的 `wait` |
| `close` 后所有线程卡住 | 只修改标志、没有 `notify_all` | 检查等待者和通知路径 |
| 只能收到一次队列满变化 | 每次状态变化后只通知一部分且条件不匹配 | 区分 `notify_one` 与 `notify_all` 的语义 |
| 析构后崩溃或卡死 | 对象销毁时仍有线程等待/持有锁 | 先停止投递、关闭队列、`join` 全部线程 |

**语言与 OS 边界**：C++ 标准规定条件变量的等待、通知、谓词和虚假唤醒处理契约；OS 通常让等待线程进入睡眠并在通知后变为可运行，但被唤醒后何时真正获得 CPU 和锁仍由调度决定。  
**本章总结**：条件变量必须与共享状态和互斥量一起使用；用谓词等待，关闭时唤醒，并在所有等待者退出后再销毁同步对象。  
**自测题**：1. 条件变量是否自己存储“有任务”这个状态？2. 为什么 `close()` 之后消费者仍可能继续工作一段时间？

## 第 6 章：事件循环、系统调用与文件描述符

**学习目标与时间**：解释阻塞等待、就绪通知和单线程事件循环；实现有界、带优先级、可停止且可检测队列满的任务循环。预计 5 小时。  
**前置知识**：第 3～5 章。

### 从问题出发

EdgeBot 的 `post()` 会从传感器、UI 或网络线程调用。若事件循环线程休眠等待新任务，外部线程怎样唤醒它？若队列无限增长，过载时会发生什么？若任务回调长期持锁，其他投递者会怎样？

### 概念与 OS 背景

**阻塞等待**表示当前线程暂时不能继续，OS 可把它移出可运行集合，CPU 便不必持续执行它。**非阻塞 I/O** 表示操作当前无法立即完成时返回“暂不可用”；Reactor 风格程序会用文件描述符和就绪通知机制等待多个 I/O 来源。Linux 的 `epoll` 是常见实现，但 `epoll` 不是 C++ 标准接口。EdgeBot MVP 可以先使用虚拟传输和条件变量任务队列，再接真实 FD I/O；两者可以组合，不要误以为“单线程”就必须用 `epoll` 或不能用 mutex。

事件循环的回调线程可以只有一个，但系统中仍可有生产者线程。业务状态由循环线程独占，其他线程只投递任务，可以显著减少共享状态数量。代价是：任何慢回调都会阻塞后续任务，高优先级任务可能让普通任务长期得不到执行，称为**饥饿（Starvation）**。

### 最小示例：线程安全的事件循环

把下面内容保存为 `event_loop.hpp`。

```cpp
#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <utility>

class EventLoop {
public:
    using Task = std::function<void()>;
    enum class Priority { normal, high };
    enum class PostStatus { accepted, full, stopped };

    explicit EventLoop(std::size_t capacity) : capacity_(capacity) {}
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    PostStatus post(Task task, Priority priority = Priority::normal) {
        {
            std::scoped_lock lock(mutex_);
            if (!accepting_) {
                return PostStatus::stopped;
            }
            if (normal_.size() + high_.size() >= capacity_) {
                return PostStatus::full;
            }
            auto& queue = priority == Priority::high ? high_ : normal_;
            queue.push_back(std::move(task));
        }
        changed_.notify_one();
        return PostStatus::accepted;
    }

    void request_stop() {
        {
            std::scoped_lock lock(mutex_);
            accepting_ = false;
        }
        changed_.notify_all();
    }

    void run() {
        for (;;) {
            std::deque<Task> batch;
            {
                std::unique_lock lock(mutex_);
                changed_.wait(lock, [this] {
                    return !accepting_ || !high_.empty() || !normal_.empty();
                });

                if (high_.empty() && normal_.empty()) {
                    return;
                }
                auto& ready = high_.empty() ? normal_ : high_;
                batch.swap(ready);
            }

            for (auto& task : batch) {
                try {
                    task();
                } catch (const std::exception& error) {
                    std::cerr << "task exception: " << error.what() << '\n';
                } catch (...) {
                    std::cerr << "task exception: unknown type\n";
                }
            }
        }
    }

private:
    std::size_t capacity_;
    std::deque<Task> normal_;
    std::deque<Task> high_;
    bool accepting_ = true;
    std::mutex mutex_;
    std::condition_variable changed_;
};
```

下面是同目录下完整的 `event_loop_demo.cpp`。

```cpp
#include "event_loop.hpp"

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

int main() {
    EventLoop loop(16);
    std::jthread runner([&loop] { loop.run(); });

    std::jthread normal([&loop](std::stop_token stop) {
        for (int i = 0; i < 5 && !stop.stop_requested(); ++i) {
            loop.post([i] {
                std::cout << "normal " << i << '\n';
            });
            std::this_thread::sleep_for(1ms);
        }
    });

    std::jthread high([&loop](std::stop_token stop) {
        for (int i = 0; i < 5 && !stop.stop_requested(); ++i) {
            loop.post([i] {
                std::cout << "high " << i << '\n';
            }, EventLoop::Priority::high);
            std::this_thread::sleep_for(1ms);
        }
    });

    normal.join();
    high.join();
    loop.request_stop();
    runner.join();
}
```

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic -pthread \
  event_loop_demo.cpp -o event_loop_demo
./event_loop_demo
```

预期共有 10 行，`normal 0..4` 与 `high 0..4` 各出现一次。高优先级只在两者都已位于队列时优先；生产者提交任务的实际先后受调度影响，所以不能断言所有 `high` 行都绝对先于所有 `normal` 行。

### 逐层解释

`post` 在锁内检查停止标志和总容量，再用同一把锁修改对应队列。这样队列长度和是否接受新任务不会被并发修改撕裂。`notify_one` 放在解锁后，被唤醒的循环线程会重新检查谓词。`run` 每次在锁内只交换一批任务，锁外执行回调；因此回调调用 `post` 时不会遇到自己已持有、又试图再次获取的同一互斥量。回调中新投递的任务进入下一轮，避免当前批次无限扩张。

`run` 在停止后仍会处理已排队任务，两个队列都空时才返回。这个语义必须写进项目 README。若业务要求“停止立即丢弃”，可以改变实现，但应同时明确已接收任务、在途任务和调用者的责任，并通过测试固定行为。

从 OS 角度看，线程在条件不满足时可以睡眠；另外的线程调用 `post` 后通过唤醒机制使其可运行。具体是否直接调用某个 `futex` 操作由标准库实现决定。事件循环线程若执行阻塞 `read`，就无法同时处理其他任务；真实 I/O 适配器需要考虑非阻塞、超时或专门的 I/O 等待层。

### 常见错误与排查

| 现象 | 根因 | 排查 |
|---|---|---|
| 外部线程投递后无人醒来 | 修改队列未通知，或谓词写错 | 检查每次状态变化和关闭路径 |
| 偶发死锁 | 持锁执行回调，回调再次 `post` | 只复制/交换队列，回调必须锁外执行 |
| 某个线程长期没机会运行 | 高优先级任务持续到达 | 指标记录普通任务等待时间，设计配额/老化 |
| 内存持续增长 | 队列无界或没有背压 | 有界队列、返回 `full`、计数并上报 |
| 关闭后仍接受任务 | 停止检查不在锁内，或状态分散 | 将接受状态与队列操作放在同一同步协议下 |
| 回调异常终止循环 | 没有任务级异常隔离 | 捕获并记录，同时测试循环继续运行 |

**语言、标准库与 OS 边界**：队列容量、优先级和每轮批次属于你自己的设计契约；mutex 与条件变量属于 C++ 标准同步工具；线程如何睡眠和唤醒、FD 如何就绪属于运行时与 OS 实现。C++ 标准没有“OS 实时优先级”语义。  
**本章总结**：单线程事件循环是所有权设计，不等于全程序只有一个线程；跨线程投递必须有同步，回调应在锁外执行，过载必须有明确策略。  
**自测题**：1. 为什么不能在持有 `EventLoop::mutex_` 时执行回调？2. 高优先级是否等于严格的全局实时优先？

## 第 7 章：取消、超时与优雅关闭

**学习目标与时间**：使用 C++20 停止令牌、可取消条件等待和超时，避免退出时卡死或访问已销毁对象。预计 3 小时。  
**前置知识**：第 5～6 章。

### 从问题出发

在一个函数里设置 `stopping = true`，等待线程为什么不醒？为什么主函数返回后，一个正在等待队列的线程可能崩溃？`jthread` 的 `request_stop()` 是不是强制终止线程？

### 概念与最小示例

`std::jthread` 的 `request_stop()` 是**协作式取消**：它设置停止状态并调用已注册的停止回调，但是否退出由线程自己检查 `std::stop_token`。C++20 还允许 `condition_variable_any` 等待停止令牌。以下程序在等待期间收到停止请求后会退出。

```cpp
// cancellable_wait.cpp
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

struct Shared {
    std::mutex mutex;
    std::condition_variable_any changed;
    bool has_work = false;
    bool closed = false;
};

void worker(Shared& shared, std::stop_token stop) {
    std::unique_lock lock(shared.mutex);
    const bool has_work = shared.changed.wait(
        lock, stop, [&shared] {
            return shared.closed || shared.has_work;
        });

    if (!has_work || stop.stop_requested()) {
        std::cout << "worker stopped\n";
        return;
    }
    std::cout << "worker received work\n";
}

int main() {
    Shared shared;
    std::jthread thread([&shared](std::stop_token stop) {
        worker(shared, stop);
    });
    thread.request_stop();
    thread.join();
}
```

构建运行后稳定输出 `worker stopped`。`wait(lock, stop, predicate)` 返回谓词结果；停止请求可以使等待结束，但仍不要在无锁状态下直接读业务状态。

这里用 lambda 接收停止令牌，是因为 `jthread` 会向可调用对象注入令牌；把参数顺序藏在函数指针构造里容易混淆。`std::stop_token` 本身没有可中断睡眠接口，`this_thread::sleep_for` 通常要等睡眠返回后才检查停止，所以长等待应改用可停止条件等待或拆成较短等待。

### 完整应用规则

关闭顺序建议固定为：

1. 事件源停止产生新任务。
2. 请求外部线程停止，并让它们使用停止令牌离开等待。
3. `join` 生产者，确保不会再有新任务进入。
4. 关闭队列，唤醒消费者；按声明策略决定是否排空剩余任务。
5. `join` 所有等待过的线程。
6. 最后销毁互斥量、条件变量、队列和它们保护的对象。

超时等待可用 `wait_for` 或 `wait_until`，同样检查谓词。例如等待 200 毫秒：

```cpp
const bool ready = queue_changed.wait_for(
    lock, std::chrono::milliseconds(200),
    [&] { return closed || !items.empty(); });
```

真实测试不应总依赖真实等待数秒。可把时钟和截止时间抽成可注入接口，或者把“判断是否超时”的逻辑提取成纯函数测试；`sleep_for(200ms)` 只能证明“可能通过”，不能证明超时后的状态机准确。

### 常见错误与排查

| 错误 | 后果 | 正确方向 |
|---|---|---|
| 以为 `request_stop()` 强杀线程 | 线程继续等待或运行 | 在循环和等待谓词中响应 stop token |
| 先销毁对象再 `join` | 等待者访问已销毁对象 | 先停止、唤醒、join、再销毁 |
| `join` 前一直持锁 | 等待线程无法拿锁退出 | 释放锁后 join |
| 关闭只唤醒一个线程 | 其他等待者永久停留 | 关闭通常应 `notify_all` |
| 取消时直接丢锁读状态 | 产生新数据竞争 | 保持原有同步协议 |

**语言与 OS 边界**：C++20 规定 `stop_token`、`stop_source`、`jthread` 和可停止等待的语义；OS 线程通常没有能被标准 API 安全强杀的 C++ 对象生命周期概念。停止是协作协议。  
**本章总结**：停止请求必须被等待条件和循环观察；资源销毁必须晚于所有可能访问它的线程；关闭策略必须明确并测试。  
**自测题**：1. `request_stop` 能保证线程立刻结束吗？2. 为什么通常先 join 再销毁队列？

## 第 8 章：综合实践：MiniEdge

**学习目标与时间**：在 EdgeBot 场景中组合多个线程、互斥量、条件变量、有界队列、优先级和关闭。预计 8～10 小时。  
**前置知识**：第 2～7 章，并已能运行 `event_loop.hpp`。

### 需求分析

实现 `MiniEdge`，不包含真实串口和协议解析，只验证并发骨架：

- 一个事件循环线程独占设备会话状态。
- 传感器生产者投递普通心跳和遥测任务。
- 控制生产者投递普通命令和一次急停高优先级任务。
- 总队列有上限，满时返回 `full`，生产者计数后继续或结束，不无限重试。
- 所有诊断输出由事件循环执行，避免多线程直接写同一业务状态。
- 生产者停止并汇合后，停止并排空事件循环，最后读取汇总。

暂不实现定时器、串口 FD 和非阻塞 I/O。它们可以在骨架稳定后加入；当前目标是证明你真正理解线程所有权和安全等待。

### 设计选择

业务状态 `Session` 只由 lambda 在事件循环线程中修改。生产者虽然持有 `Session&` 以便让任务捕获，但自身代码不读写它。生产者与事件循环之间唯一的同步通道是 `EventLoop::post`；该函数内部用 mutex 保护队列，用条件变量唤醒等待者。主线程只在事件循环 `join` 后读取 `Session`，此时“前一线程全部操作完成”的顺序已建立。

队列满时采用明确的“拒绝并上报”，不偷偷扩容、不阻塞生产者、不无限重试。高优先级任务共享当前容量是一个已知限制：它可以插队，但普通任务占满队列时，急停仍可能被拒绝。生产版 EdgeBot 应为急停保留独立容量或通道，并测试普通任务洪泛时急停仍可进入。

### 实现

将上一章的 `event_loop.hpp` 作为同目录头文件，再创建下面的 `edgebot_mini.cpp`。

```cpp
#include "event_loop.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

using namespace std::chrono_literals;

enum class DeviceState { offline, online, emergency };

struct Session {
    DeviceState state = DeviceState::offline;
    bool emergency_latched = false;
    int accepted = 0;
    int telemetry = 0;
    int heartbeats = 0;
    int commands = 0;
    int emergencies = 0;
};

EventLoop::PostStatus submit(EventLoop& loop,
                             std::atomic<int>& rejected,
                             EventLoop::Task task,
                             EventLoop::Priority priority) {
    const auto result = loop.post(std::move(task), priority);
    if (result == EventLoop::PostStatus::full) {
        rejected.fetch_add(1, std::memory_order_relaxed);
    }
    return result;
}

int main() {
    EventLoop loop(4);
    Session session;
    std::atomic<int> rejected{0};

    std::jthread loop_thread([&loop] { loop.run(); });

    std::jthread sensor([&loop, &session, &rejected](std::stop_token stop) {
        for (int i = 0; i < 20 && !stop.stop_requested(); ++i) {
            const auto task = [&session, i] {
                ++session.accepted;
                ++session.telemetry;
                if (i % 3 == 0) {
                    ++session.heartbeats;
                    if (!session.emergency_latched) {
                        session.state = DeviceState::online;
                    }
                }
                std::this_thread::sleep_for(2ms); // 模拟短暂处理
            };
            if (submit(loop, rejected, task,
                       EventLoop::Priority::normal) ==
                EventLoop::PostStatus::stopped) {
                return;
            }
        }
    });

    std::jthread controller([&loop, &session, &rejected](std::stop_token stop) {
        for (int i = 0; i < 6 && !stop.stop_requested(); ++i) {
            const auto command = [&session, i] {
                ++session.accepted;
                ++session.commands;
                std::cout << "command " << i << '\n';
            };
            submit(loop, rejected, command,
                   EventLoop::Priority::normal);

            if (i == 3) {
                const auto emergency = [&session] {
                    ++session.accepted;
                    ++session.emergencies;
                    session.state = DeviceState::emergency;
                    session.emergency_latched = true;
                    std::cout << "EMERGENCY\n";
                };
                submit(loop, rejected, emergency,
                       EventLoop::Priority::high);
            }
            std::this_thread::sleep_for(3ms);
        }
    });

    sensor.join();
    controller.join();
    loop.request_stop();
    loop_thread.join();

    std::cerr << "telemetry=" << session.telemetry
              << " heartbeats=" << session.heartbeats
              << " commands=" << session.commands
              << " emergencies=" << session.emergencies
              << " accepted=" << session.accepted
              << " rejected=" << rejected.load()
              << " state=" << static_cast<int>(session.state) << '\n';
}
```

构建：

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -pthread \
  edgebot_mini.cpp -o edgebot_mini
./edgebot_mini
```

不同调度下接受的准确数量可能不同，这是有界队列和并发调度的可观察差异。必须保持的条件是：所有已接受任务最终执行；`accepted` 等于 `telemetry + commands + emergencies`；急停被接受时最终状态是 `emergency`；没有生产者绕过 `post` 直接修改 `Session`；程序能自行退出。输出到标准错误的一行便于与业务日志区分。

### 运行验证与故障注入

1. 把容量改为 1，反复运行，观察拒绝数变化，确认生产者不会卡死。
2. 在传感器任务中临时移除 `sleep_for`，观察普通任务洪泛；验证急停共享容量时可能被拒，这正是生产设计必须改进的地方。
3. 将传感器 lambda 中 `++session.commands` 移到 `loop.post` 之前，用 TSan 构建观察数据竞争；然后改回任务内执行。
4. 删除 `loop.request_stop()`，观察循环线程继续等待、主程序不能正常结束；恢复后验证能退出。
5. 让某个任务抛 `std::runtime_error`，增加一个投递在它后面的任务，确认异常被记录后循环仍处理后续任务。

TSan 构建建议：

```bash
g++ -std=c++20 -O1 -g -Wall -Wextra -pthread \
  -fsanitize=thread edgebot_mini.cpp -o edgebot_mini_tsan
./edgebot_mini_tsan
```

若遇到上面的 TSan 映射错误，用 `setarch "$(uname -m)" -R ./edgebot_mini_tsan` 重跑。TSan 与 AddressSanitizer 通常应分开构建。Sanitizer 通过也不能证明所有逻辑条件正确，还需要确定性的语义测试。

### 综合验收标准

- 能从新终端用给定命令完成构建和运行。
- 能解释普通任务和高优先级任务的批次顺序，以及为什么输出先后仍受调度影响。
- 能指出哪把锁保护哪个队列、条件变量等待什么谓词。
- 能解释 `Session` 的线程所有权，以及主线程为何在 `join` 后读取它安全。
- 能说明队列满时系统的可见行为，并给出至少一条指标。
- 能用 TSan 复现一次人为数据竞争，再修复。
- 能说明当前共享容量对急停不合理，以及后续独立预留通道的方案。
- 能按“现象—复现—假设—证据—根因—修复—回归—遗留风险”写出一次复盘。

## 第 9 章：验收清单

逐项只标记“可以独立完成”或“仍需复习”。不能独立讲出原因时，不应仅因示例能运行就标记完成。

| 验收项 | 达到标准 |
|---|---|
| 程序运行过程 | 能区分源文件、可执行文件、进程、线程、虚拟地址 |
| 内存基础 | 能解释栈、堆、地址空间和 FD，不把地址值当跨平台常量 |
| 线程 API | 能正确使用 `std::jthread`，说明停止令牌为何是协作式 |
| 数据竞争 | 能指出无同步共享访问的 UB，并用 TSan 验证修复 |
| 互斥量 | 能围绕不变量设计临界区，避免持锁回调与锁顺序死锁 |
| 条件变量 | 能写出带谓词的等待，处理虚假唤醒、通知和关闭唤醒 |
| 事件循环 | 能解释锁外执行回调、当前批次、容量上限和停止排空策略 |
| 操作系统 | 能解释系统调用、阻塞等待、调度和上下文切换的基本作用 |
| 边界意识 | 能区分 C++ 标准、标准库/运行时、Linux 实现和硬件结果 |
| 故障排查 | 能从现象选择日志、TSan、GDB 或系统调用观察手段 |
| EdgeBot 综合 | 能完成 MiniEdge，并说明线程所有权、背压和急停容量限制 |
| 面试表达 | 能在两分钟内讲清一次真实并发故障及修复证据 |

## 附录：术语表

| 术语 | 简明定义 | 与当前项目的关系 |
|---|---|---|
| 数据竞争 | 多线程无同步地访问同一内存位置且至少一侧为写 | 必须用锁或原子操作排除 |
| UB | C++ 标准不规定行为的错误操作集合 | “这次正常”不是正确性证据 |
| 临界区 | 受互斥量保护的代码范围 | 维护队列与业务不变量 |
| 不变量 | 程序在临界区外应始终满足的条件 | 决定哪些更新必须一起加锁 |
| 逻辑竞争 | 没有 UB 但结果依赖合法线程交错 | 需明确业务顺序或接受任意合法顺序 |
| 虚假唤醒 | `wait` 没有对应通知也可能返回 | 必须用谓词重检 |
| 背压 | 系统超载时明确拒绝、阻塞或降级 | 有界队列满时返回 `full` |
| 饥饿 | 某线程长期得不到执行机会 | 高优先级洪泛需指标与配额 |
| 原子操作 | 对单个原子对象提供不可分割及规定内存顺序的操作 | 不能自动保护多个字段不变量 |
| 停止令牌 | C++20 中协作传递停止请求的对象 | 让阻塞生产者可退出 |

## 附录：易混淆概念对照

| 容易混淆 | 左边是什么 | 右边是什么 | 关键区别 |
|---|---|---|---|
| 可执行文件 / 进程 | 磁盘上的程序映像 | OS 中正在运行并拥有资源的实例 | 同一程序可产生多个进程 |
| 进程 / 线程 | 地址空间与资源集合 | 进程中的一条控制流 | 线程通常共享地址空间 |
| C++ 线程 / OS 线程 | 语言对象与执行接口 | 内核可调度实体（具体实现相关） | 常见实现接近映射，但标准不规定实现细节 |
| 栈 / 堆 | 调用相关的自动区域 | 动态分配区域 | 生存期和所有权规则不同 |
| 互斥量 / 条件变量 | 排他访问 | 等待条件变化 | 条件变量不保存业务数据 |
| 阻塞 / 空转 | 等待者可让出 CPU | 反复检查消耗 CPU | 条件变量优于忙等常见方案 |
| 系统调用 / 标准库函数 | 内核接口 | 可移植语言接口封装 | 标准库内部可依赖平台实现 |
| 优先级 / 实时保证 | 队列内的业务选择 | OS 调度承诺 | 本教程不提供全局实时保证 |

## 附录：常见错误速查

| 症状 | 优先检查 | 建议工具 |
|---|---|---|
| 结果偶尔错误、崩溃位置怪异 | 共享数据是否全部受同一同步协议保护 | TSan、GDB、最小复现 |
| 卡在退出 | 是否有等待者未被唤醒或未 `join` | GDB 线程栈、日志阶段标记 |
| 死锁 | 锁内回调、锁顺序、持锁 I/O | 锁顺序文档、最小双线程复现 |
| 条件变量漏数据 | 用 `if` 或修改谓词未持锁 | 改写为 predicate `wait` |
| CPU 一直很高 | 轮询、忙等、队列处理过密 | `top`/`pidstat`、采样占比 |
| 队列内存上涨 | 无界队列或没有背压 | 队列长度、拒绝计数 |
| 回调抛异常后服务停止 | 缺少任务异常隔离 | 日志、投递后续测试任务 |
| 只在一台机器出现 | 调度、库实现、构建选项或数据竞争 | 同环境对照、Sanitizer |

## 附录：练习参考答案

以下答案先给结论，再解释原因。实际工程还应有自动测试。

**第 2 章练习**

1. 基础题：进程拥有独立虚拟地址空间和资源；可执行文件是静态文件。解释：进程由装载和运行时初始化后产生，能执行系统调用并持有 FD。
2. 改错题：若代码把 `new` 的地址返回给调用者，却由局部 `unique_ptr` 析构释放，返回指针会悬空。应明确转移所有权，例如返回 `std::unique_ptr<int>` 或确保对象生存期覆盖使用期。
3. 应用题：用一个 RAII 类包装 `open/close`；检查 `open` 失败、测试正常与抛异常路径，并用 `/proc/<pid>/fd` 观察关闭后 FD 消失。

**第 3 章练习**

1. 基础题：线程共享代码、全局数据、堆、FD；拥有独立栈、寄存器和调度状态。
2. 改错题：`std::thread t(work);` 离开作用域前既未 `join` 也未 `detach` 会触发终止。优先改 `std::jthread`；若需控制关闭，显式 `request_stop()` 后 `join()`。
3. 应用题：两个线程向不同结果位置写局部结果，主线程 `join` 后合并，或让它们通过同步队列交回结果。不要让主线程在 `join` 前无同步读取结果。

**第 4 章练习**

1. 基础题：数据竞争是无同步并发访问并触发 UB；逻辑竞争是即使同步正确，业务结果仍依赖合法交错。互斥量只消除部分数据竞争，不一定规定业务先后。
2. 改错题：给 `counter` 的所有读和写使用同一 `std::mutex`，用 `scoped_lock`；若只做单个独立计数且确定需要原子语义，可用 `std::atomic<long>`，但不能用它自动保护多个字段的联合不变量。
3. 应用题：把“接收数”和“字节数”的更新放在同一临界区；不要在锁内调用可能 `post` 或阻塞的回调。

**第 5 章练习**

1. 基础题：条件变量不存队列内容，只负责等待/唤醒；真实条件由互斥量保护的变量表示。
2. 改错题：把一次性的 `wait` 改成 `wait(lock, predicate)`，并把每次可能导致谓词变化的修改放在同一把锁内，随后通知。
3. 应用题：`close` 持锁设置关闭标志并 `notify_all`；消费者先取完剩余元素，空队列且关闭后退出；生产者关闭后返回失败。

**第 6 章练习**

1. 基础题：每轮锁内交换一批，锁外执行，所以回调中新投递的任务进入下一轮；这限制了重入和本批次无限增长。
2. 改错题：把回调移出锁外。持锁回调若再次 `post` 会尝试获取同一非递归互斥量，形成死锁；慢回调还会阻塞所有生产者。
3. 应用题：普通队列满时拒绝并计数；急停使用独立预留容量或专用通道，并增加洪泛测试证明急停仍可进入。

**第 7 章练习**

1. 基础题：`request_stop` 只设置协作停止状态，线程仍需在循环或等待谓词中观察 `stop_token`。
2. 改错题：先停止生产、唤醒并 `join` 所有等待线程，再销毁队列和条件变量；否则可能唤醒不存在或已部分销毁的对象。
3. 应用题：用 `wait_for` 返回值和谓词共同判断结果；测试“先满足条件”和“超时先发生”两类路径，不依赖真实长睡眠。

**第 8 章练习**

1. 基础题：多生产者仅通过线程安全的 `post` 交接任务；`Session` 只由事件循环线程在任务中修改；主线程在 `join` 后读取。
2. 改错题：生产者直接执行 `++session.commands` 会与事件循环并发读写，产生数据竞争。应把修改封装到任务，或增加统一的同步接口。
3. 应用题：至少测试队列满、任务异常、停止排空、急停容量和无生产直接停止；用 TSan 验证同步路径，用不变量断言验证逻辑。

## 附录：自测题答案

1. 一次运行正确不能覆盖所有交错和优化结果，数据竞争仍属于 UB。
2. OS 调度器决定可运行线程何时真正执行；C++ 标准不承诺具体顺序。
3. 可执行文件是静态映像；进程是 OS 创建并管理的运行实例。
4. 虚拟地址是进程的地址视图，由 OS 与 CPU 映射到物理内存，不保证等于物理地址。
5. 同进程线程通常共享代码、全局数据、堆和 FD，但各有栈、寄存器和调度状态。
6. `sleep_for` 只请求至少等待一段时间，实际恢复时机仍受调度影响。
7. 加锁消除其所覆盖访问的数据竞争，但不自动解决业务原子性、顺序和死锁。
8. 无锁并发读写共享值仍是数据竞争；读取也必须遵循同一同步协议。
9. 条件变量不存储队列，条件由被互斥量保护的数据表达。
10. 关闭后消费者可先排空已接收元素，空队列且关闭时才退出；策略由实现决定并需测试。
11. 持锁执行回调可能让回调再次 `post` 时自锁，并阻塞其他线程。
12. 高优先级仅是本队列的选择规则，不等同于 OS 实时优先级或永远先执行。
13. 不能；停止是协作式的，线程要检查令牌或使用可停止等待。
14. 为保证等待线程醒来时对象仍存在；销毁前必须停止、唤醒并 `join`。

## 附录：后续学习路线

现在可以继续 EdgeBot 的定时器、虚拟传输、协议解析、真实串口 FD、非阻塞 I/O 和 `epoll`。优先级建议是：先让 MiniEdge 的关闭语义与测试可靠，再加入截止时间队列，最后接 Linux I/O。之后可学习 `epoll`、自旋锁与无锁数据结构、内存模型、内核调度器、`perf`、信号和进程间通信。

这些内容暂时可以跳过：内核调度器源码、完整页表实现、复杂无锁算法、CPU 缓存一致性协议细节。它们不是当前目标的必要前提；当前阶段只需能从现象定位同步、生命周期和阻塞问题，并能说明自己依赖的是标准保证还是 Linux 实现。
