#include "edgebot_labs/labs.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace edgebot_labs;
using namespace std::chrono_literals;

class TestFailure : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void check(bool condition, std::string_view message, int line) {
    if (!condition) {
        std::ostringstream stream;
        stream << "line " << line << ": " << message;
        throw TestFailure{stream.str()};
    }
}

#define CHECK(condition) check((condition), #condition, __LINE__)

std::span<const std::byte> bytes_of(std::string_view text) {
    return {
        reinterpret_cast<const std::byte*>(text.data()),
        text.size(),
    };
}

void test_event_loop_priority_and_fifo() {
    EventLoop loop{8};
    std::vector<std::string> order;

    CHECK(loop.post([&] { order.push_back("normal-1"); }) ==
          EventLoop::PostResult::accepted);
    CHECK(loop.post_high_priority([&] { order.push_back("high-1"); }) ==
          EventLoop::PostResult::accepted);
    CHECK(loop.post([&] { order.push_back("normal-2"); }) ==
          EventLoop::PostResult::accepted);
    CHECK(loop.post_high_priority([&] { order.push_back("high-2"); }) ==
          EventLoop::PostResult::accepted);

    CHECK(loop.run_once() == 4U);
    CHECK((order == std::vector<std::string>{
                       "high-1", "high-2", "normal-1", "normal-2"}));
    CHECK(loop.pending_tasks() == 0U);
}

void test_event_loop_batch_reentrancy() {
    EventLoop loop{8};
    std::vector<std::string> order;

    CHECK(loop.post([&] {
              order.push_back("first");
              CHECK(loop.post([&] { order.push_back("second"); }) ==
                    EventLoop::PostResult::accepted);
          }) == EventLoop::PostResult::accepted);

    CHECK(loop.run_once() == 1U);
    CHECK((order == std::vector<std::string>{"first"}));
    CHECK(loop.pending_tasks() == 1U);
    CHECK(loop.run_once() == 1U);
    CHECK((order == std::vector<std::string>{"first", "second"}));
}

void test_event_loop_capacity_and_stop() {
    EventLoop loop{1};
    CHECK(loop.post([] {}) == EventLoop::PostResult::accepted);
    CHECK(loop.post([] {}) == EventLoop::PostResult::full);
    CHECK(loop.run_once() == 1U);
    CHECK(loop.post([] {}) == EventLoop::PostResult::accepted);

    loop.request_stop();
    CHECK(loop.stopped());
    CHECK(loop.post([] {}) == EventLoop::PostResult::stopped);

    // A stop request rejects new work but drains work accepted before the stop.
    CHECK(loop.run_once() == 1U);
    CHECK(loop.run_once() == 0U);
}

void test_event_loop_task_error_isolation() {
    EventLoop loop{8};
    bool later_task_ran = false;

    CHECK(loop.post([] { throw std::runtime_error{"expected"}; }) ==
          EventLoop::PostResult::accepted);
    CHECK(loop.post([&] { later_task_ran = true; }) ==
          EventLoop::PostResult::accepted);

    CHECK(loop.run_once() == 2U);
    CHECK(later_task_ran);
    CHECK(loop.task_errors() == 1U);
}

void test_event_loop_timers_with_fake_clock() {
    Clock::time_point now{};
    EventLoop loop{8, 8, [&now] { return now; }};
    std::vector<std::string> order;

    CHECK(loop.post_after(10ms, [&] { order.push_back("t10"); }) ==
          EventLoop::PostResult::accepted);
    CHECK(loop.post_after(5ms, [&] { order.push_back("t5-a"); }) ==
          EventLoop::PostResult::accepted);
    CHECK(loop.post_after(5ms, [&] { order.push_back("t5-b"); }) ==
          EventLoop::PostResult::accepted);

    CHECK(loop.pending_timers() == 3U);
    now += 4ms;
    CHECK(loop.run_once() == 0U);
    now += 1ms;
    CHECK(loop.run_once() == 2U);
    CHECK((order == std::vector<std::string>{"t5-a", "t5-b"}));
    now += 5ms;
    CHECK(loop.run_once() == 1U);
    CHECK((order == std::vector<std::string>{"t5-a", "t5-b", "t10"}));
}

