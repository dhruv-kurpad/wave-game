#include "game/Game.hpp"
#include "game/HudText.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <string>

Game::Game(Config config, Ball::Config ball_config, PipeField::Config pipe_config)
    : config_(config),
      ball_(ball_config),
      pipes_(pipe_config) {
  layoutButtons();
  std::srand(static_cast<unsigned>(std::time(nullptr)));
}

void Game::layoutButtons() {
  const int cx = static_cast<int>(config_.window_width / 2.0f);
  const int bw = 220;
  const int bh = 56;
  volume_btn_ = SDL_Rect{cx - bw - 20, 200, bw, bh};
  frequency_btn_ = SDL_Rect{cx + 20, 200, bw, bh};
  play_btn_ = SDL_Rect{cx - 120, 290, 240, 54};
  resume_btn_ = SDL_Rect{cx - 120, 260, 240, 50};
  menu_btn_ = SDL_Rect{cx - 120, 330, 240, 50};
  retry_btn_ = SDL_Rect{cx - 120, 260, 240, 50};
}

bool Game::pointInRect(int x, int y, const SDL_Rect& r) const {
  return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

void Game::startPlaying(const std::vector<float>& wave_heights) {
  state_ = GameState::Playing;
  score_ = 0;
  ball_.reset(config_.window_width, config_.window_height, wave_heights);
  pipes_.reset(config_.window_width, config_.window_height);
  pipes_.unfreeze();
}

void Game::returnToMenu() {
  state_ = GameState::ModeSelect;
  pipes_.freeze();
}

void Game::pause() {
  if (state_ == GameState::Playing) {
    state_ = GameState::Paused;
    pipes_.freeze();
  }
}

void Game::resume() {
  if (state_ == GameState::Paused) {
    state_ = GameState::Playing;
    pipes_.unfreeze();
  }
}

void Game::retry(const std::vector<float>& wave_heights) {
  startPlaying(wave_heights);
}

bool Game::handleEvent(const SDL_Event& event, const std::vector<float>& wave) {
  if (event.type == SDL_QUIT) {
    return true;
  }

  if (event.type == SDL_KEYDOWN) {
    const SDL_Keycode key = event.key.keysym.sym;
    switch (state_) {
      case GameState::ModeSelect:
        if (key == SDLK_ESCAPE) {
          return true;
        }
        if (key == SDLK_1) {
          selected_mode_ = ControlMode::Volume;
        } else if (key == SDLK_2) {
          selected_mode_ = ControlMode::Frequency;
        } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
          startPlaying(wave);
        }
        break;

      case GameState::Playing:
        if (key == SDLK_ESCAPE) {
          pause();
        } else if (key == SDLK_1) {
          selected_mode_ = ControlMode::Volume;
        } else if (key == SDLK_2) {
          selected_mode_ = ControlMode::Frequency;
        }
        break;

      case GameState::Paused:
        if (key == SDLK_ESCAPE || key == SDLK_RETURN || key == SDLK_SPACE) {
          resume();
        } else if (key == SDLK_q || key == SDLK_m) {
          returnToMenu();
        }
        break;

      case GameState::GameOver:
        if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_r) {
          retry(wave);
        } else if (key == SDLK_ESCAPE || key == SDLK_m) {
          returnToMenu();
        }
        break;
    }
  }

  if (event.type == SDL_MOUSEBUTTONDOWN &&
      event.button.button == SDL_BUTTON_LEFT) {
    const int mx = event.button.x;
    const int my = event.button.y;
    switch (state_) {
      case GameState::ModeSelect:
        if (pointInRect(mx, my, volume_btn_)) {
          selected_mode_ = ControlMode::Volume;
        } else if (pointInRect(mx, my, frequency_btn_)) {
          selected_mode_ = ControlMode::Frequency;
        } else if (pointInRect(mx, my, play_btn_)) {
          startPlaying(wave);
        }
        break;
      case GameState::Paused:
        if (pointInRect(mx, my, resume_btn_)) {
          resume();
        } else if (pointInRect(mx, my, menu_btn_)) {
          returnToMenu();
        }
        break;
      case GameState::GameOver:
        if (pointInRect(mx, my, retry_btn_)) {
          retry(wave);
        } else if (pointInRect(mx, my, menu_btn_)) {
          returnToMenu();
        }
        break;
      default:
        break;
    }
  }

  return false;
}

