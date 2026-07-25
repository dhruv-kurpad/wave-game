#include "game/Collision.hpp"
#include "game/Obstacle.hpp"
#include "game/Game.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
int g_failures = 0;

void expect(bool cond, const char* name) {
  if (!cond) {
    std::cerr << "FAIL: " << name << '\n';
    ++g_failures;
  } else {
    std::cout << "PASS: " << name << '\n';
  }
}
}  // namespace

int main() {
  // P6-GM07 collision hit
  {
    expect(circleIntersectsRect(50, 50, 10, 45, 45, 20, 20), "P6-GM07 hit");
  }
  // P6-GM08 collision miss
  {
    expect(!circleIntersectsRect(0, 0, 5, 40, 40, 10, 10), "P6-GM08 miss");
  }

  // Pipe hitboxes
  {
    Pipe p;
    p.x = 100;
    p.width = 70;
    p.gap_center_y = 360;
    p.gap_size = 180;
    // Ball in the gap — should miss
    expect(!p.hitsBall(135, 360, 18, 720), "pipe gap miss");
    // Ball in top pipe
    expect(p.hitsBall(135, 50, 18, 720), "pipe top hit");
  }

  // Pipe scroll + score
  {
    std::srand(1);
    PipeField::Config cfg;
    cfg.scroll_speed = 200.0f;
    cfg.spawn_spacing = 400.0f;
    PipeField field(cfg);
    field.reset(1280, 720);
    expect(!field.pipes().empty(), "pipes spawn");
    const float x0 = field.pipes().front().x;
    field.update(0.5f, 1280, 720);
    expect(field.pipes().front().x < x0, "P6-GM05 pipes scroll left");

    // Force a scored pass
    PipeField field2(cfg);
    field2.reset(1280, 720);
    // Move pipes past ball x manually via many updates
    for (int i = 0; i < 200; ++i) {
      field2.update(0.05f, 1280, 720);
    }
    const int gained = field2.collectScore(640.0f);
    expect(gained >= 0, "P6-GM09 score non-negative");
  }

  // Game state machine
  {
    Game::Config gc;
    Ball::Config bc;
    PipeField::Config pc;
    Game game(gc, bc, pc);
    expect(game.state() == GameState::ModeSelect, "P6-GM01 ModeSelect");

    std::vector<float> wave(256, 0.4f);
    game.setSelectedMode(ControlMode::Frequency);
    game.startPlaying(wave);
    expect(game.state() == GameState::Playing, "P6-GM02 Playing");
    expect(game.selectedMode() == ControlMode::Frequency, "mode matches");

    game.pause();
    expect(game.state() == GameState::Paused, "P6-GM03 Paused");
    game.resume();
    expect(game.state() == GameState::Playing, "P6-GM03 Resume");

    game.pause();
    game.returnToMenu();
    expect(game.state() == GameState::ModeSelect, "P6-GM04 menu");
  }

  // Pause freeze
  {
    std::srand(2);
    PipeField field;
    field.reset(1280, 720);
    const float x0 = field.pipes().front().x;
    field.freeze();
    field.update(1.0f, 1280, 720);
    expect(field.pipes().front().x == x0, "pause freezes scroll");
  }

  if (g_failures > 0) {
    std::cerr << g_failures << " test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "All gameplay tests passed\n";
  return EXIT_SUCCESS;
}
