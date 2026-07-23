#include "audio/AudioInput.hpp"
#include "dsp/DSPProcessor.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr const char* kWindowTitle = "Wave Game — Phase 2 Volume DSP";
}  // namespace

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
    return EXIT_FAILURE;
  }

  SDL_Window* window = SDL_CreateWindow(
      kWindowTitle,
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      kWindowWidth,
      kWindowHeight,
      SDL_WINDOW_SHOWN);

  if (!window) {
    std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
    SDL_Quit();
    return EXIT_FAILURE;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  if (!renderer) {
    std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_FAILURE;
  }

  AudioInput::Config audio_config;
  audio_config.sample_rate = 44100.0;
  audio_config.frames_per_buffer = 1024;
  audio_config.device_index = -1;
  audio_config.ring_capacity = 16384;

  AudioInput audio(audio_config);
  const bool audio_ok = audio.start();
  if (!audio_ok) {
    std::cerr << "AudioInput failed: " << audio.lastError() << '\n';
  } else {
    std::cout << "Audio + DSP started. Speak to raise the wave. Esc quits.\n";
  }

  DSPProcessor::Config dsp_config;
  dsp_config.volume_gate = 0.01f;
  dsp_config.volume_sensitivity = 1.0f;
  dsp_config.smoothing_alpha = 0.2f;
  dsp_config.spatial_smooth = 0.35f;
  dsp_config.wave_point_count = 256;
  dsp_config.analysis_frames = 1024;

  DSPProcessor dsp(dsp_config);
  dsp.start(audio.buffer());

  std::vector<float> wave;
  std::vector<SDL_Point> points;
  int frame_counter = 0;

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_KEYDOWN &&
                 event.key.keysym.sym == SDLK_ESCAPE) {
        running = false;
      }
    }

    dsp.copyWaveHeights(wave);
    const float volume = dsp.volume();
    const float rms = dsp.rawRms();
    const float fill =
        static_cast<float>(audio.buffer().available()) /
        static_cast<float>(std::max<std::size_t>(1, audio.buffer().capacity()));

    ++frame_counter;
    if (frame_counter % 60 == 0) {
      std::cout << "vol=" << volume << " rms=" << rms << " fill=" << fill
                << " overflow=" << audio.buffer().overflowCount()
                << " points=" << wave.size() << '\n';
    }

    if (audio_ok) {
      SDL_SetRenderDrawColor(renderer, 12, 48, 64, 255);
    } else {
      SDL_SetRenderDrawColor(renderer, 64, 20, 20, 255);
    }
    SDL_RenderClear(renderer);

    // Volume meter (top)
    const int bar_margin = 40;
    const int bar_height = 28;
    const int bar_max_w = kWindowWidth - bar_margin * 2;
    SDL_Rect vol_bg{bar_margin, bar_margin, bar_max_w, bar_height};
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
    SDL_RenderFillRect(renderer, &vol_bg);
    SDL_Rect vol_fg{bar_margin, bar_margin,
                    static_cast<int>(bar_max_w * std::min(1.0f, volume)),
                    bar_height};
    SDL_SetRenderDrawColor(renderer, 220, 160, 60, 255);
    SDL_RenderFillRect(renderer, &vol_fg);

    // Wave polyline — heights in [0,1] map to screen Y (1 = higher on screen)
    if (!wave.empty()) {
      points.resize(wave.size());
      const float y_top = 140.0f;
      const float y_bottom = static_cast<float>(kWindowHeight) - 40.0f;
      for (std::size_t i = 0; i < wave.size(); ++i) {
        const float t = (wave.size() == 1)
                            ? 0.0f
                            : static_cast<float>(i) /
                                  static_cast<float>(wave.size() - 1);
        const float x = t * static_cast<float>(kWindowWidth - 1);
        const float y = y_bottom - wave[i] * (y_bottom - y_top);
        points[i].x = static_cast<int>(x);
        points[i].y = static_cast<int>(y);
      }

      // Simple fill under the wave (vertical strips).
      SDL_SetRenderDrawColor(renderer, 40, 120, 150, 255);
      for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        const int x0 = points[i].x;
        const int x1 = points[i + 1].x;
        const int y0 = points[i].y;
        const int y1 = points[i + 1].y;
        for (int x = x0; x <= x1; ++x) {
          const float u =
              (x1 == x0) ? 0.0f
                         : static_cast<float>(x - x0) / static_cast<float>(x1 - x0);
          const int y = static_cast<int>(y0 + u * (y1 - y0));
          SDL_RenderDrawLine(renderer, x, y, x, kWindowHeight);
        }
      }

      SDL_SetRenderDrawColor(renderer, 180, 230, 255, 255);
      SDL_RenderDrawLines(renderer, points.data(),
                          static_cast<int>(points.size()));
    }

    SDL_RenderPresent(renderer);
  }

  dsp.stop();
  audio.stop();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return EXIT_SUCCESS;
}
