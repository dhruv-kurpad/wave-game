#pragma once

#include "dsp/DSPProcessor.hpp"
#include "game/Ball.hpp"
#include "game/Obstacle.hpp"

#include <SDL.h>
#include <SDL_ttf.h>

#include <string>
#include <vector>

enum class GameState {
  ModeSelect,
  Playing,
  Paused,
  GameOver,
};

class Game {
 public:
  struct Config {
    float window_width = 1280.0f;
    float window_height = 720.0f;
    float scroll_speed = 180.0f;
    float pipe_gap = 180.0f;
  };

  Game(Config config, Ball::Config ball_config, PipeField::Config pipe_config);

  GameState state() const { return state_; }
  ControlMode selectedMode() const { return selected_mode_; }
  int score() const { return score_; }
  const Ball& ball() const { return ball_; }
  Ball& ball() { return ball_; }
  const PipeField& pipes() const { return pipes_; }
  PipeField& pipes() { return pipes_; }

  void setSelectedMode(ControlMode mode) { selected_mode_ = mode; }

  // Start a run with the currently selected mode.
  void startPlaying(const std::vector<float>& wave_heights);
  void returnToMenu();
  void pause();
  void resume();
  void retry(const std::vector<float>& wave_heights);

  // Returns true if the app should quit.
  bool handleEvent(const SDL_Event& event, const std::vector<float>& wave);

  void update(float dt, const std::vector<float>& wave_heights);

  void drawOverlay(SDL_Renderer* renderer,
                   TTF_Font* font,
                   TTF_Font* font_large) const;

  // Hit-test helpers for mode select buttons (filled by drawOverlay layout).
  SDL_Rect volumeButton() const { return volume_btn_; }
  SDL_Rect frequencyButton() const { return frequency_btn_; }
  SDL_Rect playButton() const { return play_btn_; }

 private:
  void layoutButtons();
  bool pointInRect(int x, int y, const SDL_Rect& r) const;

  Config config_;
  GameState state_ = GameState::ModeSelect;
  ControlMode selected_mode_ = ControlMode::Volume;
  int score_ = 0;

  Ball ball_;
  PipeField pipes_;

  SDL_Rect volume_btn_{};
  SDL_Rect frequency_btn_{};
  SDL_Rect play_btn_{};
  SDL_Rect resume_btn_{};
  SDL_Rect menu_btn_{};
  SDL_Rect retry_btn_{};
};
