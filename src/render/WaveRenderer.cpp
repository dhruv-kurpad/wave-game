#include "render/WaveRenderer.hpp"
#include "render/Spline.hpp"

#include <algorithm>
#include <cmath>

WaveRenderer::WaveRenderer() : WaveRenderer(Config{}) {}

WaveRenderer::WaveRenderer(Config config) : config_(config) {}

void WaveRenderer::update(float dt_seconds) {
  osc_phase_ += dt_seconds * config_.oscillation_speed;
  if (osc_phase_ > 6.2831853f * 100.0f) {
    osc_phase_ = std::fmod(osc_phase_, 6.2831853f);
  }
}

WaveRenderer::Tint WaveRenderer::makeTint(float control, ControlMode mode) const {
  control = std::clamp(control, 0.0f, 1.0f);
  Tint t{};

  if (mode == ControlMode::Frequency) {
    // Cool cyan/teal — brightens with pitch.
    t.bg_r = 8;
    t.bg_g = static_cast<Uint8>(28 + control * 36);
    t.bg_b = static_cast<Uint8>(48 + control * 50);
    t.fill_top_r = static_cast<Uint8>(40 + control * 30);
    t.fill_top_g = static_cast<Uint8>(140 + control * 60);
    t.fill_top_b = static_cast<Uint8>(180 + control * 50);
    t.fill_bot_r = 10;
    t.fill_bot_g = static_cast<Uint8>(40 + control * 20);
    t.fill_bot_b = static_cast<Uint8>(60 + control * 30);
    t.crest_r = 200;
    t.crest_g = 240;
    t.crest_b = 255;
    t.glow_r = 80;
    t.glow_g = 200;
    t.glow_b = 255;
  } else {
    // Deeper teal with warm lift from volume.
    t.bg_r = static_cast<Uint8>(10 + control * 18);
    t.bg_g = static_cast<Uint8>(36 + control * 20);
    t.bg_b = static_cast<Uint8>(52 + control * 10);
    t.fill_top_r = static_cast<Uint8>(50 + control * 40);
    t.fill_top_g = static_cast<Uint8>(130 + control * 40);
    t.fill_top_b = static_cast<Uint8>(150 + control * 20);
    t.fill_bot_r = static_cast<Uint8>(8 + control * 12);
    t.fill_bot_g = static_cast<Uint8>(36 + control * 16);
    t.fill_bot_b = static_cast<Uint8>(48 + control * 12);
    t.crest_r = 220;
    t.crest_g = 245;
    t.crest_b = 255;
    t.glow_r = 255;
    t.glow_g = 200;
    t.glow_b = 120;
  }
  return t;
}

void WaveRenderer::rebuildScreenYs(const std::vector<float>& heights,
                                   int window_width,
                                   int window_height) {
  const int step = std::max(1, config_.column_step);
  const int columns = std::max(2, (window_width + step - 1) / step);

  resampleWaveDense(heights, dense_heights_, static_cast<std::size_t>(columns),
                    osc_phase_, config_.oscillation_amplitude);

  const float y_top = config_.y_top;
  const float y_bottom =
      static_cast<float>(window_height) - config_.y_bottom_margin;
  const float span = y_bottom - y_top;

  screen_y_.resize(static_cast<std::size_t>(columns));
  for (int i = 0; i < columns; ++i) {
    const float h = dense_heights_[static_cast<std::size_t>(i)];
    screen_y_[static_cast<std::size_t>(i)] =
        static_cast<int>(y_bottom - std::clamp(h, 0.0f, 1.0f) * span);
  }
}

