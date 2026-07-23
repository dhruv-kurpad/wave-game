#include "dsp/FFT.hpp"
#include "dsp/PitchMath.hpp"

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

std::vector<float> makeSine(float freq_hz, float amp, int n, float sr) {
  std::vector<float> s(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    s[static_cast<std::size_t>(i)] =
        amp * std::sin(2.0f * 3.14159265f * freq_hz * static_cast<float>(i) / sr);
  }
  return s;
}
}  // namespace

int main() {
  // P3-DSP01 Hann endpoints ~0, center peak
  {
    std::vector<float> ones(64, 1.0f);
    applyHannWindowInPlace(ones.data(), ones.size());
    expect(ones.front() < 0.01f && ones.back() < 0.01f, "P3-DSP01 hann ends");
    expect(ones[32] > 0.9f, "P3-DSP01 hann center");
  }

  constexpr int kNfft = 1024;
  constexpr float kSr = 44100.0f;
  RealFFT fft(kNfft);
  std::vector<float> mags(static_cast<std::size_t>(fft.binCount()));

  auto detect = [&](float freq) {
    auto s = makeSine(freq, 0.5f, kNfft, kSr);
    applyHannWindowInPlace(s.data(), s.size());
    fft.forwardMagnitudes(s.data(), mags.data());
    return findDominantFrequency(mags.data(), fft.binCount(), kSr, kNfft, 80.0f,
                                 2000.0f);
  };

  // P3-DSP02 440 Hz
  {
    const auto d = detect(440.0f);
    const float bin_w = kSr / static_cast<float>(kNfft);
    expect(approx(d.hz, 440.0f, bin_w), "P3-DSP02 dominant ~440");
  }

  // P3-DSP03 200 Hz → low band mapping
  {
    const auto d = detect(200.0f);
    const float u = mapPitchHzToUnit(d.hz);
    expect(u < 1.0f / 3.0f, "P3-DSP03 maps low");
  }

  // P3-DSP04 600 Hz → mid
  {
    const auto d = detect(600.0f);
    const float u = mapPitchHzToUnit(d.hz);
    expect(u >= 1.0f / 3.0f && u < 2.0f / 3.0f, "P3-DSP04 maps mid");
  }

  // P3-DSP05 1500 Hz → high
  {
    const auto d = detect(1500.0f);
    const float u = mapPitchHzToUnit(d.hz);
    expect(u >= 2.0f / 3.0f, "P3-DSP05 maps high");
  }

  // P3-DSP06 ignore DC — DC-heavy signal should not report ~0 as dominant in range
  {
    std::vector<float> s(static_cast<std::size_t>(kNfft), 0.5f);
    for (int i = 0; i < kNfft; ++i) {
      s[static_cast<std::size_t>(i)] +=
          0.05f * std::sin(2.0f * 3.14159265f * 440.0f *
                           static_cast<float>(i) / kSr);
    }
    applyHannWindowInPlace(s.data(), s.size());
    fft.forwardMagnitudes(s.data(), mags.data());
    const auto d = findDominantFrequency(mags.data(), fft.binCount(), kSr,
                                         kNfft, 80.0f, 2000.0f);
    expect(d.hz > 50.0f, "P3-DSP06 not DC");
  }

  // P3-DSP07 silence stability — noise floor shouldn't map to high pitch unit
  // (raw detector may pick a bin; mapping + gate handled in DSPProcessor)
  {
    std::vector<float> s(static_cast<std::size_t>(kNfft), 0.0f);
    applyHannWindowInPlace(s.data(), s.size());
    fft.forwardMagnitudes(s.data(), mags.data());
    float max_mag = 0.0f;
    for (float m : mags) {
      max_mag = std::max(max_mag, m);
    }
    expect(max_mag < 1.0e-3f, "P3-DSP07 silence spectrum tiny");
  }

  // Band edges of mapPitchHzToUnit
  {
    expect(approx(mapPitchHzToUnit(80.0f), 0.0f, 0.01f), "map at min");
    expect(mapPitchHzToUnit(300.0f) > 0.3f && mapPitchHzToUnit(300.0f) < 0.35f,
           "map at 300");
    expect(mapPitchHzToUnit(1000.0f) > 0.65f && mapPitchHzToUnit(1000.0f) < 0.68f,
           "map at 1000");
    expect(approx(mapPitchHzToUnit(2000.0f), 1.0f, 0.01f), "map at max");
  }

  if (g_failures > 0) {
    std::cerr << g_failures << " test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "All pitch/FFT tests passed\n";
  return EXIT_SUCCESS;
}
