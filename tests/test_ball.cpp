#include "game/Ball.hpp"

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
  constexpr float kW = 1280.0f;
  constexpr float kH = 720.0f;

  std::vector<float> flat(256, 0.5f);

  Ball::Config cfg;
  cfg.radius = 18.0f;
  cfg.follow_blend = 0.2f;
  cfg.follow_rate = 12.0f;
  cfg.damping = 0.85f;
  cfg.max_vertical_speed = 800.0f;
  cfg.y_top = 140.0f;
  cfg.y_bottom_margin = 40.0f;

  // P5-BAL01 rest on flat wave
  {
    Ball ball(cfg);
    ball.reset(kW, kH, flat);
    for (int i = 0; i < 120; ++i) {
      ball.update(1.0f / 60.0f, kW, kH, flat);
    }
    const float crest = ball.crestY(kW, kH, flat);
    const float expected = crest - cfg.radius;
    expect(approx(ball.y(), expected, 8.0f), "P5-BAL01 rest near surface");
  }

  // P5-BAL02 center X fixed
  {
    Ball ball(cfg);
    ball.reset(kW, kH, flat);
    bool ok = true;
    for (int i = 0; i < 30; ++i) {
      ball.update(1.0f / 60.0f, kW, kH, flat);
      if (!approx(ball.x(), kW * 0.5f, 0.01f)) {
        ok = false;
        break;
      }
    }
    expect(ok, "P5-BAL02 center X");
  }

  // P5-BAL03 soft-follow — higher wave lifts ball (smaller screen Y)
  {
    Ball ball(cfg);
    ball.reset(kW, kH, flat);
    std::vector<float> high(256, 0.95f);
    const float y0 = ball.y();
    ball.update(1.0f / 60.0f, kW, kH, high);
    const float y1 = ball.y();
    for (int i = 0; i < 50; ++i) {
      ball.update(1.0f / 60.0f, kW, kH, high);
    }
    const float y_end = ball.y();
    const float target = ball.crestY(kW, kH, high) - cfg.radius;
    expect(y1 < y0, "P5-BAL03 moves up toward higher wave");
    expect(std::fabs(y_end - target) < std::fabs(y0 - target),
           "P5-BAL03 closer after settle");
  }

  // P5-BAL05 speed clamp
  {
    Ball ball(cfg);
    ball.reset(kW, kH, flat);
    std::vector<float> sky(256, 1.0f);
    ball.update(1.0f, kW, kH, sky);
    expect(std::fabs(ball.vy()) <= cfg.max_vertical_speed + 1.0f,
           "P5-BAL05 speed clamped");
  }

  // P5-BAL06 crest sampling on ramp is between endpoints
  {
    Ball ball(cfg);
    std::vector<float> ramp(5);
    for (std::size_t i = 0; i < ramp.size(); ++i) {
      ramp[i] = 0.2f * static_cast<float>(i);
    }
    ball.reset(kW, kH, ramp);
    const float crest_mid = ball.crestY(kW, kH, ramp);
    const float y_bottom = kH - cfg.y_bottom_margin;
    const float span = y_bottom - cfg.y_top;
    const float crest_left = y_bottom - 0.0f * span;
    const float crest_right = y_bottom - 0.8f * span;
    expect(crest_mid < crest_left && crest_mid > crest_right,
           "P5-BAL06 mid crest between ends");
  }

  // P5-BAL08 no teleport on spike
  {
    Ball ball(cfg);
    ball.reset(kW, kH, flat);
    const float y0 = ball.y();
    std::vector<float> spike(256, 1.0f);
    ball.update(1.0f / 60.0f, kW, kH, spike);
    const float max_step = cfg.max_vertical_speed / 60.0f + 40.0f;
    expect(std::fabs(ball.y() - y0) <= max_step, "P5-BAL08 no teleport");
  }

  if (g_failures > 0) {
    std::cerr << g_failures << " test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "All ball tests passed\n";
  return EXIT_SUCCESS;
}
