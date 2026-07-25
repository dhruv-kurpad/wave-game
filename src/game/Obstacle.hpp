#pragma once

#include "game/Collision.hpp"

#include <SDL.h>

#include <vector>

// Flappy-style pipe pair: solid top + bottom with a vertical gap.
struct Pipe {
  float x = 0.0f;
  float gap_center_y = 360.0f;
  float gap_size = 180.0f;
  float width = 70.0f;
  bool scored = false;

  float gapTop() const { return gap_center_y - gap_size * 0.5f; }
  float gapBottom() const { return gap_center_y + gap_size * 0.5f; }

  bool hitsBall(float ball_x, float ball_y, float ball_radius,
                float window_height) const {
    const float top_h = std::max(0.0f, gapTop());
    if (top_h > 0.0f &&
        circleIntersectsRect(ball_x, ball_y, ball_radius, x, 0.0f, width,
                             top_h)) {
      return true;
    }
    const float bottom_y = gapBottom();
    const float bottom_h = std::max(0.0f, window_height - bottom_y);
    if (bottom_h > 0.0f &&
        circleIntersectsRect(ball_x, ball_y, ball_radius, x, bottom_y, width,
                             bottom_h)) {
      return true;
    }
    return false;
  }
};

class PipeField {
 public:
  struct Config {
    float scroll_speed = 180.0f;
    float pipe_width = 70.0f;
    float pipe_gap = 180.0f;
    float spawn_spacing = 320.0f;
    float min_gap_center = 200.0f;
    float max_gap_center = 520.0f;
  };

  PipeField();
  explicit PipeField(Config config);

  void reset(float window_width, float window_height);
  void update(float dt, float window_width, float window_height);
  void freeze() { frozen_ = true; }
  void unfreeze() { frozen_ = false; }
  bool frozen() const { return frozen_; }

  // Returns true if any pipe hits the ball.
  bool collides(float ball_x, float ball_y, float ball_radius,
                float window_height) const;

  // Awards +1 for each newly passed pipe; returns points gained this call.
  int collectScore(float ball_x);

  void draw(SDL_Renderer* renderer, float window_height) const;

  const std::vector<Pipe>& pipes() const { return pipes_; }
  const Config& config() const { return config_; }
  void setScrollSpeed(float speed) { config_.scroll_speed = speed; }
  void setPipeGap(float gap) { config_.pipe_gap = gap; }

 private:
  void spawnPipe(float window_width, float window_height);

  Config config_;
  std::vector<Pipe> pipes_;
  bool frozen_ = false;
  float spawn_x_ = 0.0f;
};
