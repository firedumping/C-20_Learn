#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

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