void WaveRenderer::draw(SDL_Renderer* renderer,
                        const std::vector<float>& heights,
                        float control,
                        ControlMode mode,
                        int window_width,
                        int window_height) {
  if (renderer == nullptr || heights.empty() || window_width <= 0 ||
      window_height <= 0) {
    return;
  }

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  const Tint tint = makeTint(control, mode);
  SDL_SetRenderDrawColor(renderer, tint.bg_r, tint.bg_g, tint.bg_b, 255);
  SDL_RenderClear(renderer);

  rebuildScreenYs(heights, window_width, window_height);

  const int step = std::max(1, config_.column_step);
  const int columns = static_cast<int>(screen_y_.size());
  const int bottom = window_height;
  const int grad_steps = std::max(2, config_.gradient_steps);

  // Gradient fill under the crest (vertical bands per column).
  for (int i = 0; i < columns; ++i) {
    const int x = std::min(i * step, window_width - 1);
    const int crest = screen_y_[static_cast<std::size_t>(i)];
    if (crest >= bottom) {
      continue;
    }
    const int span = bottom - crest;
    for (int s = 0; s < grad_steps; ++s) {
      const float t0 = static_cast<float>(s) / static_cast<float>(grad_steps);
      const float t1 =
          static_cast<float>(s + 1) / static_cast<float>(grad_steps);
      const int y0 = crest + static_cast<int>(span * t0);
      const int y1 = crest + static_cast<int>(span * t1);
      const float t = 0.5f * (t0 + t1);
      const Uint8 r = static_cast<Uint8>(
          tint.fill_top_r + (tint.fill_bot_r - tint.fill_top_r) * t);
      const Uint8 g = static_cast<Uint8>(
          tint.fill_top_g + (tint.fill_bot_g - tint.fill_top_g) * t);
      const Uint8 b = static_cast<Uint8>(
          tint.fill_top_b + (tint.fill_bot_b - tint.fill_top_b) * t);
      SDL_SetRenderDrawColor(renderer, r, g, b, 255);
      if (step == 1) {
        SDL_RenderDrawLine(renderer, x, y0, x, y1);
      } else {
        SDL_Rect bar{x, y0, step, std::max(1, y1 - y0)};
        SDL_RenderFillRect(renderer, &bar);
      }
    }
  }

  // Soft glow under/above the crest (alpha layers).
  const int glow_layers = std::max(0, config_.glow_layers);
  for (int layer = glow_layers; layer >= 1; --layer) {
    const Uint8 alpha = static_cast<Uint8>(28 + (glow_layers - layer) * 18);
    SDL_SetRenderDrawColor(renderer, tint.glow_r, tint.glow_g, tint.glow_b,
                           alpha);
    for (int i = 0; i + 1 < columns; ++i) {
      const int x0 = std::min(i * step, window_width - 1);
      const int x1 = std::min((i + 1) * step, window_width - 1);
      const int y0 = screen_y_[static_cast<std::size_t>(i)] + layer;
      const int y1 = screen_y_[static_cast<std::size_t>(i + 1)] + layer;
      SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
      SDL_RenderDrawLine(renderer, x0, y0 - layer, x1, y1 - layer);
    }
  }

  // Soft crest: thick polyline (parallel offsets).
  SDL_SetRenderDrawColor(renderer, tint.crest_r, tint.crest_g, tint.crest_b,
                         220);
  for (int offset = -2; offset <= 2; ++offset) {
    for (int i = 0; i + 1 < columns; ++i) {
      const int x0 = std::min(i * step, window_width - 1);
      const int x1 = std::min((i + 1) * step, window_width - 1);
      const int y0 = screen_y_[static_cast<std::size_t>(i)] + offset;
      const int y1 = screen_y_[static_cast<std::size_t>(i + 1)] + offset;
      SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
    }
  }
}

float WaveRenderer::crestYAtPixelX(float x,
                                   int window_width,
                                   int window_height) const {
  if (dense_heights_.empty() || window_width <= 1) {
    return static_cast<float>(window_height) * 0.5f;
  }
  const float u =
      std::clamp(x / static_cast<float>(window_width - 1), 0.0f, 1.0f);
  const float h = sampleWaveHeight(dense_heights_, u);
  const float y_top = config_.y_top;
  const float y_bottom =
      static_cast<float>(window_height) - config_.y_bottom_margin;
  return y_bottom - std::clamp(h, 0.0f, 1.0f) * (y_bottom - y_top);
}