void test_crc_known_vector() {
    // CRC-16/CCITT-FALSE of ASCII "123456789" is 0x29B1.
    CHECK(crc16_ccitt(bytes_of("123456789")) == 0x29B1U);
}

void test_frame_round_trip_and_fragmentation() {
    constexpr std::array<std::byte, 4> payload{
        std::byte{0x01}, std::byte{0x02}, std::byte{0xFF}, std::byte{0x10}};
    const auto encoded = encode_frame(0x02, 0x1234, payload);

    FrameDecoder decoder{64};
    for (const auto value : encoded) {
        decoder.feed(std::span{&value, 1});
    }

    auto outcome = decoder.next();
    CHECK(outcome.has_value());
    auto* frame = std::get_if<Frame>(&*outcome);
    CHECK(frame != nullptr);
    CHECK(frame->version == kFrameVersion);
    CHECK(frame->type == 0x02);
    CHECK(frame->sequence == 0x1234);
    CHECK(std::equal(
        frame->payload.begin(), frame->payload.end(), payload.begin(), payload.end()));
    CHECK(decoder.buffered_bytes() == 0U);
}

void test_frame_decoder_multiple_frames_and_resync() {
    constexpr std::array<std::byte, 2> first_payload{
        std::byte{0x11}, std::byte{0x12}};
    constexpr std::array<std::byte, 1> second_payload{std::byte{0x21}};
    const auto first = encode_frame(0x01, 1, first_payload);
    const auto second = encode_frame(0x02, 2, second_payload);

    std::vector<std::byte> stream{
        std::byte{0x00}, std::byte{0xAA}, std::byte{0x00},
        std::byte{0x55}, std::byte{0x99}};
    stream.insert(stream.end(), first.begin(), first.end());
    stream.insert(stream.end(), second.begin(), second.end());

    FrameDecoder decoder{64};
    decoder.feed(stream);

    auto first_outcome = decoder.next();
    CHECK(first_outcome.has_value());
    auto* first_frame = std::get_if<Frame>(&*first_outcome);
    CHECK(first_frame != nullptr);
    CHECK(first_frame->sequence == 1U);

    auto second_outcome = decoder.next();
    CHECK(second_outcome.has_value());
    auto* second_frame = std::get_if<Frame>(&*second_outcome);
    CHECK(second_frame != nullptr);
    CHECK(second_frame->sequence == 2U);

    CHECK(!decoder.next().has_value());
}

void test_frame_decoder_crc_failure_then_recovery() {
    constexpr std::array<std::byte, 2> payload{
        std::byte{0x10}, std::byte{0x20}};
    auto bad = encode_frame(0x02, 7, payload);
    bad[8] ^= std::byte{0x01};
    const auto good = encode_frame(0x02, 8, payload);

    FrameDecoder decoder{64};
    decoder.feed(bad);
    auto bad_outcome = decoder.next();
    CHECK(bad_outcome.has_value());
    auto* failure = std::get_if<FrameFailure>(&*bad_outcome);
    CHECK(failure != nullptr);
    CHECK(failure->error == FrameError::checksum);

    decoder.feed(good);
    auto good_outcome = decoder.next();
    CHECK(good_outcome.has_value());
    auto* frame = std::get_if<Frame>(&*good_outcome);
    CHECK(frame != nullptr);
    CHECK(frame->sequence == 8U);
}

