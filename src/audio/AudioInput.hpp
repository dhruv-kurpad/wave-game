#pragma once

#include "audio/RingBuffer.hpp"

#include <cstddef>
#include <memory>
#include <string>

// PortAudio microphone capture → RingBuffer.
// The PortAudio callback only writes PCM; it must never allocate or do DSP.
class AudioInput {
 public:
  struct Config {
    double sample_rate = 44100.0;
    unsigned long frames_per_buffer = 1024;
    int device_index = -1;  // -1 = PortAudio default input
    std::size_t ring_capacity = 16384;
  };

  AudioInput();
  explicit AudioInput(Config config);
  ~AudioInput();

  AudioInput(const AudioInput&) = delete;
  AudioInput& operator=(const AudioInput&) = delete;

  // Opens the device and starts the stream. On failure, lastError() is set.
  bool start();
  void stop();

  bool isRunning() const { return running_; }
  const std::string& lastError() const { return last_error_; }

  RingBuffer& buffer() { return *ring_; }
  const RingBuffer& buffer() const { return *ring_; }

  double sampleRate() const { return sample_rate_; }
  int channelCount() const { return channel_count_; }
  unsigned long framesPerBuffer() const { return config_.frames_per_buffer; }

  // Invoked from the PortAudio realtime callback (keep it allocation-free).
  void handleCallback(const float* input, unsigned long frame_count);

 private:
  Config config_;
  std::unique_ptr<RingBuffer> ring_;
  void* stream_ = nullptr;  // PaStream*
  bool pa_initialized_ = false;
  bool running_ = false;
  double sample_rate_ = 44100.0;
  int channel_count_ = 1;
  std::string last_error_;
};
