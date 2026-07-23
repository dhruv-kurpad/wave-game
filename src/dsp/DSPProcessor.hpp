#pragma once

#include "audio/RingBuffer.hpp"
#include "dsp/DSPMath.hpp"
#include "dsp/FFT.hpp"
#include "dsp/PitchMath.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

enum class ControlMode : int {
  Volume = 0,
  Frequency = 1,
};

// Reads PCM from a RingBuffer on its own thread, runs volume or frequency DSP,
// and publishes a double-buffered wave height array for the render thread.
class DSPProcessor {
 public:
  struct Config {
    float volume_gate = 0.01f;
    float volume_sensitivity = 1.0f;
    // Wave temporal smooth: attack (rise) vs release (fall).
    float smoothing_alpha = 0.25f;
    float smoothing_release_alpha = 0.05f;
    float spatial_smooth = 0.35f;
    // Control envelope per DSP tick (~5 ms): fast rise, slow fall.
    float control_attack = 0.45f;
    float control_release = 0.04f;
    std::size_t wave_point_count = 256;
    std::size_t analysis_frames = 1024;
    int fft_size = 1024;
    float sample_rate = 44100.0f;
    float pitch_min_hz = 80.0f;
    float pitch_max_hz = 2000.0f;
    // Peak bin must exceed this fraction of the spectrum mean (after gate).
    float pitch_peak_ratio = 8.0f;
  };

  DSPProcessor();
  explicit DSPProcessor(Config config);
  ~DSPProcessor();

  DSPProcessor(const DSPProcessor&) = delete;
  DSPProcessor& operator=(const DSPProcessor&) = delete;

  void start(RingBuffer& buffer);
  void stop();

  bool isRunning() const { return running_.load(std::memory_order_acquire); }

  void setMode(ControlMode mode) {
    mode_.store(static_cast<int>(mode), std::memory_order_relaxed);
  }
  ControlMode mode() const {
    return static_cast<ControlMode>(mode_.load(std::memory_order_relaxed));
  }

  float volume() const { return volume_.load(std::memory_order_relaxed); }
  float rawRms() const { return raw_rms_.load(std::memory_order_relaxed); }

  // Normalized pitch control in [0, 1] (frequency mode).
  float pitch() const { return pitch_.load(std::memory_order_relaxed); }
  float pitchHz() const { return pitch_hz_.load(std::memory_order_relaxed); }

  // Active control used for the wave (volume or pitch depending on mode).
  float controlValue() const {
    return (mode() == ControlMode::Volume) ? volume() : pitch();
  }

  void copyWaveHeights(std::vector<float>& out) const;

  const Config& config() const { return config_; }

 private:
  void threadMain(RingBuffer* buffer);
  void processBlock(const float* samples, std::size_t count);
  void publishWave(const std::vector<float>& wave);
  void decayTowardSilence();

  Config config_;
  std::thread worker_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};
  std::atomic<int> mode_{static_cast<int>(ControlMode::Volume)};

  std::atomic<float> volume_{0.0f};
  std::atomic<float> raw_rms_{0.0f};
  std::atomic<float> pitch_{0.0f};
  std::atomic<float> pitch_hz_{0.0f};

  std::vector<float> wave_buffers_[2];
  mutable std::atomic<int> published_{0};

  float peak_envelope_ = 0.02f;
  float phase_ = 0.0f;
  float volume_env_ = 0.0f;
  float pitch_env_ = 0.0f;
  std::vector<float> wave_state_;
  std::vector<float> scratch_wave_;
  std::vector<float> sample_scratch_;
  std::vector<float> windowed_;
  std::vector<float> magnitudes_;
  std::unique_ptr<RealFFT> fft_;
};
