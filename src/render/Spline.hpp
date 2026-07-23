#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

// Uniform Catmull-Rom: interpolates from p1 to p2 as t goes 0→1.
inline float catmullRom(float p0, float p1, float p2, float p3, float t) {
  const float t2 = t * t;
  const float t3 = t2 * t;
  return 0.5f * ((2.0f * p1) + (-p0 + p2) * t +
                 (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                 (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

// Sample a normalized wave height array at u ∈ [0, 1] with Catmull-Rom.
inline float sampleWaveHeight(const std::vector<float>& heights, float u) {
  if (heights.empty()) {
    return 0.0f;
  }
  if (heights.size() == 1) {
    return heights[0];
  }

  u = std::clamp(u, 0.0f, 1.0f);
  const float x = u * static_cast<float>(heights.size() - 1);
  const int i1 = static_cast<int>(x);
  const int i2 = std::min(i1 + 1, static_cast<int>(heights.size()) - 1);
  const int i0 = std::max(i1 - 1, 0);
  const int i3 = std::min(i2 + 1, static_cast<int>(heights.size()) - 1);
  const float t = x - static_cast<float>(i1);
  return catmullRom(heights[static_cast<std::size_t>(i0)],
                    heights[static_cast<std::size_t>(i1)],
                    heights[static_cast<std::size_t>(i2)],
                    heights[static_cast<std::size_t>(i3)], t);
}

// Build a dense height polyline (still normalized [0,1]) with optional
// horizontal phase oscillation applied in sample-parameter space.
inline void resampleWaveDense(const std::vector<float>& heights,
                              std::vector<float>& dense_out,
                              std::size_t count,
                              float osc_phase_radians,
                              float osc_strength) {
  dense_out.resize(count);
  if (count == 0) {
    return;
  }
  for (std::size_t i = 0; i < count; ++i) {
    float u = (count == 1) ? 0.0f
                           : static_cast<float>(i) / static_cast<float>(count - 1);
    if (osc_strength != 0.0f) {
      u += osc_strength *
           std::sin(osc_phase_radians + u * 6.2831853f * 2.0f);
      u = std::clamp(u, 0.0f, 1.0f);
    }
    dense_out[i] = sampleWaveHeight(heights, u);
  }
}
