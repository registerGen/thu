#pragma once

#include <string>
#include <vector>

#include "path.h"
#include "stats.h"
#include "timer.h"

class Game;

/// One enemy spawn within a wave: an enemy type at a relative time on a route.
/// `route` is mandatory — every spawn must state which route the enemy follows.
struct EnemySpawn {
  std::string type;
  float time;
  int route;
};

/// A wave is a sequence of timed spawns.
struct Wave {
  std::vector<EnemySpawn> spawns;
};

/// Narrow, non-owning view of exactly the data WaveManager reads: the wave
/// schedule, the per-wave pre-delays, the enemy stats table (to build spawns),
/// and the per-route paths. Built by Game from the current Level.
struct WaveSpec {
  std::vector<Wave> const* waves = nullptr;
  std::vector<float> const* gaps = nullptr;
  EnemyStatsTable const* enemy_stats = nullptr;
  std::vector<Path const*> routes;  // route index -> Path the spawned enemy follows
};

/// Drives timed enemy spawns across the level's waves and tracks progression.
/// Each wave begins after a configurable pre-delay (`gaps[i]`):
/// `gaps[0]` elapses from game start to wave 0; `gaps[i]` (i>0) elapses between
/// the previous wave's last spawn and wave i. Wave progression is independent
/// of whether earlier enemies are still alive. The gaps vector is authored in
/// the level config and must have exactly one entry per wave.
class WaveManager {
  WaveSpec spec_;
  int current_wave_ = -1;                  // index of the wave in progress
  float wave_time_ = 0.0f;                 // seconds since the current wave started spawning
  CountdownTimer gap_timer_{0.0f, false};  // armed per-wave; wave starts when finished
  bool in_gap_ = true;                     // true while waiting for gaps[current_wave_]
  bool all_done_ = false;
  std::vector<bool> spawned_;  // which spawns of the current wave have fired

  void enter_gap(int index);  // begin waiting gaps[index] seconds before wave `index` starts
  void start_wave(Game& game, int index);

public:
  void start(WaveSpec const& spec);

  /// Advance spawns. Spawns enemies into `game` at their scheduled times.
  void update(Game& game, float dt);
  /// 1-based wave number for UI (0 before the first wave starts).
  int current_wave() const;
  /// True once every wave's spawns have been issued.
  bool all_waves_done() const;
};
