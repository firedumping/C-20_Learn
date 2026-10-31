# 第 12 章：C++20 并发

## 学习目标

- 区分线程、任务、异步操作和协作式取消。
- 识别数据竞争和未定义行为。
- 用互斥量保护复合不变量。
- 用 `std::jthread` 和 `std::stop_token` 实现可停止任务。
- 理解条件变量、`latch`、`barrier`、信号量的典型用途。
- 知道原子操作不自动让复杂逻辑线程安全。

## 1. 数据竞争不是“结果偶尔不对”

两个线程在没有同步的情况下访问同一个对象，至少一个写入，就构成数据竞争；这是未定义行为。表现可能是错误值、崩溃、编译器重排后的异常结果，甚至看似正常运行。

线程安全需要先定义共享状态：

- 哪些对象被共享？
- 哪些操作是读？
- 哪些操作是写？
- 复合操作是否需要保持不变量？

## 2. 基本线程

```cpp
#include <iostream>
#include <thread>
#include <vector>

void worker(int id) {
    std::cout << "worker " << id << '\n';
}

int main() {
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back(worker, i);
    }
    for (auto& worker : workers) {
        worker.join();
    }
}
```

每个 `std::thread` 在对象销毁前必须 `join()` 或 `detach()`，否则会导致 `std::terminate`。`detach` 启动的后台线程生命周期难以掌握，初学者应优先显式 join。

## 3. `std::jthread` 与停止令牌

`std::jthread` 析构时自动请求停止并 join：

```cpp
#include <chrono>
#include <iostream>
#include <thread>

void poll_forever(std::stop_token stop) {
    while (!stop.stop_requested()) {
        // 做一小段工作，而不是永远阻塞
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
}

int main() {
    std::jthread worker{poll_forever};
    std::this_thread::sleep_for(std::chrono::milliseconds{350});
    worker.request_stop();
} // jthread 析构会自动 join
```

停止是协作式的：`request_stop()` 只是设置标志，工作线程必须定期检查。若线程永远在阻塞等待，令牌不会强行打断它。

## 4. 互斥量保护不变量

```cpp
#include <mutex>
#include <stdexcept>

class Counter {
public:
    void increment() {
        std::lock_guard lock{mutex_};
        ++value_;
        if (value_ < 0) {
            throw std::logic_error{"counter invariant broken"};
        }
    }

    int value() const {
        std::lock_guard lock{mutex_};
        return value_;
    }

private:
    mutable std::mutex mutex_;
    int value_{};
};
```

要点：

- 锁的作用域尽量小。
- 不要持锁调用可能回调外部代码的函数，避免死锁和长时间阻塞。
- `mutable std::mutex` 允许 `const` 成员函数在内部同步。
- 锁保护的是数据和它对应的一组操作，不只是单个变量赋值。

`std::scoped_lock` 可同时锁定多个互斥量并避免顺序死锁：

```cpp
std::scoped_lock lock{mutex_a, mutex_b};
```

## 5. 条件变量

条件变量用于等待某个状态：

```cpp
#include <condition_variable>
#include <mutex>
#include <queue>

class BlockingQueue {
public:
    void push(int value) {
        {
            std::lock_guard lock{mutex_};
            values_.push(value);
        }
        condition_.notify_one();
    }

    bool pop(int& output) {
        std::unique_lock lock{mutex_};
        condition_.wait(lock, [this] {
            return closed_ || !values_.empty();
        });
        if (values_.empty()) {
            return false;
        }
        output = values_.front();
        values_.pop();
        return true;
    }

    void close() {
        {
            std::lock_guard lock{mutex_};
            closed_ = true;
        }
        condition_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<int> values_;
    bool closed_{};
};
```

`wait` 会释放锁并睡眠，醒来后重新获取锁。谓词用于处理伪唤醒和状态变化。所有修改谓词状态的路径都必须通知等待者。

把 `pop` 拆开看：

1. `unique_lock` 先获得互斥量。
2. `wait` 在持锁状态下检查谓词。
3. 条件为假时，`wait` 自动释放锁并睡眠，不会让生产者被永久挡住。
4. 被通知后重新获得锁，再检查一次谓词。
5. 条件为真时，`pop` 在仍持锁的状态下读取并移除队列头部。

假设上面的 `BlockingQueue` 已放在同一个文件中，下面演示生产者、消费者和关闭流程：