void Game::update(float dt, const std::vector<float>& wave_heights) {
  if (state_ != GameState::Playing) {
    return;
  }

  ball_.update(dt, config_.window_width, config_.window_height, wave_heights);
  pipes_.update(dt, config_.window_width, config_.window_height);
  score_ += pipes_.collectScore(ball_.x());

  if (pipes_.collides(ball_.x(), ball_.y(), ball_.radius(),
                      config_.window_height)) {
    state_ = GameState::GameOver;
    pipes_.freeze();
  }
}

void Game::drawOverlay(SDL_Renderer* renderer,
                       TTF_Font* font,
                       TTF_Font* font_large) const {
  if (renderer == nullptr) {
    return;
  }

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  const SDL_Color white{235, 240, 245, 255};
  const SDL_Color dim{180, 190, 200, 255};
  const int cx = static_cast<int>(config_.window_width / 2.0f);

  if (state_ == GameState::ModeSelect) {
    SDL_Rect veil{0, 0, static_cast<int>(config_.window_width),
                  static_cast<int>(config_.window_height)};
    SDL_SetRenderDrawColor(renderer, 6, 12, 18, 110);
    SDL_RenderFillRect(renderer, &veil);

    drawTextCentered(renderer, font_large ? font_large : font, "Wave Game", cx,
                     110, white);
    drawTextCentered(renderer, font, "Choose a control mode, then Play", cx, 160,
                     dim);

    fillButton(renderer, volume_btn_,
               selected_mode_ == ControlMode::Volume, false);
    fillButton(renderer, frequency_btn_,
               selected_mode_ == ControlMode::Frequency, true);
    drawTextCentered(renderer, font, "1  Volume",
                     volume_btn_.x + volume_btn_.w / 2,
                     volume_btn_.y + 16, white);
    drawTextCentered(renderer, font, "2  Frequency",
                     frequency_btn_.x + frequency_btn_.w / 2,
                     frequency_btn_.y + 16, white);

    fillButton(renderer, play_btn_, true, selected_mode_ == ControlMode::Frequency);
    drawTextCentered(renderer, font, "Play  (Space / Enter)",
                     play_btn_.x + play_btn_.w / 2, play_btn_.y + 15, white);

    drawTextCentered(renderer, font, "Esc quits   |   Live mic preview behind",
                     cx, 370, dim);
  }

  if (state_ == GameState::Playing) {
    drawText(renderer, font, "Score: " + std::to_string(score_), 40, 100,
             white);
    drawText(renderer, font, "Esc = Pause", 40, 128, dim);
  }

  if (state_ == GameState::Paused) {
    SDL_Rect veil{0, 0, static_cast<int>(config_.window_width),
                  static_cast<int>(config_.window_height)};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
    SDL_RenderFillRect(renderer, &veil);
    drawTextCentered(renderer, font_large ? font_large : font, "Paused", cx, 180,
                     white);
    fillButton(renderer, resume_btn_, true, false);
    drawTextCentered(renderer, font, "Resume  (Esc / Space)",
                     resume_btn_.x + resume_btn_.w / 2, resume_btn_.y + 14,
                     white);
    fillButton(renderer, menu_btn_, false, false);
    drawTextCentered(renderer, font, "Menu  (Q / M)",
                     menu_btn_.x + menu_btn_.w / 2, menu_btn_.y + 14, white);
  }

  if (state_ == GameState::GameOver) {
    SDL_Rect veil{0, 0, static_cast<int>(config_.window_width),
                  static_cast<int>(config_.window_height)};
    SDL_SetRenderDrawColor(renderer, 20, 8, 8, 160);
    SDL_RenderFillRect(renderer, &veil);
    drawTextCentered(renderer, font_large ? font_large : font, "Game Over", cx,
                     170, white);
    drawTextCentered(renderer, font,
                     "Score: " + std::to_string(score_), cx, 220, dim);
    fillButton(renderer, retry_btn_, true, false);
    drawTextCentered(renderer, font, "Retry  (R / Space)",
                     retry_btn_.x + retry_btn_.w / 2, retry_btn_.y + 14, white);
    fillButton(renderer, menu_btn_, false, false);
    drawTextCentered(renderer, font, "Menu  (M / Esc)",
                     menu_btn_.x + menu_btn_.w / 2, menu_btn_.y + 14, white);
  }
}
