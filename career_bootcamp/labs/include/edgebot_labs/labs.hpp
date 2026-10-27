#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <variant>
#include <vector>

namespace edgebot_labs {

using Clock = std::chrono::steady_clock;
using Milliseconds = std::chrono::milliseconds;
using Microseconds = std::chrono::microseconds;

// Lab 01 and Lab 02: event loop with bounded priority task queue and timers.
class EventLoop {
public:
    using Task = std::function<void()>;
    using NowFunction = std::function<Clock::time_point()>;

    enum class Priority {
        normal,
        high,
    };

    enum class PostResult {
        accepted,
        full,
        stopped,
    };

    explicit EventLoop(
        std::size_t task_capacity,
        std::size_t timer_capacity = 1024,
        NowFunction now = Clock::now);

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;
    EventLoop(EventLoop&&) = delete;
    EventLoop& operator=(EventLoop&&) = delete;

    PostResult post(Task task);
    PostResult post_high_priority(Task task);
    PostResult post_after(Milliseconds delay, Task task);

    // Executes the tasks that are ready when this call begins. Tasks posted by
    // callbacks are left for a later call to run_once().
    std::size_t run_once();

    void request_stop();
    [[nodiscard]] bool stopped() const noexcept;
    [[nodiscard]] std::size_t pending_tasks() const noexcept;
    [[nodiscard]] std::size_t pending_timers() const noexcept;
    [[nodiscard]] std::size_t task_errors() const noexcept;

private:
    struct QueuedTask {
        Priority priority;
        std::uint64_t sequence;
        Task callback;
    };

    struct ScheduledTask {
        std::uint64_t sequence;
        Task callback;
    };

    std::size_t task_capacity_;
    std::size_t timer_capacity_;
    NowFunction now_;
    std::uint64_t next_sequence_{0};
    std::vector<QueuedTask> ready_;
    std::multimap<Clock::time_point, ScheduledTask> timers_;
    bool stopped_{false};
    std::size_t task_errors_{0};
};

// Lab 03: incremental frame codec.
constexpr std::uint8_t kFrameMagic0 = 0xAA;
constexpr std::uint8_t kFrameMagic1 = 0x55;
constexpr std::uint8_t kFrameVersion = 0x01;
constexpr std::size_t kFrameHeaderSize = 8;
constexpr std::size_t kFrameTrailerSize = 2;

struct Frame {
    std::uint8_t version{};
    std::uint8_t type{};
    std::uint16_t sequence{};
    std::vector<std::byte> payload;
};

enum class FrameError {
    unsupported_version,
    checksum,
    payload_too_large,
};

struct FrameFailure {
    FrameError error;
};

using FrameOutcome = std::variant<Frame, FrameFailure>;

[[nodiscard]] std::uint16_t crc16_ccitt(std::span<const std::byte> data);

[[nodiscard]] std::vector<std::byte> encode_frame(
    std::uint8_t type,
    std::uint16_t sequence,
    std::span<const std::byte> payload);

class FrameDecoder {
public:
    explicit FrameDecoder(std::size_t max_payload = 256);

    void feed(std::span<const std::byte> bytes);

    // std::nullopt means that more bytes are required.
    [[nodiscard]] std::optional<FrameOutcome> next();

    [[nodiscard]] std::size_t buffered_bytes() const noexcept;

private:
    std::size_t max_payload_;
    std::vector<std::byte> buffer_;
};

// Lab 04: deterministic reconnect backoff.
class ReconnectPolicy {
public:
    ReconnectPolicy(Milliseconds initial_delay, Milliseconds maximum_delay);

    [[nodiscard]] Milliseconds next_delay();
    void reset() noexcept;
    [[nodiscard]] std::size_t consecutive_failures() const noexcept;

private:
    Milliseconds initial_delay_;
    Milliseconds maximum_delay_;
    Milliseconds current_delay_;
    std::size_t consecutive_failures_{0};
};

// Lab 05: bounded-memory latency summary.
class LatencyRecorder {
public:
    explicit LatencyRecorder(std::size_t maximum_samples = 4096);

    void add(Microseconds latency);
    [[nodiscard]] std::optional<Microseconds> percentile(double percentile) const;
    [[nodiscard]] std::size_t sample_count() const noexcept;
    void reset() noexcept;

private:
    std::size_t maximum_samples_;
    std::vector<Microseconds> samples_;
    std::size_t next_sample_{0};
};

// Lab 06: device connection state machine.
enum class DeviceState {
    idle,
    connecting,
    online,
    degraded,
    closed,
};

enum class DeviceEvent {
    connect_requested,
    connection_opened,
    heartbeat_lost,
    heartbeat_restored,
    close_requested,
};

class DeviceStateMachine {
public:
    [[nodiscard]] DeviceState state() const noexcept;
    [[nodiscard]] bool handle(DeviceEvent event) noexcept;

private:
    DeviceState state_{DeviceState::idle};
};

}  // namespace edgebot_labs

