#pragma once

#include <vector>

// Ball that rests on a normalized water surface (screen-space physics).
class Ball {
 public:
  struct Config {
    float radius = 18.0f;
    float follow_blend = 0.2f;       // soft-follow toward target (per ~16ms feel)
    float follow_rate = 12.0f;       // dt-aware alternative strength
    float damping = 0.88f;           // velocity damping each step
    float max_vertical_speed = 800.0f;
    float gravity = 1400.0f;         // px/s^2 downward (SDL +Y)
    float buoyancy = 2200.0f;        // push up when below target surface
    float contact_epsilon = 3.0f;    // px — “touching” the crest
    float y_top = 140.0f;
    float y_bottom_margin = 40.0f;
  };

  Ball();
  explicit Ball(Config config);

  void setConfig(const Config& config) { config_ = config; }
  const Config& config() const { return config_; }

  // Place at horizontal center; snap onto the current wave.
  void reset(float window_width,
             float window_height,
             const std::vector<float>& wave_heights);

  // Fixed X at screen center. Updates Y from wave sample + forces.
  void update(float dt,
              float window_width,
              float window_height,
              const std::vector<float>& wave_heights);

  float x() const { return x_; }
  float y() const { return y_; }
  float radius() const { return config_.radius; }
  float vy() const { return vy_; }
  bool inContact() const { return in_contact_; }
  float contactImpulse() const { return contact_impulse_; }

  // Screen-space crest Y at the ball's X (no render oscillation).
  float crestY(float window_width,
               float window_height,
               const std::vector<float>& wave_heights) const;

 private:
  float mapCrestY(float normalized_height, float window_height) const;

  Config config_;
  float x_ = 0.0f;
  float y_ = 0.0f;
  float vy_ = 0.0f;
  bool in_contact_ = false;
  float contact_impulse_ = 0.0f;
};
