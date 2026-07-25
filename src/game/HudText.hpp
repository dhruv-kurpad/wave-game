#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <string>

inline void drawText(SDL_Renderer* renderer,
                     TTF_Font* font,
                     const std::string& text,
                     int x,
                     int y,
                     SDL_Color color) {
  if (renderer == nullptr || font == nullptr || text.empty()) {
    return;
  }
  SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
  if (surface == nullptr) {
    return;
  }
  SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
  const int w = surface->w;
  const int h = surface->h;
  SDL_FreeSurface(surface);
  if (texture == nullptr) {
    return;
  }
  SDL_Rect dst{x, y, w, h};
  SDL_RenderCopy(renderer, texture, nullptr, &dst);
  SDL_DestroyTexture(texture);
}

inline void drawTextCentered(SDL_Renderer* renderer,
                             TTF_Font* font,
                             const std::string& text,
                             int cx,
                             int y,
                             SDL_Color color) {
  if (renderer == nullptr || font == nullptr || text.empty()) {
    return;
  }
  int w = 0;
  int h = 0;
  if (TTF_SizeUTF8(font, text.c_str(), &w, &h) != 0) {
    return;
  }
  drawText(renderer, font, text, cx - w / 2, y, color);
}

inline void fillButton(SDL_Renderer* renderer,
                       const SDL_Rect& rect,
                       bool selected,
                       bool accent_cyan) {
  if (selected) {
    if (accent_cyan) {
      SDL_SetRenderDrawColor(renderer, 40, 110, 140, 230);
    } else {
      SDL_SetRenderDrawColor(renderer, 140, 100, 40, 230);
    }
  } else {
    SDL_SetRenderDrawColor(renderer, 24, 28, 36, 200);
  }
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, 180, 200, 210, 255);
  SDL_RenderDrawRect(renderer, &rect);
}
