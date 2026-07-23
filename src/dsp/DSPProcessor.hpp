#pragma once

#include "audio/RingBuffer.hpp"
#include "dsp/DSPMath.hpp"

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

// Reads PCM from a RingBuffer on its own thread, runs volume-mode DSP,
// and publishes a double-buffered wave height array for the render thread.
class DSPProcessor {
 public:
  struct Config {
    float volume_gate = 0.01f;
    float volume_sensitivity = 1.0f;
    float smoothing_alpha = 0.2f;
    float spatial_smooth = 0.35f;
    std::size_t wave_point_count = 256;
    std::size_t analysis_frames = 1024;
  };

  DSPProcessor();
  explicit DSPProcessor(Config config);
  ~DSPProcessor();

  DSPProcessor(const DSPProcessor&) = delete;
  DSPProcessor& operator=(const DSPProcessor&) = delete;

  // Begins the DSP thread. `buffer` must outlive stop()/destructor.
  void start(RingBuffer& buffer);
  void stop();

  bool isRunning() const { return running_.load(std::memory_order_acquire); }

  // Normalized loudness in [0, 1].
  float volume() const { return volume_.load(std::memory_order_relaxed); }

  // Instantaneous RMS before gate/normalize (for debug meters).
  float rawRms() const { return raw_rms_.load(std::memory_order_relaxed); }

  // Copies the latest published wave (normalized heights in [0, 1]).
  void copyWaveHeights(std::vector<float>& out) const;

  const Config& config() const { return config_; }

 private:
  void threadMain(RingBuffer* buffer);
  void processBlock(const float* samples, std::size_t count);
  void publishWave(const std::vector<float>& wave);

  Config config_;
  std::thread worker_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};

  std::atomic<float> volume_{0.0f};
  std::atomic<float> raw_rms_{0.0f};

  // Double-buffered wave heights. Writer fills back, then flips published_.
  std::vector<float> wave_buffers_[2];
  mutable std::atomic<int> published_{0};

  float peak_envelope_ = 0.02f;
  float phase_ = 0.0f;
  std::vector<float> wave_state_;
  std::vector<float> scratch_wave_;
  std::vector<float> sample_scratch_;
};
