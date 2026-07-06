#include "game.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>

#include "factory.h"

namespace {
// Resolve the stats + tile for a would-be tower of `type` at `position`, or
// nullopt if the type is unknown or the position is off the map. Used by
// Game::place_tower so the lookup + validity checks live in one place.
std::optional<std::pair<TowerStats const*, Tile const*>>
resolve_tower(Level const& level, Map const& map, std::string const& type, Vec2 position) {
  auto it = level.tower_stats.find(type);
  if (it == level.tower_stats.end()) return std::nullopt;
  Tile const* tile = map.tile_at(position);
  if (!tile) return std::nullopt;
  return std::pair{&it->second, tile};
}

// --- view helpers (model -> view projections not expressible as a direct
// accessor on the model class) ---

Vec2 tower_aim(Tower const& t) {
  if (auto* a = dynamic_cast<AttackTower const*>(&t)) return a->aim();
  return {0.0f, 0.0f};
}

float bullet_radius(Bullet const& b) {
  if (auto* s = dynamic_cast<SplashBullet const*>(&b)) return s->radius();
  return 0.0f;
}

std::string tile_terrain(Tile const& t) {
  if (t.is_portal()) return "portal";
  if (t.enemy_speed_factor() > 1.0f) return "ice";
  if (!t.can_place_tower()) return "rock";
  if (t.resource_cost_factor() < 1.0f) return "fertile";
  return "grass";
}
}  // namespace

Level const& Game::level() const { return *level_; }
Map const& Game::map() const { return level().map; }
Resource const& Game::resource() const { return resource_; }

std::vector<std::unique_ptr<Tower>> const& Game::towers() const { return towers_; }
std::vector<std::shared_ptr<Enemy>> const& Game::enemies() const { return enemies_; }
std::vector<std::unique_ptr<Bullet>> const& Game::bullets() const { return bullets_; }
Game::State Game::state() const { return state_; }
bool Game::over() const { return state_ != State::Playing; }
bool Game::paused() const { return paused_; }
int Game::score() const { return score_; }
float Game::elapsed_time() const { return elapsed_time_; }
int Game::current_wave() const { return waves_.current_wave(); }
bool Game::all_waves_done() const { return waves_.all_waves_done(); }
Game::Result Game::last_result() const { return last_result_; }
std::mt19937& Game::rng() { return rng_; }

void Game::start_level(Level const& level) {
  level_ = std::make_unique<Level>(level);  // owned copy — survives registry reallocation
  level_->map.clear_occupancy();
  clear_entities();
  pending_spawns_.clear();
  resource_ = Resource(
    level_->starting_resources,
    {level_->resource_auto_inc_amount, level_->resource_auto_inc_interval}
  );
  std::vector<Path const*> paths;
  paths.reserve(level_->paths.size());
  for (auto const& p : level_->paths) paths.push_back(&p);
  waves_.start(WaveSpec{&level_->waves, &level_->enemy_stats, std::move(paths)});
  score_ = 0;
  elapsed_time_ = 0.0f;
  cheated_ = false;
  paused_ = false;
  state_ = State::Playing;
}

void Game::end_level(bool cleared) {
  clear_entities();
  state_ = cleared ? State::Victory : State::Defeat;
  last_result_ = {cleared, cheated_, elapsed_time_, score_};
}

void Game::grant_resource(int amount) { resource_.increase(amount); }

int Game::place_tower(std::string const& type, Vec2 position) {
  if (state_ != State::Playing || paused_) return 1;

  // The level may restrict which tower types are available.
  auto const& avail = level().available_towers;
  if (std::find(avail.begin(), avail.end(), type) == avail.end()) return 1;

  auto site = resolve_tower(level(), map(), type, position);
  if (!site) return 1;
  auto [stats, tile] = *site;

  auto tower = make_tower(type, *tile, *stats, rng_, map());
  int cost = tower->effective_resource_cost();
  if (!tower->can_place_on(*tile)) return 1;
  if (!resource_.decrease(cost)) return 2;

  tile->set_occupied_by_tower(true);
  towers_.push_back(std::move(tower));
  events_.push_back({"tower_placed", towers_.back()->tile()->position(), cost, type, false, false});
  return 0;
}

