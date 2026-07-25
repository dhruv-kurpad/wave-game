#pragma once

#include <SDL.h>

#include <cmath>

// Filled circle helper for the ball (no texture needed).
inline void drawFilledCircle(SDL_Renderer* renderer,
                             int cx,
                             int cy,
                             int radius) {
  if (renderer == nullptr || radius <= 0) {
    return;
  }
  for (int dy = -radius; dy <= radius; ++dy) {
    const int dx = static_cast<int>(
        std::sqrt(static_cast<float>(radius * radius - dy * dy)));
    SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
  }
}
