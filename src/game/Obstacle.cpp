#include "game/Obstacle.hpp"

#include <algorithm>
#include <cstdlib>

PipeField::PipeField() : PipeField(Config{}) {}

PipeField::PipeField(Config config) : config_(config) {}

void PipeField::reset(float window_width, float window_height) {
  pipes_.clear();
  frozen_ = false;
  spawn_x_ = window_width + 80.0f;
  // Prime a couple of pipes ahead.
  spawnPipe(window_width, window_height);
  spawn_x_ += config_.spawn_spacing;
  spawnPipe(window_width, window_height);
}

void PipeField::spawnPipe(float /*window_width*/, float window_height) {
  Pipe p;
  p.x = spawn_x_;
  p.width = config_.pipe_width;
  p.gap_size = config_.pipe_gap;
  const float min_c = config_.min_gap_center;
  const float max_c = std::min(config_.max_gap_center, window_height - 120.0f);
  const float lo = std::min(min_c, max_c);
  const float hi = std::max(min_c, max_c);
  const float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
  p.gap_center_y = lo + t * (hi - lo);
  p.scored = false;
  pipes_.push_back(p);
}

void PipeField::update(float dt, float window_width, float window_height) {
  if (frozen_ || dt <= 0.0f) {
    return;
  }
  dt = std::min(dt, 0.05f);

  for (Pipe& p : pipes_) {
    p.x -= config_.scroll_speed * dt;
  }

  // Despawn off-screen left.
  while (!pipes_.empty() &&
         pipes_.front().x + pipes_.front().width < -40.0f) {
    pipes_.erase(pipes_.begin());
  }

  float rightmost = -1.0e9f;
  for (const Pipe& p : pipes_) {
    rightmost = std::max(rightmost, p.x);
  }
  if (pipes_.empty() || rightmost < window_width - config_.spawn_spacing) {
    spawn_x_ = (pipes_.empty() ? window_width : rightmost) + config_.spawn_spacing;
    spawnPipe(window_width, window_height);
  }
}

bool PipeField::collides(float ball_x, float ball_y, float ball_radius,
                         float window_height) const {
  for (const Pipe& p : pipes_) {
    if (p.hitsBall(ball_x, ball_y, ball_radius, window_height)) {
      return true;
    }
  }
  return false;
}

int PipeField::collectScore(float ball_x) {
  int gained = 0;
  for (Pipe& p : pipes_) {
    if (!p.scored && p.x + p.width < ball_x) {
      p.scored = true;
      ++gained;
    }
  }
  return gained;
}

void PipeField::draw(SDL_Renderer* renderer, float window_height) const {
  if (renderer == nullptr) {
    return;
  }
  for (const Pipe& p : pipes_) {
    const int x = static_cast<int>(p.x);
    const int w = static_cast<int>(p.width);
    const int gap_top = static_cast<int>(p.gapTop());
    const int gap_bot = static_cast<int>(p.gapBottom());

    SDL_SetRenderDrawColor(renderer, 34, 120, 70, 255);
    if (gap_top > 0) {
      SDL_Rect top{x, 0, w, gap_top};
      SDL_RenderFillRect(renderer, &top);
      SDL_SetRenderDrawColor(renderer, 20, 80, 45, 255);
      SDL_RenderDrawRect(renderer, &top);
      // Cap
      SDL_SetRenderDrawColor(renderer, 46, 150, 85, 255);
      SDL_Rect cap{x - 4, gap_top - 18, w + 8, 18};
      SDL_RenderFillRect(renderer, &cap);
    }
    SDL_SetRenderDrawColor(renderer, 34, 120, 70, 255);
    if (gap_bot < static_cast<int>(window_height)) {
      SDL_Rect bot{x, gap_bot, w,
                   static_cast<int>(window_height) - gap_bot};
      SDL_RenderFillRect(renderer, &bot);
      SDL_SetRenderDrawColor(renderer, 20, 80, 45, 255);
      SDL_RenderDrawRect(renderer, &bot);
      SDL_SetRenderDrawColor(renderer, 46, 150, 85, 255);
      SDL_Rect cap{x - 4, gap_bot, w + 8, 18};
      SDL_RenderFillRect(renderer, &cap);
    }
  }
}
