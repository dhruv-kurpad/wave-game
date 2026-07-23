#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// Hann window in-place (or into out). Endpoints ≈ 0, center ≈ 1.
inline void applyHannWindow(const float* in, float* out, std::size_t n) {
  if (in == nullptr || out == nullptr || n == 0) {
    return;
  }
  if (n == 1) {
    out[0] = in[0];
    return;
  }
  constexpr float kPi = 3.14159265358979323846f;
  for (std::size_t i = 0; i < n; ++i) {
    const float w =
        0.5f * (1.0f - std::cos(2.0f * kPi * static_cast<float>(i) /
                                 static_cast<float>(n - 1)));
    out[i] = in[i] * w;
  }
}

inline void applyHannWindowInPlace(float* data, std::size_t n) {
  if (data == nullptr || n == 0) {
    return;
  }
  if (n == 1) {
    return;
  }
  constexpr float kPi = 3.14159265358979323846f;
  for (std::size_t i = 0; i < n; ++i) {
    const float w =
        0.5f * (1.0f - std::cos(2.0f * kPi * static_cast<float>(i) /
                                 static_cast<float>(n - 1)));
    data[i] *= w;
  }
}

struct DominantFrequency {
  float hz = 0.0f;
  float magnitude = 0.0f;
  int bin = 0;
};

// Magnitude spectrum length is typically nfft/2+1.
// Skips DC and bins below min_hz. Uses parabolic interpolation around the peak.
inline DominantFrequency findDominantFrequency(const float* magnitudes,
                                               int bin_count,
                                               float sample_rate,
                                               int nfft,
                                               float min_hz,
                                               float max_hz) {
  DominantFrequency result;
  if (magnitudes == nullptr || bin_count < 3 || nfft <= 0 || sample_rate <= 0.0f) {
    return result;
  }

  const float hz_per_bin = sample_rate / static_cast<float>(nfft);
  int min_bin = static_cast<int>(std::ceil(min_hz / hz_per_bin));
  int max_bin = static_cast<int>(std::floor(max_hz / hz_per_bin));
  if (min_bin < 1) {
    min_bin = 1;  // skip DC
  }
  if (max_bin >= bin_count) {
    max_bin = bin_count - 1;
  }
  if (min_bin > max_bin) {
    return result;
  }

  int peak_bin = min_bin;
  float peak_mag = magnitudes[min_bin];
  for (int k = min_bin + 1; k <= max_bin; ++k) {
    if (magnitudes[k] > peak_mag) {
      peak_mag = magnitudes[k];
      peak_bin = k;
    }
  }

  float refined = static_cast<float>(peak_bin);
  if (peak_bin > min_bin && peak_bin < max_bin) {
    const float y1 = magnitudes[peak_bin - 1];
    const float y2 = magnitudes[peak_bin];
    const float y3 = magnitudes[peak_bin + 1];
    const float denom = y1 - 2.0f * y2 + y3;
    if (std::fabs(denom) > 1.0e-12f) {
      refined = static_cast<float>(peak_bin) + 0.5f * (y1 - y3) / denom;
    }
  }

  result.bin = peak_bin;
  result.magnitude = peak_mag;
  result.hz = refined * hz_per_bin;
  if (result.hz < min_hz) {
    result.hz = min_hz;
  }
  if (result.hz > max_hz) {
    result.hz = max_hz;
  }
  return result;
}

// Map Hz through the game bands into [0, 1]:
// <300 low, 300–1000 mid, >1000 high (clamped to pitch_max).
inline float mapPitchHzToUnit(float hz,
                              float pitch_min_hz = 80.0f,
                              float low_edge = 300.0f,
                              float mid_edge = 1000.0f,
                              float pitch_max_hz = 2000.0f) {
  if (hz <= pitch_min_hz) {
    return 0.0f;
  }
  if (hz >= pitch_max_hz) {
    return 1.0f;
  }
  if (hz < low_edge) {
    return (1.0f / 3.0f) * (hz - pitch_min_hz) / (low_edge - pitch_min_hz);
  }
  if (hz < mid_edge) {
    return (1.0f / 3.0f) +
           (1.0f / 3.0f) * (hz - low_edge) / (mid_edge - low_edge);
  }
  return (2.0f / 3.0f) +
         (1.0f / 3.0f) * (hz - mid_edge) / (pitch_max_hz - mid_edge);
}
