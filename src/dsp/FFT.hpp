#pragma once

// Thin RAII wrapper around KissFFT real-FFT (kiss_fftr).
class RealFFT {
 public:
  explicit RealFFT(int nfft);
  ~RealFFT();

  RealFFT(const RealFFT&) = delete;
  RealFFT& operator=(const RealFFT&) = delete;

  int size() const { return nfft_; }
  int binCount() const { return nfft_ / 2 + 1; }

  // timedata length must be nfft_. Fills magnitudes with binCount() values.
  void forwardMagnitudes(const float* timedata, float* magnitudes);

 private:
  int nfft_ = 0;
  void* cfg_ = nullptr;  // opaque RealFFTState*
};
