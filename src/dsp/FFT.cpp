#include "dsp/FFT.hpp"

#include "kiss_fftr.h"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace {

struct RealFFTState {
  kiss_fftr_cfg cfg = nullptr;
  std::vector<kiss_fft_cpx> freq;
};

}  // namespace

RealFFT::RealFFT(int nfft) : nfft_(nfft) {
  if (nfft_ < 4 || (nfft_ % 2) != 0) {
    throw std::invalid_argument("RealFFT: nfft must be even and >= 4");
  }
  auto* state = new RealFFTState();
  state->cfg = kiss_fftr_alloc(nfft_, 0, nullptr, nullptr);
  if (state->cfg == nullptr) {
    delete state;
    throw std::runtime_error("RealFFT: kiss_fftr_alloc failed");
  }
  state->freq.resize(static_cast<std::size_t>(binCount()));
  cfg_ = state;
}

RealFFT::~RealFFT() {
  if (cfg_ != nullptr) {
    auto* state = static_cast<RealFFTState*>(cfg_);
    kiss_fftr_free(state->cfg);
    delete state;
    cfg_ = nullptr;
  }
}

void RealFFT::forwardMagnitudes(const float* timedata, float* magnitudes) {
  if (timedata == nullptr || magnitudes == nullptr || cfg_ == nullptr) {
    return;
  }

  auto* state = static_cast<RealFFTState*>(cfg_);
  kiss_fftr(state->cfg, timedata, state->freq.data());

  const int bins = binCount();
  for (int i = 0; i < bins; ++i) {
    const float re = state->freq[static_cast<std::size_t>(i)].r;
    const float im = state->freq[static_cast<std::size_t>(i)].i;
    magnitudes[i] = std::sqrt(re * re + im * im);
  }
}
