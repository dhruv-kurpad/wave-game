#include "audio/AudioInput.hpp"
#include "dsp/DSPProcessor.hpp"
#include "game/DrawCircle.hpp"
#include "game/Game.hpp"
#include "render/WaveRenderer.hpp"

#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr const char* kWindowTitle = "Wave Game — Phase 6 Gameplay";

const char* kFontCandidates[] = {
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
    "/Library/Fonts/Arial.ttf",
    nullptr,
};

TTF_Font* loadHudFont(int point_size) {
  for (int i = 0; kFontCandidates[i] != nullptr; ++i) {
    TTF_Font* font = TTF_OpenFont(kFontCandidates[i], point_size);
    if (font != nullptr) {
      return font;
    }
  }
  return nullptr;
}
}  // namespace

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
    return EXIT_FAILURE;
  }
  if (TTF_Init() != 0) {
    std::cerr << "TTF_Init failed: " << TTF_GetError() << '\n';
    SDL_Quit();
    return EXIT_FAILURE;
  }

  SDL_Window* window = SDL_CreateWindow(
      kWindowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      kWindowWidth, kWindowHeight, SDL_WINDOW_SHOWN);
  if (!window) {
    std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
    TTF_Quit();
    SDL_Quit();
    return EXIT_FAILURE;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return EXIT_FAILURE;
  }

  TTF_Font* font = loadHudFont(18);
  TTF_Font* font_large = loadHudFont(36);
  if (font == nullptr) {
    std::cerr << "Warning: HUD font missing\n";
  }

  AudioInput::Config audio_config;
  audio_config.sample_rate = 44100.0;
  audio_config.frames_per_buffer = 1024;
  audio_config.ring_capacity = 16384;

  AudioInput audio(audio_config);
  const bool audio_ok = audio.start();
  if (!audio_ok) {
    std::cerr << "AudioInput failed: " << audio.lastError() << '\n';
  }

  DSPProcessor::Config dsp_config;
  dsp_config.volume_gate = 0.01f;
  dsp_config.volume_sensitivity = 1.0f;
  dsp_config.smoothing_alpha = 0.25f;
  dsp_config.smoothing_release_alpha = 0.05f;
  dsp_config.control_attack = 0.45f;
  dsp_config.control_release = 0.04f;
  dsp_config.spatial_smooth = 0.35f;
  dsp_config.wave_point_count = 256;
  dsp_config.analysis_frames = 1024;
  dsp_config.fft_size = 1024;
  dsp_config.sample_rate = 44100.0f;

  DSPProcessor dsp(dsp_config);
  dsp.start(audio.buffer());

  WaveRenderer::Config wave_cfg;
  wave_cfg.oscillation_amplitude = 0.012f;
  wave_cfg.glow_layers = 4;
  wave_cfg.gradient_steps = 6;
  WaveRenderer wave_renderer(wave_cfg);

  Ball::Config ball_cfg;
  ball_cfg.radius = 18.0f;
  ball_cfg.y_top = wave_cfg.y_top;
  ball_cfg.y_bottom_margin = wave_cfg.y_bottom_margin;

  PipeField::Config pipe_cfg;
  pipe_cfg.scroll_speed = 180.0f;
  pipe_cfg.pipe_gap = 180.0f;
  pipe_cfg.spawn_spacing = 340.0f;

  Game::Config game_cfg;
  game_cfg.window_width = static_cast<float>(kWindowWidth);
  game_cfg.window_height = static_cast<float>(kWindowHeight);
  Game game(game_cfg, ball_cfg, pipe_cfg);

  std::vector<float> wave(256, 0.22f);
  auto prev = std::chrono::steady_clock::now();
  ControlMode last_mode = game.selectedMode();
  dsp.setMode(last_mode);

  std::cout << "Mode select: 1/2 choose mode, Space to play. Esc pauses in-game.\n";

  bool running = true;
  while (running) {
    const auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - prev).count();
    prev = now;
    if (dt <= 0.0f) {
      dt = 1.0f / 60.0f;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (game.handleEvent(event, wave)) {
        running = false;
      }
    }

    if (game.selectedMode() != last_mode) {
      last_mode = game.selectedMode();
      dsp.setMode(last_mode);
    }

    dsp.copyWaveHeights(wave);
    if (wave.empty()) {
      wave.assign(256, 0.22f);
    }

    // Keep DSP mode synced while playing (1/2 still work).
    dsp.setMode(game.selectedMode());

    game.update(dt, wave);

    if (!audio_ok) {
      SDL_SetRenderDrawColor(renderer, 64, 20, 20, 255);
      SDL_RenderClear(renderer);
    } else {
      wave_renderer.update(dt);
      wave_renderer.draw(renderer, wave, dsp.controlValue(), dsp.mode(),
                         kWindowWidth, kWindowHeight);
    }

    // Pipes (only meaningful during play / pause / game over)
    if (game.state() != GameState::ModeSelect) {
      game.pipes().draw(renderer, static_cast<float>(kWindowHeight));
    }

    // Ball during play-related states
    if (game.state() == GameState::Playing || game.state() == GameState::Paused ||
        game.state() == GameState::GameOver) {
      const Ball& ball = game.ball();
      const int cx = static_cast<int>(ball.x());
      const int cy = static_cast<int>(ball.y());
      const int r = static_cast<int>(ball.radius());
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 60);
      drawFilledCircle(renderer, cx + 2, cy + 3, r);
      if (dsp.mode() == ControlMode::Frequency) {
        SDL_SetRenderDrawColor(renderer, 255, 210, 90, 255);
      } else {
        SDL_SetRenderDrawColor(renderer, 255, 140, 70, 255);
      }
      drawFilledCircle(renderer, cx, cy, r);
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 160);
      drawFilledCircle(renderer, cx - r / 3, cy - r / 3, std::max(2, r / 4));
    }

    // Compact meters while playing
    if (game.state() == GameState::Playing) {
      const float control = dsp.controlValue();
      const int bar_margin = 40;
      const int bar_h = 16;
      const int bar_w = 280;
      SDL_Rect bg{bar_margin, 40, bar_w, bar_h};
      SDL_SetRenderDrawColor(renderer, 20, 20, 28, 180);
      SDL_RenderFillRect(renderer, &bg);
      SDL_Rect fg{bar_margin, 40,
                  static_cast<int>(bar_w * std::min(1.0f, control)), bar_h};
      if (dsp.mode() == ControlMode::Frequency) {
        SDL_SetRenderDrawColor(renderer, 100, 210, 255, 220);
      } else {
        SDL_SetRenderDrawColor(renderer, 220, 160, 60, 220);
      }
      SDL_RenderFillRect(renderer, &fg);
    }

    game.drawOverlay(renderer, font, font_large);
    SDL_RenderPresent(renderer);
  }

  dsp.stop();
  audio.stop();
  if (font) {
    TTF_CloseFont(font);
  }
  if (font_large) {
    TTF_CloseFont(font_large);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  TTF_Quit();
  SDL_Quit();
  return EXIT_SUCCESS;
}
