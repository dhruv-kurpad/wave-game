#include "render/Spline.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
int g_failures = 0;

void expect(bool cond, const char* name) {
  if (!cond) {
    std::cerr << "FAIL: " << name << '\n';
    ++g_failures;
  } else {
    std::cout << "PASS: " << name << '\n';
  }
}

bool approx(float a, float b, float eps) { return std::fabs(a - b) <= eps; }
}  // namespace

int main() {
  // P4-WAV01 Catmull-Rom hits endpoints of the segment (t=0 → p1, t=1 → p2)
  {
    const float y = catmullRom(0.0f, 1.0f, 2.0f, 3.0f, 0.0f);
    const float z = catmullRom(0.0f, 1.0f, 2.0f, 3.0f, 1.0f);
    expect(approx(y, 1.0f, 1e-5f), "P4-WAV01 t=0 is p1");
    expect(approx(z, 2.0f, 1e-5f), "P4-WAV01 t=1 is p2");
  }

  // Continuity / no NaN on dense resample
  {
    std::vector<float> h{0.1f, 0.4f, 0.2f, 0.8f, 0.3f};
    std::vector<float> dense;
    resampleWaveDense(h, dense, 64, 0.0f, 0.0f);
    expect(dense.size() == 64, "P4-WAV01 dense size");
    bool ok = true;
    for (float v : dense) {
      if (std::isnan(v) || v < -0.01f || v > 1.01f) {
        ok = false;
      }
    }
    expect(ok, "P4-WAV01 dense finite in range");
  }

  // P4-WAV02 bounds: u=0 and u=1 match endpoints
  {
    std::vector<float> h{0.25f, 0.5f, 0.75f};
    expect(approx(sampleWaveHeight(h, 0.0f), 0.25f, 1e-4f), "P4-WAV02 u=0");
    expect(approx(sampleWaveHeight(h, 1.0f), 0.75f, 1e-4f), "P4-WAV02 u=1");
    const float mid = sampleWaveHeight(h, 0.5f);
    expect(mid > 0.25f && mid < 0.75f, "P4-WAV02 mid between ends");
  }

  // Mid-segment between neighbors on a linear ramp (approx)
  {
    std::vector<float> ramp(5);
    for (std::size_t i = 0; i < ramp.size(); ++i) {
      ramp[i] = 0.2f * static_cast<float>(i);
    }
    const float a = sampleWaveHeight(ramp, 0.25f);
    const float b = sampleWaveHeight(ramp, 0.5f);
    expect(a < b, "P4-WAV02 ramp increasing");
  }

  if (g_failures > 0) {
    std::cerr << g_failures << " test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "All spline tests passed\n";
  return EXIT_SUCCESS;
}
