#include "dsp/DSPMath.hpp"

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

bool approx(float a, float b, float eps) { return std::fabs(a - b) <= eps; }
}  // namespace

int main() {
  // P2-DSP01 RMS silence
  {
    std::vector<float> z(1024, 0.0f);
    expect(computeRms(z.data(), z.size()) == 0.0f, "P2-DSP01 RMS silence");
  }

  // P2-DSP02 RMS of 0.5 amplitude sine ≈ 0.5/√2
  {
    std::vector<float> s(4410);
    constexpr float kAmp = 0.5f;
    constexpr float kFreq = 440.0f;
    constexpr float kSr = 44100.0f;
    for (std::size_t i = 0; i < s.size(); ++i) {
      s[i] = kAmp * std::sin(2.0f * 3.14159265f * kFreq *
                             static_cast<float>(i) / kSr);
    }
    const float rms = computeRms(s.data(), s.size());
    const float expected = kAmp / std::sqrt(2.0f);
    expect(approx(rms, expected, 0.01f), "P2-DSP02 RMS sine 0.5");
  }

  // P2-DSP03 gate
  {
    float peak = 0.1f;
    const float v = volumeFromRms(0.005f, 0.01f, 1.0f, peak);
    expect(v == 0.0f, "P2-DSP03 gated to zero");
  }

  // P2-DSP04 normalize range
  {
    float peak = 0.0f;
    bool ok = true;
    for (int i = 0; i < 50; ++i) {
      const float rms = 0.02f + 0.01f * static_cast<float>(i % 5);
      const float v = volumeFromRms(rms, 0.001f, 1.0f, peak);
      if (v < 0.0f || v > 1.0f) {
        ok = false;
        break;
      }
    }
    expect(ok, "P2-DSP04 in range");
  }

  // P2-DSP05 wave point count (builder preserves size)
  {
    std::vector<float> w(256, 0.0f);
    buildWaveFromVolume(w, 0.5f, 0.0f);
    expect(w.size() == 256, "P2-DSP05 point count 256");
    bool in_range = true;
    for (float h : w) {
      if (h < 0.0f || h > 1.0f) {
        in_range = false;
      }
    }
    expect(in_range, "P2-DSP05 heights in [0,1]");
  }

  // P2-DSP06 temporal smoothing rises gradually
  {
    std::vector<float> target(8, 1.0f);
    std::vector<float> state(8, 0.0f);
    temporalSmooth(target, state, 0.2f);
    expect(state[0] > 0.0f && state[0] < 1.0f, "P2-DSP06 partial step");
    for (int i = 0; i < 40; ++i) {
      temporalSmooth(target, state, 0.2f);
    }
    expect(state[0] > 0.95f, "P2-DSP06 converges high");
  }

  // Release envelope is slower than attack
  {
    float up = envelopeToward(1.0f, 0.0f, 0.45f, 0.04f);
    float down = envelopeToward(0.0f, 1.0f, 0.45f, 0.04f);
    expect(up > 0.4f, "envelope attack jumps up");
    expect(down > 0.9f, "envelope release stays high");
  }

  // P2-DSP07 spatial smoothing attenuates spike
  {
    std::vector<float> v(16, 0.2f);
    v[8] = 1.0f;
    spatialSmooth(v, 0.5f);
    expect(v[8] < 1.0f, "P2-DSP07 spike attenuated");
    expect(v[7] > 0.2f && v[9] > 0.2f, "P2-DSP07 neighbors raised");
  }

  // P2-DSP11 loud clamp / no NaN
  {
    std::vector<float> s(256, 1.0f);
    const float rms = computeRms(s.data(), s.size());
    float peak = 0.0f;
    const float v = volumeFromRms(rms, 0.0f, 10.0f, peak);
    expect(v <= 1.0f && !std::isnan(v), "P2-DSP11 clamped no NaN");
  }

  if (g_failures > 0) {
    std::cerr << g_failures << " test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "All DSP math tests passed\n";
  return EXIT_SUCCESS;
}