```cpp
#include <iostream>
#include <stop_token>
#include <thread>

int main() {
    BlockingQueue queue;

    std::jthread consumer{[&queue] {
        int value = 0;
        while (queue.pop(value)) {
            std::cout << "consumed " << value << '\n';
        }
    }};

    std::jthread producer{[&queue] {
        for (int value = 1; value <= 3; ++value) {
            queue.push(value);
        }
        queue.close();
    }};
}
```

输出：

```text
consumed 1
consumed 2
consumed 3
```

`close()` 把 `closed_` 设为 `true` 并执行 `notify_all()`。消费者把已有元素处理完后，下一次 `pop` 看到“已关闭且队列为空”就不再等待，返回 `false`，线程正常结束。`jthread` 析构时会 join 两个线程。

这个示例为了聚焦条件变量，允许多次 `pop` 但只启动一个消费者。多个消费者同时处理时，还需要定义结果顺序、异常处理和关闭策略。

完整可编译版本位于
[`examples/blocking_queue.cpp`](../examples/blocking_queue.cpp)。

## 6. 原子类型

```cpp
#include <atomic>

std::atomic<int> completed{0};

void finish_one() {
    completed.fetch_add(1, std::memory_order_relaxed);
}
```

原子操作保证单个原子对象上的操作不可分割，但复杂业务不变量仍可能需要锁。`fetch_add` 适合独立计数，不代表一个多字段对象的更新是原子的。

默认使用顺序一致（`seq_cst`）更安全。只有在理解内存序和性能后，才考虑 `acquire`/`release` 等更弱顺序。

```cpp
std::atomic<bool> ready{false};
int shared_value = 0;

// 线程 A
shared_value = 42;
ready.store(true, std::memory_order_release);

// 线程 B
while (!ready.load(std::memory_order_acquire)) {
}
// 现在可以读取 shared_value
```

更弱的内存序会增加证明难度。初学阶段不要为了微优化把每个原子都改成 `relaxed`。

## 7. `latch`、`barrier` 与信号量

### `std::latch`

一次性等待多个任务到达：

```cpp
#include <latch>
#include <thread>
#include <vector>

std::latch ready{3};
std::vector<std::jthread> workers;
for (int i = 0; i < 3; ++i) {
    workers.emplace_back([&ready] {
        // 初始化工作
        ready.count_down();
    });
}
ready.wait();
// 三个线程都完成后继续
```

### `std::barrier`

可重复使用的同步点。每个阶段所有参与者到达后一起继续。

### `std::counting_semaphore`

限制并发资源数量：

```cpp
#include <semaphore>

std::counting_semaphore<8> slots{8};

void use_resource() {
    slots.acquire();
    // 最多 8 个线程同时到这里
    slots.release();
}
```

异常路径可能跳过 `release`；生产代码应使用 RAII 包装。

## 8. 并发设计规则

- 优先减少共享状态，而不是给所有东西加锁。
- 明确线程的生命周期和退出条件。
- 一个互斥量保护一组相关状态；不要用多个锁保护同一不变量。
- 用谓词条件变量等待，避免虚假唤醒。
- 不持锁调用未知回调。
- 原子变量不是事务。
- 每个线程都应该输出可诊断的错误，而不是静默终止。
- 使用 ThreadSanitizer 等工具检查数据竞争（工具链支持时）。

## 练习 1：并行求和

把一组整数分成若干段，用 4 个 `std::jthread` 分别求和，再合并。要求：

- 每段大小正确，空输入不创建无意义线程。
- 计算线程只读输入范围。
- 不通过无同步的多个写者更新同一个普通整数。

可以先让每个线程写入独立结果，再在主线程合并。

## 练习 2：可停止的任务循环

实现一个 `jthread` 工作器：

- 每 100 毫秒检查一次 `stop_token`。
- 每次循环增加一个独立计数。
- 主线程请求停止后能安全 join。
- 输出最终计数。

## 练习 3：有界队列

扩展示例 `BlockingQueue`：

- 添加容量上限。
- `push` 在满时等待。
- `pop` 在空时等待。
- 支持关闭并唤醒所有等待者。
- 说明关闭后队列中剩余元素如何处理。

## 自测

1. 什么是数据竞争？
2. `std::jthread` 析构时做了什么？
3. 条件变量为什么要配合谓词？
4. 原子变量为什么不等于所有操作都线程安全？
5. 如何避免持锁调用外部回调导致死锁？

## 下一步

[第 13 章](13_engineering.md)把代码放进真实工程：构建、测试、静态分析和性能分析。