void Game::spawn_enemy(std::shared_ptr<Enemy> enemy) {
  pending_spawns_.push_back(std::move(enemy));
}
void Game::spawn_bullet(std::unique_ptr<Bullet> bullet) { bullets_.push_back(std::move(bullet)); }

void Game::clear_entities() {
  towers_.clear();
  enemies_.clear();
  bullets_.clear();
}

void Game::pause() { paused_ = true; }
void Game::resume() { paused_ = false; }
void Game::restart() { start_level(*level_); }

void Game::apply_cheat(std::string_view code) {
  cheated_ = true;
  if (code == "gold") {
    resource_.increase(1000);
  } else if (code == "killall") {
    enemies_.clear();
    pending_spawns_.clear();
  } else if (code == "win") {
    if (state_ == State::Playing) end_level(true);
  }
}

bool Game::update(float dt) {
  if (!level_ || state_ != State::Playing) return true;
  if (paused_) return false;

  elapsed_time_ += dt;

  resource_.update(dt);  // automatic resource increase over time

  // Flush enemies queued since the last update (wave spawns + on-death spawns).
  for (auto& enemy : pending_spawns_) {
    enemies_.push_back(std::move(enemy));
  }
  pending_spawns_.clear();

  waves_.update(*this, dt);

  if (update_movables(dt)) {
    end_level(false);  // an enemy reached the exit -> defeat
    return true;
  }
  check_collisions();
  update_towers(dt);

  // Victory: every wave's spawns issued and nothing left alive.
  if (waves_.all_waves_done() && enemies_.empty() && pending_spawns_.empty()) {
    end_level(true);
    return true;
  }
  return false;
}

bool Game::update_movables(float dt) {
  for (auto it = enemies_.begin(); it != enemies_.end();) {
    auto& enemy = *it;
    if (enemy->is_destroyed()) {
      ++it;  // destroyed enemies are cleaned up in check_collisions()
      continue;
    }

    // Defeat: enemy reached the end of its route (the destination).
    if (enemy->path() && enemy->path_distance() >= enemy->path()->total_length()) {
      return true;
    }

    // Update the enemy's position along its path.
    float old_distance = enemy->path_distance();
    enemy->update(*this, dt);

    Tile const* new_tile = map().tile_at(enemy->position());
    if (new_tile && new_tile->occupied_by_tower()) {
      // Blocked by a tower: revert movement and attack the tower.
      enemy->set_path_distance(old_distance);
      enemy->sync_position();
      for (auto& tower : towers_) {
        if (tower->tile() == new_tile) {
          tower->apply_tower_damage(enemy);
          break;
        }
      }
    } else if (new_tile && new_tile->is_portal() && enemy->portal_ready()) {
      // Portal: jump to the paired portal's path distance on this enemy's route.
      float dest = enemy->path()->paired_portal_distance(new_tile->position());
      if (dest >= 0.0f) {
        enemy->set_path_distance(dest);
        enemy->sync_position();
        enemy->start_portal_cooldown(PORTAL_COOLDOWN);
      }
    }

    ++it;
  }

  for (auto it = bullets_.begin(); it != bullets_.end();) {
    auto& bullet = *it;

    // Remove bullets that are out of bounds.
    if (
      bullet->position().x < 0 || bullet->position().x > map().width() ||
      bullet->position().y < 0 || bullet->position().y > map().height()
    ) {
      it = bullets_.erase(it);
      continue;
    }

    bullet->update(dt);
    ++it;
  }

  return false;
}