void test_frame_decoder_rejects_oversize_payload() {
    FrameDecoder decoder{8};
    std::array<std::byte, 8> header{};
    header[0] = std::byte{kFrameMagic0};
    header[1] = std::byte{kFrameMagic1};
    header[2] = std::byte{kFrameVersion};
    header[3] = std::byte{0x01};
    header[4] = std::byte{0x00};
    header[5] = std::byte{0x00};
    header[6] = std::byte{0x09};
    header[7] = std::byte{0x00};
    decoder.feed(header);

    auto outcome = decoder.next();
    CHECK(outcome.has_value());
    auto* failure = std::get_if<FrameFailure>(&*outcome);
    CHECK(failure != nullptr);
    CHECK(failure->error == FrameError::payload_too_large);
}

void test_reconnect_backoff_and_reset() {
    ReconnectPolicy policy{100ms, 1000ms};
    CHECK(policy.next_delay() == 100ms);
    CHECK(policy.next_delay() == 200ms);
    CHECK(policy.next_delay() == 400ms);
    CHECK(policy.next_delay() == 800ms);
    CHECK(policy.next_delay() == 1000ms);
    CHECK(policy.next_delay() == 1000ms);
    CHECK(policy.consecutive_failures() == 6U);

    policy.reset();
    CHECK(policy.consecutive_failures() == 0U);
    CHECK(policy.next_delay() == 100ms);
}

void test_latency_recorder_ring_buffer_and_percentiles() {
    LatencyRecorder recorder{4};
    for (const auto value : {10, 20, 30, 40, 50}) {
        recorder.add(Microseconds{value});
    }

    CHECK(recorder.sample_count() == 4U);
    CHECK(recorder.percentile(0.0).value() == Microseconds{20});
    CHECK(recorder.percentile(0.5).value() == Microseconds{30});
    CHECK(recorder.percentile(1.0).value() == Microseconds{50});
}

void test_device_state_machine() {
    DeviceStateMachine machine;
    CHECK(machine.state() == DeviceState::idle);
    CHECK(machine.handle(DeviceEvent::connection_opened) == false);
    CHECK(machine.handle(DeviceEvent::connect_requested));
    CHECK(machine.state() == DeviceState::connecting);
    CHECK(machine.handle(DeviceEvent::connection_opened));
    CHECK(machine.state() == DeviceState::online);
    CHECK(machine.handle(DeviceEvent::heartbeat_lost));
    CHECK(machine.state() == DeviceState::degraded);
    CHECK(machine.handle(DeviceEvent::heartbeat_restored));
    CHECK(machine.state() == DeviceState::online);
    CHECK(machine.handle(DeviceEvent::close_requested));
    CHECK(machine.state() == DeviceState::closed);
    CHECK(machine.handle(DeviceEvent::connect_requested) == false);
}

}  // namespace

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"event loop priority and FIFO", test_event_loop_priority_and_fifo},
        {"event loop batch reentrancy", test_event_loop_batch_reentrancy},
        {"event loop capacity and stop", test_event_loop_capacity_and_stop},
        {"event loop task error isolation", test_event_loop_task_error_isolation},
        {"event loop timers with fake clock", test_event_loop_timers_with_fake_clock},
        {"CRC known vector", test_crc_known_vector},
        {"frame round trip and fragmentation", test_frame_round_trip_and_fragmentation},
        {"frame multiple and resync", test_frame_decoder_multiple_frames_and_resync},
        {"frame CRC recovery", test_frame_decoder_crc_failure_then_recovery},
        {"frame oversize rejection", test_frame_decoder_rejects_oversize_payload},
        {"reconnect backoff and reset", test_reconnect_backoff_and_reset},
        {"latency ring buffer and percentiles", test_latency_recorder_ring_buffer_and_percentiles},
        {"device state machine", test_device_state_machine},
    };

    std::size_t passed = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            ++passed;
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            std::cout << "[FAIL] " << name << ": " << error.what() << '\n';
        } catch (...) {
            std::cout << "[FAIL] " << name << ": unknown exception\n";
        }
    }

    std::cout << "\nPassed " << passed << "/" << tests.size() << " tests.\n";
    return passed == tests.size() ? 0 : 1;
}

