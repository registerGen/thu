#include "wave.h"

#include "factory.h"
#include "game.h"
#include "observer.h"

int WaveManager::current_wave() const { return current_wave_ + 1; }
bool WaveManager::all_waves_done() const { return all_done_; }

void WaveManager::start(WaveSpec const& spec) {
  spec_ = spec;
  current_wave_ = -1;
  wave_time_ = 0.0f;
  all_done_ = false;
  if (!spec_.waves || spec_.waves->empty()) {
    in_gap_ = false;
    all_done_ = true;
  } else {
    enter_gap(0);
  }
}

void WaveManager::enter_gap(int index) {
  in_gap_ = true;
  gap_timer_ = CountdownTimer((*spec_.gaps)[static_cast<std::size_t>(index)], true);
}

void WaveManager::start_wave(Game& game, int index) {
  current_wave_ = index;
  in_gap_ = false;
  wave_time_ = 0.0f;

  auto const& spawns = (*spec_.waves)[static_cast<std::size_t>(index)].spawns;
  spawned_.assign(spawns.size(), false);

  int wave_count = static_cast<int>(spec_.waves->size());
  if (index >= 0 && index < wave_count) {
    bool has_boss = false;
    bool is_last = (index == wave_count - 1);

    for (auto const& s : spawns) {
      if (s.type == "boss") {
        has_boss = true;
        break;
      }
    }

    for (auto* o : game.observers()) o->on_wave_started(index + 1, has_boss, is_last);
  }
}

void WaveManager::update(Game& game, float dt) {
  if (all_done_ || !spec_.waves) return;

  // Pre-wave gap phase: wait gaps[current_wave_] before the wave starts.
  if (in_gap_) {
    gap_timer_.update(dt);
    if (gap_timer_.is_finished()) {
      start_wave(game, current_wave_ + 1);
    }
    return;
  }

  if (current_wave_ >= static_cast<int>(spec_.waves->size())) {
    all_done_ = true;
    return;
  }

  wave_time_ += dt;
  Wave const& wave = (*spec_.waves)[static_cast<std::size_t>(current_wave_)];
  bool all_spawned = true;

  for (std::size_t i = 0; i < wave.spawns.size(); ++i) {
    if (!spawned_[i] && wave_time_ >= wave.spawns[i].time) {
      spawned_[i] = true;
      int route = wave.spawns[i].route;  // validated at load time

      auto enemy = make_enemy(
        wave.spawns[i].type,
        spec_.enemy_stats->at(wave.spawns[i].type),
        spec_.routes[static_cast<std::size_t>(route)]
      );
      game.spawn_enemy(std::move(enemy));
    }

    if (!spawned_[i]) all_spawned = false;
  }

  // Once all spawns of this wave are issued, enter the gap before the next wave
  // (or finish if this was the last wave).
  if (all_spawned) {
    int next = current_wave_ + 1;
    if (next >= static_cast<int>(spec_.waves->size())) {
      all_done_ = true;
    } else {
      enter_gap(next);
    }
  }
}
