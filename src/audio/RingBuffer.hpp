#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

// Single-producer / single-consumer lock-free float ring buffer.
// Producer: audio callback. Consumer: game/DSP thread.
// On overflow, drops the samples that do not fit and increments overflowCount.
class RingBuffer {
 public:
  explicit RingBuffer(std::size_t capacity)
      : capacity_(capacity),
        buffer_(capacity),
        write_pos_(0),
        read_pos_(0),
        overflow_count_(0) {}

  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;

  std::size_t capacity() const { return capacity_; }

  // How many samples the consumer can read right now.
  std::size_t available() const {
    const auto w = write_pos_.load(std::memory_order_acquire);
    const auto r = read_pos_.load(std::memory_order_relaxed);
    return w - r;
  }

  std::size_t freeSpace() const { return capacity_ - available(); }

  std::uint64_t overflowCount() const {
    return overflow_count_.load(std::memory_order_relaxed);
  }

  // Producer only. Returns number of samples actually written.
  std::size_t write(const float* data, std::size_t count) {
    if (data == nullptr || count == 0 || capacity_ == 0) {
      return 0;
    }

    const auto w = write_pos_.load(std::memory_order_relaxed);
    const auto r = read_pos_.load(std::memory_order_acquire);
    const std::size_t used = w - r;
    const std::size_t free = capacity_ - used;

    std::size_t to_write = count;
    if (to_write > free) {
      overflow_count_.fetch_add(to_write - free, std::memory_order_relaxed);
      to_write = free;
    }
    if (to_write == 0) {
      return 0;
    }

    const std::size_t index = w % capacity_;
    const std::size_t first = std::min(to_write, capacity_ - index);
    std::copy_n(data, first, buffer_.data() + index);
    if (first < to_write) {
      std::copy_n(data + first, to_write - first, buffer_.data());
    }

    write_pos_.store(w + to_write, std::memory_order_release);
    return to_write;
  }

  // Consumer only. Returns number of samples actually read.
  std::size_t read(float* out, std::size_t count) {
    if (out == nullptr || count == 0 || capacity_ == 0) {
      return 0;
    }

    const auto r = read_pos_.load(std::memory_order_relaxed);
    const auto w = write_pos_.load(std::memory_order_acquire);
    const std::size_t avail = w - r;
    const std::size_t to_read = std::min(count, avail);
    if (to_read == 0) {
      return 0;
    }

    const std::size_t index = r % capacity_;
    const std::size_t first = std::min(to_read, capacity_ - index);
    std::copy_n(buffer_.data() + index, first, out);
    if (first < to_read) {
      std::copy_n(buffer_.data(), to_read - first, out + first);
    }

    read_pos_.store(r + to_read, std::memory_order_release);
    return to_read;
  }

 private:
  const std::size_t capacity_;
  std::vector<float> buffer_;
  std::atomic<std::size_t> write_pos_;
  std::atomic<std::size_t> read_pos_;
  std::atomic<std::uint64_t> overflow_count_;
};
