#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// Pure helpers used by DSPProcessor (easy to unit-test without threads/audio).

inline float computeRms(const float* samples, std::size_t count) {
  if (samples == nullptr || count == 0) {
    return 0.0f;
  }
  double energy = 0.0;
  for (std::size_t i = 0; i < count; ++i) {
    const double s = samples[i];
    energy += s * s;
  }
  return static_cast<float>(std::sqrt(energy / static_cast<double>(count)));
}

// Gate quiet noise, then normalize against a slow-decaying peak envelope.
// Returns volume in [0, 1]. Updates peak_envelope in/out.
inline float volumeFromRms(float rms,
                           float gate,
                           float sensitivity,
                           float& peak_envelope,
                           float peak_attack = 1.0f,
                           float peak_decay = 0.995f) {
  if (rms > peak_envelope) {
    peak_envelope = peak_attack * rms + (1.0f - peak_attack) * peak_envelope;
  } else {
    peak_envelope = peak_decay * peak_envelope + (1.0f - peak_decay) * rms;
  }

  if (rms < gate) {
    return 0.0f;
  }
  if (peak_envelope < 1.0e-6f) {
    return 0.0f;
  }

  float v = (rms / peak_envelope) * sensitivity;
  if (v < 0.0f) {
    v = 0.0f;
  }
  if (v > 1.0f) {
    v = 1.0f;
  }
  return v;
}

// Temporal EMA: out = alpha * target + (1-alpha) * previous.
inline void temporalSmooth(const std::vector<float>& target,
                           std::vector<float>& state,
                           float alpha) {
  if (state.size() != target.size()) {
    state = target;
    return;
  }
  const float one_minus = 1.0f - alpha;
  for (std::size_t i = 0; i < target.size(); ++i) {
    state[i] = alpha * target[i] + one_minus * state[i];
  }
}

// Neighbor blend. amount in [0, 1].
inline void spatialSmooth(std::vector<float>& values, float amount) {
  if (values.size() < 3 || amount <= 0.0f) {
    return;
  }
  std::vector<float> tmp = values;
  const float keep = 1.0f - amount;
  const float side = amount * 0.5f;
  for (std::size_t i = 1; i + 1 < values.size(); ++i) {
    values[i] = keep * tmp[i] + side * (tmp[i - 1] + tmp[i + 1]);
  }
}

// Build a water-like profile from a single volume control in [0, 1].
// Heights are normalized [0, 1] (0 = bottom of control range, 1 = top).
inline void buildWaveFromVolume(std::vector<float>& out,
                                float volume,
                                float phase_radians) {
  const std::size_t n = out.size();
  if (n == 0) {
    return;
  }
  constexpr float kRest = 0.22f;
  constexpr float kLift = 0.55f;
  for (std::size_t i = 0; i < n; ++i) {
    const float t = (n == 1) ? 0.0f
                             : static_cast<float>(i) / static_cast<float>(n - 1);
    const float ripple =
        0.06f * volume *
        std::sin(t * 6.2831853f * 3.0f + phase_radians);
    float h = kRest + volume * kLift + ripple;
    if (h < 0.0f) {
      h = 0.0f;
    }
    if (h > 1.0f) {
      h = 1.0f;
    }
    out[i] = h;
  }
}
