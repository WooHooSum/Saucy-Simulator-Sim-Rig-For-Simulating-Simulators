#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace wheel {

template <typename T, std::size_t Capacity>
class SpscQueue {
  static_assert(Capacity >= 2, "SPSC queue requires at least two entries");

 public:
  bool push(const T& value) {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto next = increment(head);
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    entries_[head] = value;
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool pop(T& value) {
    const auto tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return false;
    }
    value = entries_[tail];
    tail_.store(increment(tail), std::memory_order_release);
    return true;
  }

  bool empty() const {
    return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
  }

 private:
  static constexpr std::size_t increment(std::size_t value) { return (value + 1) % Capacity; }

  std::array<T, Capacity> entries_{};
  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
};

}  // namespace wheel

