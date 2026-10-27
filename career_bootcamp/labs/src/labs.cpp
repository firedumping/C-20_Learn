#include "edgebot_labs/labs.hpp"

#include <string>

namespace edgebot_labs {
namespace {

[[noreturn]] void not_implemented(const char* lab) {
    throw std::logic_error{std::string{lab} + " is not implemented yet"};
}

}  // namespace

EventLoop::EventLoop(
    std::size_t task_capacity,
    std::size_t timer_capacity,
    NowFunction now)
    : task_capacity_{task_capacity},
      timer_capacity_{timer_capacity},
      now_{std::move(now)} {
    if (task_capacity_ == 0 || timer_capacity_ == 0) {
        throw std::invalid_argument{"capacity must be greater than zero"};
    }
    if (!now_) {
        throw std::invalid_argument{"now function must not be empty"};
    }
}

EventLoop::PostResult EventLoop::post(Task /*task*/) {
    not_implemented("Lab 01 EventLoop::post");
}

EventLoop::PostResult EventLoop::post_high_priority(Task /*task*/) {
    not_implemented("Lab 01 EventLoop::post_high_priority");
}

EventLoop::PostResult EventLoop::post_after(
    Milliseconds /*delay*/,
    Task /*task*/) {
    not_implemented("Lab 02 EventLoop::post_after");
}

std::size_t EventLoop::run_once() {
    not_implemented("Lab 01/02 EventLoop::run_once");
}

void EventLoop::request_stop() {
    not_implemented("Lab 01 EventLoop::request_stop");
}

bool EventLoop::stopped() const noexcept {
    return stopped_;
}

std::size_t EventLoop::pending_tasks() const noexcept {
    return ready_.size();
}

std::size_t EventLoop::pending_timers() const noexcept {
    return timers_.size();
}

std::size_t EventLoop::task_errors() const noexcept {
    return task_errors_;
}

std::uint16_t crc16_ccitt(std::span<const std::byte> /*data*/) {
    not_implemented("Lab 03 crc16_ccitt");
}

std::vector<std::byte> encode_frame(
    std::uint8_t /*type*/,
    std::uint16_t /*sequence*/,
    std::span<const std::byte> /*payload*/) {
    not_implemented("Lab 03 encode_frame");
}

FrameDecoder::FrameDecoder(std::size_t max_payload)
    : max_payload_{max_payload} {
    if (max_payload_ == 0) {
        throw std::invalid_argument{"max payload must be greater than zero"};
    }
}

void FrameDecoder::feed(std::span<const std::byte> /*bytes*/) {
    not_implemented("Lab 03 FrameDecoder::feed");
}

std::optional<FrameOutcome> FrameDecoder::next() {
    not_implemented("Lab 03 FrameDecoder::next");
}

std::size_t FrameDecoder::buffered_bytes() const noexcept {
    return buffer_.size();
}

ReconnectPolicy::ReconnectPolicy(
    Milliseconds initial_delay,
    Milliseconds maximum_delay)
    : initial_delay_{initial_delay},
      maximum_delay_{maximum_delay},
      current_delay_{initial_delay} {
    if (initial_delay_ <= Milliseconds::zero() ||
        maximum_delay_ < initial_delay_) {
        throw std::invalid_argument{"invalid reconnect delay range"};
    }
}

Milliseconds ReconnectPolicy::next_delay() {
    not_implemented("Lab 04 ReconnectPolicy::next_delay");
}

void ReconnectPolicy::reset() noexcept {
    current_delay_ = initial_delay_;
    consecutive_failures_ = 0;
}

std::size_t ReconnectPolicy::consecutive_failures() const noexcept {
    return consecutive_failures_;
}

LatencyRecorder::LatencyRecorder(std::size_t maximum_samples)
    : maximum_samples_{maximum_samples} {
    if (maximum_samples_ == 0) {
        throw std::invalid_argument{"maximum samples must be greater than zero"};
    }
    samples_.reserve(maximum_samples_);
}

void LatencyRecorder::add(Microseconds /*latency*/) {
    not_implemented("Lab 05 LatencyRecorder::add");
}

std::optional<Microseconds> LatencyRecorder::percentile(
    double /*percentile*/) const {
    not_implemented("Lab 05 LatencyRecorder::percentile");
}

std::size_t LatencyRecorder::sample_count() const noexcept {
    return samples_.size();
}

void LatencyRecorder::reset() noexcept {
    samples_.clear();
    next_sample_ = 0;
}

DeviceState DeviceStateMachine::state() const noexcept {
    return state_;
}

bool DeviceStateMachine::handle(DeviceEvent /*event*/) noexcept {
    // Keep the exercise starter noexcept-safe: the test should report a normal
    // assertion failure instead of terminating the whole test executable.
    return false;
}

}  // namespace edgebot_labs

