#include "dsp/DSPProcessor.hpp"

#include <algorithm>
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
  if (config_.fft_size < 64) {
    config_.fft_size = 64;
  }
  if (config_.fft_size % 2 != 0) {
    ++config_.fft_size;
  }
  if (config_.analysis_frames < static_cast<std::size_t>(config_.fft_size)) {
    config_.analysis_frames = static_cast<std::size_t>(config_.fft_size);
  }

  wave_buffers_[0].assign(config_.wave_point_count, 0.22f);
  wave_buffers_[1].assign(config_.wave_point_count, 0.22f);
  wave_state_.assign(config_.wave_point_count, 0.22f);
  scratch_wave_.assign(config_.wave_point_count, 0.22f);
  sample_scratch_.resize(config_.analysis_frames);
  windowed_.resize(static_cast<std::size_t>(config_.fft_size), 0.0f);
  fft_ = std::make_unique<RealFFT>(config_.fft_size);
  magnitudes_.resize(static_cast<std::size_t>(fft_->binCount()), 0.0f);
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
  out = wave_buffers_[idx];
}

void DSPProcessor::publishWave(const std::vector<float>& wave) {
  const int front = published_.load(std::memory_order_relaxed);
  const int back = 1 - front;
  wave_buffers_[back] = wave;
  published_.store(back, std::memory_order_release);
}

void DSPProcessor::decayTowardSilence() {
  float vol = volume_.load(std::memory_order_relaxed) * 0.92f;
  float pitch = pitch_.load(std::memory_order_relaxed) * 0.92f;
  if (vol < 0.001f) {
    vol = 0.0f;
  }
  if (pitch < 0.001f) {
    pitch = 0.0f;
  }
  volume_.store(vol, std::memory_order_relaxed);
  pitch_.store(pitch, std::memory_order_relaxed);
  raw_rms_.store(raw_rms_.load(std::memory_order_relaxed) * 0.92f,
                 std::memory_order_relaxed);
  if (pitch <= 0.0f) {
    pitch_hz_.store(0.0f, std::memory_order_relaxed);
  }

  const float control =
      (mode() == ControlMode::Volume) ? vol : pitch;
  phase_ += 0.08f;
  buildWaveFromVolume(scratch_wave_, control, phase_);
  temporalSmooth(scratch_wave_, wave_state_, config_.smoothing_alpha);
  spatialSmooth(wave_state_, config_.spatial_smooth);
  publishWave(wave_state_);
}

void DSPProcessor::processBlock(const float* samples, std::size_t count) {
  const float rms = computeRms(samples, count);
  raw_rms_.store(rms, std::memory_order_relaxed);

  const float vol = volumeFromRms(rms,
                                  config_.volume_gate,
                                  config_.volume_sensitivity,
                                  peak_envelope_);
  volume_.store(vol, std::memory_order_relaxed);

  // --- Frequency path (always computed so meters stay live) ---
  float pitch_unit = 0.0f;
  float pitch_hz = 0.0f;

  const int nfft = config_.fft_size;
  const std::size_t need = static_cast<std::size_t>(nfft);
  std::fill(windowed_.begin(), windowed_.end(), 0.0f);
  const std::size_t copy_n = std::min(count, need);
  // Prefer the newest samples when we have extra.
  const std::size_t offset = (count > copy_n) ? (count - copy_n) : 0;
  std::copy_n(samples + offset, copy_n, windowed_.begin());
  applyHannWindowInPlace(windowed_.data(), need);
  fft_->forwardMagnitudes(windowed_.data(), magnitudes_.data());

  const DominantFrequency dom = findDominantFrequency(
      magnitudes_.data(),
      fft_->binCount(),
      config_.sample_rate,
      nfft,
      config_.pitch_min_hz,
      config_.pitch_max_hz);

  // Silence / noise gate: weak RMS or peak not standing out from the mean.
  double mag_sum = 0.0;
  const int bins = fft_->binCount();
  for (int i = 1; i < bins; ++i) {
    mag_sum += magnitudes_[static_cast<std::size_t>(i)];
  }
  const float mag_mean =
      (bins > 1) ? static_cast<float>(mag_sum / static_cast<double>(bins - 1))
                 : 0.0f;

  const bool voiced =
      rms >= config_.volume_gate &&
      dom.magnitude > mag_mean * config_.pitch_peak_ratio &&
      dom.magnitude > 1.0e-4f;

  if (voiced) {
    pitch_hz = dom.hz;
    pitch_unit = mapPitchHzToUnit(dom.hz,
                                  config_.pitch_min_hz,
                                  300.0f,
                                  1000.0f,
                                  config_.pitch_max_hz);
  } else {
    pitch_unit = pitch_.load(std::memory_order_relaxed) * 0.85f;
    if (pitch_unit < 0.001f) {
      pitch_unit = 0.0f;
      pitch_hz = 0.0f;
    } else {
      pitch_hz = pitch_hz_.load(std::memory_order_relaxed);
    }
  }

  pitch_.store(pitch_unit, std::memory_order_relaxed);
  pitch_hz_.store(pitch_hz, std::memory_order_relaxed);

  const float control =
      (mode() == ControlMode::Volume) ? vol : pitch_unit;

  phase_ += 0.12f + control * 0.25f;
  if (phase_ > 6.2831853f * 100.0f) {
    phase_ = std::fmod(phase_, 6.2831853f);
  }

  buildWaveFromVolume(scratch_wave_, control, phase_);

  // In frequency mode, blend in a coarse spectrum envelope for visual character.
  if (mode() == ControlMode::Frequency && bins > 2) {
    const std::size_t n = scratch_wave_.size();
    for (std::size_t i = 0; i < n; ++i) {
      const float t = (n == 1) ? 0.0f
                               : static_cast<float>(i) /
                                     static_cast<float>(n - 1);
      const int b = 1 + static_cast<int>(t * static_cast<float>(bins - 2));
      const float env = magnitudes_[static_cast<std::size_t>(b)] /
                        std::max(dom.magnitude, 1.0e-4f);
      scratch_wave_[i] =
          std::min(1.0f, scratch_wave_[i] * 0.75f + 0.20f * env * control);
    }
  }

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
      decayTowardSilence();
    }

    std::this_thread::sleep_for(5ms);
  }
}
