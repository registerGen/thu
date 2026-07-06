#pragma once

class Tower;
class Enemy;

/// Observer interface for discrete game events. GameController implements this
/// to forward events as Qt signals for the view layer. Register with
/// Game::add_observer.
class GameObserver {
public:
  virtual ~GameObserver() = default;

  virtual void on_tower_placed(Tower const&, int /*cost*/) { }
  virtual void on_enemy_killed(Enemy const&) { }
  virtual void on_wave_started(int /*wave*/, bool /*has_boss*/, bool /*is_last*/) { }
};
