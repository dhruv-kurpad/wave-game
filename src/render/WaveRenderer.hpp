#pragma once

#include "dsp/DSPProcessor.hpp"

#include <SDL.h>

#include <vector>

// Draws a water-like surface from normalized wave heights [0, 1].
class WaveRenderer {
 public:
  struct Config {
    float y_top = 140.0f;
    float y_bottom_margin = 40.0f;
    float oscillation_amplitude = 0.012f;  // sample-space wobble
    float oscillation_speed = 1.6f;
    int glow_layers = 4;
    int gradient_steps = 6;
    // Draw every Nth column (1 = full width). 2 is a cheap 60 FPS safety.
    int column_step = 1;
  };

  WaveRenderer();
  explicit WaveRenderer(Config config);

  void setConfig(const Config& config) { config_ = config; }
  const Config& config() const { return config_; }

  // Advance the horizontal oscillation clock (seconds).
  void update(float dt_seconds);

  // heights: normalized [0,1]. control: volume or pitch in [0,1].
  void draw(SDL_Renderer* renderer,
            const std::vector<float>& heights,
            float control,
            ControlMode mode,
            int window_width,
            int window_height);

  // Screen-space crest Y at pixel x (for Phase 5 ball). Uses last drawn sample.
  float crestYAtPixelX(float x, int window_width, int window_height) const;

 private:
  struct Tint {
    Uint8 fill_top_r, fill_top_g, fill_top_b;
    Uint8 fill_bot_r, fill_bot_g, fill_bot_b;
    Uint8 crest_r, crest_g, crest_b;
    Uint8 glow_r, glow_g, glow_b;
    Uint8 bg_r, bg_g, bg_b;
  };

  Tint makeTint(float control, ControlMode mode) const;
  void rebuildScreenYs(const std::vector<float>& heights,
                       int window_width,
                       int window_height);

  Config config_;
  float osc_phase_ = 0.0f;
  std::vector<float> dense_heights_;
  std::vector<int> screen_y_;  // crest Y per dense sample (column)
};
