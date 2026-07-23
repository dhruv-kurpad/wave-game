#include "dsp/DSPProcessor.hpp"

#include <chrono>
#include <cmath>

DSPProcessor::DSPProcessor() : DSPProcessor(Config{}) {}

DSPProcessor::DSPProcessor(Config config) : config_(config) {
  if (config_.wave_point_count < 200) {
    config_.wave_point_count = 200;
  }
  if (config_.wave_point_count > 300) {
    config_.wave_point_count = 300;
  }

  wave_buffers_[0].assign(config_.wave_point_count, 0.22f);
  wave_buffers_[1].assign(config_.wave_point_count, 0.22f);
  wave_state_.assign(config_.wave_point_count, 0.22f);
  scratch_wave_.assign(config_.wave_point_count, 0.22f);
  sample_scratch_.resize(config_.analysis_frames);
}

DSPProcessor::~DSPProcessor() { stop(); }

void DSPProcessor::start(RingBuffer& buffer) {
  if (running_.load(std::memory_order_acquire)) {
    return;
  }
  stop_requested_.store(false, std::memory_order_release);
  running_.store(true, std::memory_order_release);
  worker_ = std::thread(&DSPProcessor::threadMain, this, &buffer);
}

void DSPProcessor::stop() {
  stop_requested_.store(true, std::memory_order_release);
  if (worker_.joinable()) {
    worker_.join();
  }
  running_.store(false, std::memory_order_release);
}

void DSPProcessor::copyWaveHeights(std::vector<float>& out) const {
  const int idx = published_.load(std::memory_order_acquire);
  const std::vector<float>& src = wave_buffers_[idx];
  out = src;
}

void DSPProcessor::publishWave(const std::vector<float>& wave) {
  const int front = published_.load(std::memory_order_relaxed);
  const int back = 1 - front;
  wave_buffers_[back] = wave;
  published_.store(back, std::memory_order_release);
}

void DSPProcessor::processBlock(const float* samples, std::size_t count) {
  const float rms = computeRms(samples, count);
  raw_rms_.store(rms, std::memory_order_relaxed);

  const float vol = volumeFromRms(rms,
                                  config_.volume_gate,
                                  config_.volume_sensitivity,
                                  peak_envelope_);
  volume_.store(vol, std::memory_order_relaxed);

  phase_ += 0.12f + vol * 0.25f;
  if (phase_ > 6.2831853f * 100.0f) {
    phase_ = std::fmod(phase_, 6.2831853f);
  }

  buildWaveFromVolume(scratch_wave_, vol, phase_);
  temporalSmooth(scratch_wave_, wave_state_, config_.smoothing_alpha);
  spatialSmooth(wave_state_, config_.spatial_smooth);
  publishWave(wave_state_);
}

void DSPProcessor::threadMain(RingBuffer* buffer) {
  using namespace std::chrono_literals;

  while (!stop_requested_.load(std::memory_order_acquire)) {
    std::size_t total = 0;
    while (total < sample_scratch_.size()) {
      const std::size_t n = buffer->read(sample_scratch_.data() + total,
                                         sample_scratch_.size() - total);
      if (n == 0) {
        break;
      }
      total += n;
    }

    if (total > 0) {
      processBlock(sample_scratch_.data(), total);
    } else {
      // No new audio: gently decay displayed volume toward silence.
      float vol = volume_.load(std::memory_order_relaxed) * 0.92f;
      if (vol < 0.001f) {
        vol = 0.0f;
      }
      volume_.store(vol, std::memory_order_relaxed);
      raw_rms_.store(raw_rms_.load(std::memory_order_relaxed) * 0.92f,
                     std::memory_order_relaxed);

      phase_ += 0.08f;
      buildWaveFromVolume(scratch_wave_, vol, phase_);
      temporalSmooth(scratch_wave_, wave_state_, config_.smoothing_alpha);
      spatialSmooth(wave_state_, config_.spatial_smooth);
      publishWave(wave_state_);
    }

    std::this_thread::sleep_for(5ms);
  }
}
