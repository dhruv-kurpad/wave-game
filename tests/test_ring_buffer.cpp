#include "audio/RingBuffer.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

int g_failures = 0;

void expect(bool cond, const char* name) {
  if (!cond) {
    std::cerr << "FAIL: " << name << '\n';
    ++g_failures;
  } else {
    std::cout << "PASS: " << name << '\n';
  }
}

}  // namespace

int main() {
  // P1-RB01 empty read
  {
    RingBuffer rb(1024);
    float out[8]{};
    expect(rb.read(out, 8) == 0, "P1-RB01 empty read");
  }

  // P1-RB02 write then read
  {
    RingBuffer rb(2048);
    std::vector<float> in(1024);
    for (std::size_t i = 0; i < in.size(); ++i) {
      in[i] = static_cast<float>(i);
    }
    expect(rb.write(in.data(), in.size()) == 1024, "P1-RB02 write 1024");
    std::vector<float> out(1024);
    expect(rb.read(out.data(), out.size()) == 1024, "P1-RB02 read 1024");
    bool match = true;
    for (std::size_t i = 0; i < out.size(); ++i) {
      if (out[i] != in[i]) {
        match = false;
        break;
      }
    }
    expect(match, "P1-RB02 data match");
    expect(rb.available() == 0, "P1-RB02 empty after read");
  }

  // P1-RB03 partial read
  {
    RingBuffer rb(4096);
    std::vector<float> in(2048, 1.0f);
    rb.write(in.data(), in.size());
    float a[512];
    float b[512];
    expect(rb.read(a, 512) == 512, "P1-RB03 first partial");
    expect(rb.read(b, 512) == 512, "P1-RB03 second partial");
    expect(rb.available() == 1024, "P1-RB03 remainder 1024");
  }

  // P1-RB04 wrap-around
  {
    RingBuffer rb(64);
    std::vector<float> first(48);
    for (std::size_t i = 0; i < first.size(); ++i) {
      first[i] = static_cast<float>(i);
    }
    rb.write(first.data(), first.size());
    float drain[48];
    rb.read(drain, 40);  // leave 8, write near end

    std::vector<float> second(50);
    for (std::size_t i = 0; i < second.size(); ++i) {
      second[i] = 100.0f + static_cast<float>(i);
    }
    const std::size_t wrote = rb.write(second.data(), second.size());
    expect(wrote == 50, "P1-RB04 write across wrap");

    std::vector<float> out(58);
    expect(rb.read(out.data(), out.size()) == 58, "P1-RB04 read all");
    bool ok = true;
    for (int i = 0; i < 8; ++i) {
      if (out[static_cast<std::size_t>(i)] != static_cast<float>(40 + i)) {
        ok = false;
      }
    }
    for (int i = 0; i < 50; ++i) {
      if (out[static_cast<std::size_t>(8 + i)] != 100.0f + static_cast<float>(i)) {
        ok = false;
      }
    }
    expect(ok, "P1-RB04 wrap order");
  }

  // P1-RB05 overflow drops + counter
  {
    RingBuffer rb(16);
    float block[32];
    for (int i = 0; i < 32; ++i) {
      block[i] = static_cast<float>(i);
    }
    const std::size_t wrote = rb.write(block, 32);
    expect(wrote == 16, "P1-RB05 writes only free space");
    expect(rb.overflowCount() == 16, "P1-RB05 overflow count");
  }

  // P1-RB06 capacity
  {
    RingBuffer rb(16384);
    expect(rb.capacity() >= 8192, "P1-RB06 capacity >= 8x 1024");
  }

  if (g_failures > 0) {
    std::cerr << g_failures << " test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "All ring buffer tests passed\n";
  return EXIT_SUCCESS;
}
