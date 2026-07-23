#include "audio/AudioInput.hpp"

#include <portaudio.h>

#include <string>

namespace {

int audioInputPaCallback(const void* input,
                         void* /*output*/,
                         unsigned long frame_count,
                         const PaStreamCallbackTimeInfo* /*time_info*/,
                         PaStreamCallbackFlags /*status_flags*/,
                         void* user_data) {
  auto* self = static_cast<AudioInput*>(user_data);
  self->handleCallback(static_cast<const float*>(input), frame_count);
  return paContinue;
}

}  // namespace

AudioInput::AudioInput() : AudioInput(Config{}) {}

AudioInput::AudioInput(Config config)
    : config_(config),
      ring_(std::make_unique<RingBuffer>(config.ring_capacity)),
      sample_rate_(config.sample_rate) {}

AudioInput::~AudioInput() {
  stop();
  if (pa_initialized_) {
    Pa_Terminate();
    pa_initialized_ = false;
  }
}

bool AudioInput::start() {
  last_error_.clear();
  if (running_) {
    return true;
  }

  if (!pa_initialized_) {
    const PaError init_err = Pa_Initialize();
    if (init_err != paNoError) {
      last_error_ =
          std::string("Pa_Initialize failed: ") + Pa_GetErrorText(init_err);
      return false;
    }
    pa_initialized_ = true;
  }

  PaDeviceIndex device = config_.device_index;
  if (device < 0) {
    device = Pa_GetDefaultInputDevice();
  }
  if (device == paNoDevice) {
    last_error_ = "No default input device (check mic / permissions)";
    return false;
  }

  const PaDeviceInfo* info = Pa_GetDeviceInfo(device);
  if (info == nullptr) {
    last_error_ =
        "Pa_GetDeviceInfo returned null for device " + std::to_string(device);
    return false;
  }

  if (info->maxInputChannels < 1) {
    last_error_ = "Selected device has no input channels";
    return false;
  }

  channel_count_ = 1;

  PaStreamParameters input_params{};
  input_params.device = device;
  input_params.channelCount = channel_count_;
  input_params.sampleFormat = paFloat32;
  input_params.suggestedLatency = info->defaultLowInputLatency;
  input_params.hostApiSpecificStreamInfo = nullptr;

  sample_rate_ = config_.sample_rate;

  PaStream* stream = nullptr;
  PaError err = Pa_OpenStream(&stream,
                              &input_params,
                              nullptr,
                              sample_rate_,
                              config_.frames_per_buffer,
                              paClipOff,
                              audioInputPaCallback,
                              this);

  // Some devices reject mono — retry stereo and downmix in handleCallback.
  if (err != paNoError && info->maxInputChannels >= 2) {
    channel_count_ = 2;
    input_params.channelCount = 2;
    err = Pa_OpenStream(&stream,
                        &input_params,
                        nullptr,
                        sample_rate_,
                        config_.frames_per_buffer,
                        paClipOff,
                        audioInputPaCallback,
                        this);
  }

  if (err != paNoError) {
    last_error_ = std::string("Pa_OpenStream failed: ") + Pa_GetErrorText(err);
    return false;
  }

  err = Pa_StartStream(stream);
  if (err != paNoError) {
    last_error_ = std::string("Pa_StartStream failed: ") + Pa_GetErrorText(err) +
                  " (on macOS: allow Microphone for your terminal/Cursor)";
    Pa_CloseStream(stream);
    return false;
  }

  stream_ = stream;
  running_ = true;
  return true;
}

void AudioInput::stop() {
  if (stream_ != nullptr) {
    PaStream* stream = static_cast<PaStream*>(stream_);
    if (running_) {
      Pa_StopStream(stream);
    }
    Pa_CloseStream(stream);
    stream_ = nullptr;
  }
  running_ = false;
}

void AudioInput::handleCallback(const float* input, unsigned long frame_count) {
  if (input == nullptr || frame_count == 0) {
    return;
  }

  // Real-time rule: no heap allocation. Stack temp for stereo→mono.
  constexpr unsigned long kMaxStackFrames = 2048;
  const unsigned long n = frame_count;
  if (n > kMaxStackFrames) {
    return;
  }

  if (channel_count_ <= 1) {
    ring_->write(input, static_cast<std::size_t>(n));
    return;
  }

  float mono[kMaxStackFrames];
  const unsigned long ch = static_cast<unsigned long>(channel_count_);
  for (unsigned long i = 0; i < n; ++i) {
    mono[i] = input[i * ch];
  }
  ring_->write(mono, static_cast<std::size_t>(n));
}
