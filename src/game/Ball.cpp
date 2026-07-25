#include "game/Ball.hpp"

#include "render/Spline.hpp"

#include <algorithm>
#include <cmath>

Ball::Ball() : Ball(Config{}) {}

Ball::Ball(Config config) : config_(config) {}

float Ball::mapCrestY(float normalized_height, float window_height) const {
  const float y_top = config_.y_top;
  const float y_bottom = window_height - config_.y_bottom_margin;
  const float h = std::clamp(normalized_height, 0.0f, 1.0f);
  return y_bottom - h * (y_bottom - y_top);
}

float Ball::crestY(float window_width,
                   float window_height,
                   const std::vector<float>& wave_heights) const {
  if (wave_heights.empty() || window_width <= 1.0f) {
    return window_height * 0.5f;
  }
  const float u = std::clamp(x_ / (window_width - 1.0f), 0.0f, 1.0f);
  return mapCrestY(sampleWaveHeight(wave_heights, u), window_height);
}

void Ball::reset(float window_width,
                 float window_height,
                 const std::vector<float>& wave_heights) {
  x_ = window_width * 0.5f;
  const float crest = crestY(window_width, window_height, wave_heights);
  y_ = crest - config_.radius;
  vy_ = 0.0f;
  in_contact_ = true;
  contact_impulse_ = 0.0f;
}

void Ball::update(float dt,
                  float window_width,
                  float window_height,
                  const std::vector<float>& wave_heights) {
  if (dt <= 0.0f) {
    dt = 1.0f / 60.0f;
  }
  // Clamp huge hitches so spikes cannot teleport the ball.
  dt = std::min(dt, 0.05f);

  x_ = window_width * 0.5f;
  const float crest = crestY(window_width, window_height, wave_heights);
  const float target_y = crest - config_.radius;

  // Soft-follow toward surface (spec baseline), dt-aware.
  const float blend =
      1.0f - std::exp(-config_.follow_rate * config_.follow_blend * dt * 60.0f);
  const float follow_y = y_ + (target_y - y_) * std::clamp(blend, 0.0f, 1.0f);

  // Gravity always; buoyancy when submerged below the target rest pose.
  float accel = config_.gravity;
  const float submersion = y_ - target_y;  // >0 means below rest (deeper in water)
  if (submersion > 0.0f) {
    accel -= config_.buoyancy * std::min(submersion / config_.radius, 2.0f);
  } else {
    // Slight snap assist when above the surface so it settles onto the wave.
    accel += (target_y - y_) * 8.0f;
  }

  vy_ += accel * dt;
  vy_ *= std::pow(config_.damping, dt * 60.0f);

  // Blend soft-follow into velocity integration so spikes don't yank Y hard.
  float new_y = follow_y * 0.55f + (y_ + vy_ * dt) * 0.45f;

  // Hard ceiling: never sink more than a little past the rest pose without
  // correcting next frames; clamp speed.
  const float max_v = config_.max_vertical_speed;
  const float dy = (new_y - y_) / dt;
  if (dy > max_v) {
    new_y = y_ + max_v * dt;
    vy_ = max_v;
  } else if (dy < -max_v) {
    new_y = y_ - max_v * dt;
    vy_ = -max_v;
  }

  const float prev_err = y_ - target_y;
  y_ = new_y;
  const float err = y_ - target_y;

  in_contact_ = std::fabs(err) <= config_.contact_epsilon;
  contact_impulse_ = 0.0f;
  if (prev_err > config_.contact_epsilon && err <= config_.contact_epsilon &&
      vy_ > 0.0f) {
    contact_impulse_ = vy_;
    in_contact_ = true;
  }

  // Keep on-screen vertically.
  const float min_y = config_.radius;
  const float max_y = window_height - config_.radius;
  y_ = std::clamp(y_, min_y, max_y);
}