void Game::check_collisions() {
  std::vector<std::size_t> bullets_to_remove;

  // bullet vs enemy
  for (auto bullet_it = bullets_.begin(); bullet_it != bullets_.end(); ++bullet_it) {
    auto bullet = bullet_it->get();
    if (!bullet->can_explode(enemies_)) continue;  // not touching any enemy yet

    bool hit = false;
    for (auto& enemy : enemies_) {
      if (enemy->is_destroyed()) continue;
      if (!bullet->effective(*enemy)) continue;

      bullet->impact(*enemy);
      hit = true;
      if (!bullet->pierces()) break;
    }

    if (hit) {
      bullets_to_remove.push_back(static_cast<std::size_t>(bullet_it - bullets_.begin()));
    }
  }

  // Remove all destroyed enemies (killed by bullets, poison, or tower damage),
  // triggering on-death effects (e.g. splitter spawns) and notifying observers.
  for (auto it = enemies_.begin(); it != enemies_.end();) {
    auto& enemy = *it;
    if (enemy->is_destroyed()) {
      score_ += enemy->score();
      events_.push_back(
        {"enemy_killed", enemy->position(), enemy->score(), enemy->type(), false, false}
      );
      enemy->on_death(*this);
      it = enemies_.erase(it);
    } else {
      ++it;
    }
  }

  // Remove bullets that hit enemies (indices are ascending; erase descending).
  for (auto it = bullets_to_remove.rbegin(); it != bullets_to_remove.rend(); ++it) {
    bullets_.erase(bullets_.begin() + static_cast<std::ptrdiff_t>(*it));
  }
}

void Game::update_towers(float dt) {
  for (auto it = towers_.begin(); it != towers_.end();) {
    auto& tower = *it;
    tower->update(*this, dt);

    if (tower->is_destroyed()) {
      // The tower already holds its tile; clear occupancy directly (no lookup).
      tower->tile()->set_occupied_by_tower(false);
      it = towers_.erase(it);
    } else {
      ++it;
    }
  }
}

// --- view producers (temporary C++; replaced by FFI when the model is Rust) ---

std::vector<TowerView> Game::tower_views() const {
  std::vector<TowerView> out;
  out.reserve(towers_.size());
  for (auto const& t : towers_) {
    out.push_back({
      t->tile()->position(),
      tower_aim(*t),
      t->type(),
      t->health(),
      t->max_health(),
    });
  }
  return out;
}

std::vector<EnemyView> Game::enemy_views() const {
  std::vector<EnemyView> out;
  for (auto const& e : enemies_) {
    if (e->is_destroyed()) continue;
    auto h = e->status_hint();
    out.push_back({
      e->position(),
      e->bounds().width * 0.5f,
      e->bounds().height * 0.5f,
      e->type(),
      e->health(),
      e->max_health(),
      StatusFlags{h.slow, h.poison, h.regen},
    });
  }
  return out;
}

std::vector<BulletView> Game::bullet_views() const {
  std::vector<BulletView> out;
  out.reserve(bullets_.size());
  for (auto const& b : bullets_) {
    out.push_back({
      b->position(),
      b->velocity(),
      b->type(),
      bullet_radius(*b),
    });
  }
  return out;
}

LevelView Game::level_view() const {
  LevelView lv;
  lv.index = level_->index;
  lv.name = level_->name;
  lv.map_width = level_->map.width();
  lv.map_height = level_->map.height();
  lv.terrain.reserve(level_->map.tiles().size());
  for (auto const& tile : level_->map.tiles()) {
    lv.terrain.push_back({tile_terrain(tile)});
  }
  lv.path_waypoints.reserve(level_->paths.size());
  for (auto const& path : level_->paths) {
    lv.path_waypoints.push_back(path.waypoints());
  }
  lv.available_towers = level_->available_towers;
  lv.tower_costs.reserve(level_->available_towers.size());
  for (auto const& type : level_->available_towers) {
    auto it = level_->tower_stats.find(type);
    if (it != level_->tower_stats.end()) {
      lv.tower_costs.push_back({type, it->second.resource_cost});
    }
  }
  return lv;
}

std::vector<GameEventView> Game::take_events() { return std::move(events_); }

GameResultView Game::last_result_view() const {
  return {last_result_.cleared, last_result_.cheated, last_result_.time, last_result_.score};
}

bool Game::can_place_at(Vec2 pos) const {
  Tile const* tile = map().tile_at(pos);
  return tile && tile->can_place_tower() && !tile->occupied_by_tower();
}

void Game::push_wave_started(int wave, bool has_boss, bool is_last) {
  events_.push_back({"wave_started", {0.0f, 0.0f}, wave, "", has_boss, is_last});
}
