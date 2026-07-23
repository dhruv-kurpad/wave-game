#include "audio/AudioInput.hpp"
#include "dsp/DSPProcessor.hpp"
#include "render/WaveRenderer.hpp"

#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr const char* kWindowTitle = "Wave Game — Phase 4 Water Visualizer";

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

void drawHelpBox(SDL_Renderer* renderer,
                 TTF_Font* font,
                 ControlMode mode,
                 int bar_margin,
                 int box_top) {
  if (renderer == nullptr || font == nullptr) {
    return;
  }

  const std::string line1 = "Press 1 = Volume mode   |   Press 2 = Frequency mode";
  const std::string line2 =
      (mode == ControlMode::Frequency)
          ? "Current: Frequency  —  hum / whistle low to high"
          : "Current: Volume  —  speak louder to raise the wave";

  SDL_Color text_color{230, 235, 240, 255};
  SDL_Surface* s1 = TTF_RenderUTF8_Blended(font, line1.c_str(), text_color);
  SDL_Surface* s2 = TTF_RenderUTF8_Blended(font, line2.c_str(), text_color);
  if (s1 == nullptr || s2 == nullptr) {
    if (s1) {
      SDL_FreeSurface(s1);
    }
    if (s2) {
      SDL_FreeSurface(s2);
    }
    return;
  }

  const int pad_x = 14;
  const int pad_y = 10;
  const int gap = 4;
  const int box_w = std::max(s1->w, s2->w) + pad_x * 2;
  const int box_h = s1->h + s2->h + gap + pad_y * 2;

  SDL_Rect box{bar_margin, box_top, box_w, box_h};
  SDL_SetRenderDrawColor(renderer, 16, 18, 24, 200);
  SDL_RenderFillRect(renderer, &box);
  SDL_SetRenderDrawColor(renderer, 90, 120, 140, 220);
  SDL_RenderDrawRect(renderer, &box);

  SDL_Texture* t1 = SDL_CreateTextureFromSurface(renderer, s1);
  SDL_Texture* t2 = SDL_CreateTextureFromSurface(renderer, s2);
  SDL_FreeSurface(s1);
  SDL_FreeSurface(s2);
  if (t1 == nullptr || t2 == nullptr) {
    if (t1) {
      SDL_DestroyTexture(t1);
    }
    if (t2) {
      SDL_DestroyTexture(t2);
    }
    return;
  }

  int w1 = 0;
  int h1 = 0;
  int w2 = 0;
  int h2 = 0;
  SDL_QueryTexture(t1, nullptr, nullptr, &w1, &h1);
  SDL_QueryTexture(t2, nullptr, nullptr, &w2, &h2);
  SDL_Rect r1{bar_margin + pad_x, box_top + pad_y, w1, h1};
  SDL_Rect r2{bar_margin + pad_x, box_top + pad_y + h1 + gap, w2, h2};
  SDL_RenderCopy(renderer, t1, nullptr, &r1);
  SDL_RenderCopy(renderer, t2, nullptr, &r2);
  SDL_DestroyTexture(t1);
  SDL_DestroyTexture(t2);
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
      kWindowTitle,
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      kWindowWidth,
      kWindowHeight,
      SDL_WINDOW_SHOWN);

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

  TTF_Font* hud_font = loadHudFont(16);
  if (hud_font == nullptr) {
    std::cerr << "Warning: could not load HUD font — help box disabled ("
              << TTF_GetError() << ")\n";
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
    std::cout << "Keys: 1 = Volume, 2 = Frequency. Esc quits.\n";
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
  dsp.setMode(ControlMode::Volume);
  dsp.start(audio.buffer());

  WaveRenderer::Config wave_cfg;
  wave_cfg.oscillation_amplitude = 0.012f;
  wave_cfg.oscillation_speed = 1.6f;
  wave_cfg.glow_layers = 4;
  wave_cfg.gradient_steps = 6;
  wave_cfg.column_step = 1;
  WaveRenderer wave_renderer(wave_cfg);

  std::vector<float> wave;
  int frame_counter = 0;
  auto prev = std::chrono::steady_clock::now();
  double fps_accum = 0.0;
  int fps_frames = 0;
  double fps_display = 0.0;

  bool running = true;
  while (running) {
    const auto now = std::chrono::steady_clock::now();
    const float dt =
        std::chrono::duration<float>(now - prev).count();
    prev = now;
    fps_accum += dt;
    ++fps_frames;
    if (fps_accum >= 1.0) {
      fps_display = static_cast<double>(fps_frames) / fps_accum;
      fps_accum = 0.0;
      fps_frames = 0;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_ESCAPE) {
          running = false;
        } else if (event.key.keysym.sym == SDLK_1) {
          dsp.setMode(ControlMode::Volume);
          std::cout << "Mode: Volume\n";
        } else if (event.key.keysym.sym == SDLK_2) {
          dsp.setMode(ControlMode::Frequency);
          std::cout << "Mode: Frequency\n";
        }
      }
    }

    dsp.copyWaveHeights(wave);
    const float control = dsp.controlValue();
    const float volume = dsp.volume();
    const float pitch = dsp.pitch();
    const float hz = dsp.pitchHz();

    ++frame_counter;
    if (frame_counter % 60 == 0) {
      const char* mode_name =
          (dsp.mode() == ControlMode::Volume) ? "volume" : "frequency";
      std::cout << "mode=" << mode_name << " control=" << control
                << " vol=" << volume << " pitch=" << pitch << " hz=" << hz
                << " fps=" << fps_display
                << " overflow=" << audio.buffer().overflowCount() << '\n';
    }

    if (!audio_ok) {
      SDL_SetRenderDrawColor(renderer, 64, 20, 20, 255);
      SDL_RenderClear(renderer);
    } else {
      wave_renderer.update(dt > 0.0f ? dt : (1.0f / 60.0f));
      wave_renderer.draw(renderer, wave, control, dsp.mode(), kWindowWidth,
                         kWindowHeight);
    }

    // HUD meters on top of water
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    const int bar_margin = 40;
    const int bar_height = 24;
    const int bar_max_w = kWindowWidth - bar_margin * 2;

    SDL_Rect ctrl_bg{bar_margin, bar_margin, bar_max_w, bar_height};
    SDL_SetRenderDrawColor(renderer, 20, 20, 28, 180);
    SDL_RenderFillRect(renderer, &ctrl_bg);
    SDL_Rect ctrl_fg{bar_margin, bar_margin,
                     static_cast<int>(bar_max_w * std::min(1.0f, control)),
                     bar_height};
    if (dsp.mode() == ControlMode::Frequency) {
      SDL_SetRenderDrawColor(renderer, 100, 210, 255, 220);
    } else {
      SDL_SetRenderDrawColor(renderer, 220, 160, 60, 220);
    }
    SDL_RenderFillRect(renderer, &ctrl_fg);

    SDL_Rect vol_bg{bar_margin, bar_margin + bar_height + 8, bar_max_w / 2 - 4,
                    12};
    SDL_Rect pitch_bg{bar_margin + bar_max_w / 2 + 4,
                      bar_margin + bar_height + 8, bar_max_w / 2 - 4, 12};
    SDL_SetRenderDrawColor(renderer, 20, 20, 28, 180);
    SDL_RenderFillRect(renderer, &vol_bg);
    SDL_RenderFillRect(renderer, &pitch_bg);
    SDL_Rect vol_fg{vol_bg.x, vol_bg.y,
                    static_cast<int>(vol_bg.w * std::min(1.0f, volume)),
                    vol_bg.h};
    SDL_Rect pitch_fg{pitch_bg.x, pitch_bg.y,
                      static_cast<int>(pitch_bg.w * std::min(1.0f, pitch)),
                      pitch_bg.h};
    SDL_SetRenderDrawColor(renderer, 220, 160, 60, 220);
    SDL_RenderFillRect(renderer, &vol_fg);
    SDL_SetRenderDrawColor(renderer, 100, 210, 255, 220);
    SDL_RenderFillRect(renderer, &pitch_fg);

    const int help_top = pitch_bg.y + pitch_bg.h + 12;
    drawHelpBox(renderer, hud_font, dsp.mode(), bar_margin, help_top);

    SDL_RenderPresent(renderer);
  }

  if (hud_font != nullptr) {
    TTF_CloseFont(hud_font);
  }
  dsp.stop();
  audio.stop();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  TTF_Quit();
  SDL_Quit();
  return EXIT_SUCCESS;
}
