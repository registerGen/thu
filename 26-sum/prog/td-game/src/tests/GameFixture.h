#pragma once

#include <string>

#include "game/game.h"
#include "game/level.h"

/// Owns a `LevelRegistry` + `Game` and wires them, for tests that previously
/// constructed `Game(config_dir)` directly. This is NOT a facade — it does not
/// mirror Game's old level API. Tests use `fx.game` / `fx.registry` directly and
/// call `advance()`/`select()`/`play()` for level transitions.
struct GameFixture {
  LevelRegistry registry;
  Game game;

  explicit GameFixture(std::string const& dir) {
    registry.load_from_config(dir);
    game.start_level(registry.current());
  }

  bool advance() {
    if (!registry.has_next_official()) return false;
    registry.advance();
    game.start_level(registry.current());
    return true;
  }
  void select(int i) {
    registry.select(i);
    game.start_level(registry.current());
  }
  void play(Level const& level) {
    // Mirror production GameController::playCustomLevel: play the level directly
    // without adding it to the registry.
    game.start_level(level);
  }
};
