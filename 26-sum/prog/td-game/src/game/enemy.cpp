#include "enemy.h"

#include <algorithm>
#include <random>

#include "game.h"
#include "path.h"
#include "status_effect.h"

Enemy::Enemy(
  std::string const& type,
  Rect bounds,
  float speed,
  int health,
  int tower_damage,
  float tower_damage_cooldown
)
    : type_(type),
      bounds_(bounds),
      speed_(speed),
      health_(health),
      max_health_(health),
      tower_damage_(tower_damage),
      tower_damage_cooldown_(tower_damage_cooldown) { }
Enemy::~Enemy() = default;

std::string const& Enemy::type() const { return type_; }
Rect Enemy::bounds() const { return bounds_; }
Vec2 Enemy::position() const { return bounds_.center; }
int Enemy::health() const { return health_; }
int Enemy::max_health() const { return max_health_; }
void Enemy::set_health(int health) { health_ = health; }
void Enemy::increase_health(int amount) { health_ += amount; }
void Enemy::decrease_health(int amount) { health_ -= mitigate(amount); }
bool Enemy::is_destroyed() const { return health_ <= 0; }
int Enemy::tower_damage() const { return tower_damage_; }
float Enemy::tower_damage_cooldown() const { return tower_damage_cooldown_; }
int Enemy::score_value() const { return score_; }
void Enemy::set_score(int s) { score_ = s; }

Path const* Enemy::path() const { return path_; }
void Enemy::set_path(Path const* path) { path_ = path; }
float Enemy::path_distance() const { return path_distance_; }
void Enemy::set_path_distance(float d) { path_distance_ = d; }

bool Enemy::portal_ready() const { return portal_cooldown_.is_finished(); }
void Enemy::start_portal_cooldown(float duration) {
  portal_cooldown_ = CountdownTimer(duration, true);
}

float Enemy::slow_resist_factor() const { return 1.0f; }
float Enemy::splash_damage_factor() const { return 1.0f; }
int Enemy::mitigate(int damage) const { return std::max(0, damage); }
void Enemy::on_death(Game&) { }

void Enemy::apply_status_effect(std::unique_ptr<StatusEffect> effect) {
  effect->on_apply(*this);
  status_effects_.push_back(std::move(effect));
}

void Enemy::sync_position() {
  if (path_) {
    bounds_.center = path_->position_at(path_distance_);
  }
}

Vec2 Enemy::predicted_position(float time_ahead) const {
  if (path_) {
    return path_->position_at(path_distance_ + speed_ * time_ahead);
  }
  return bounds_.center;
}

Enemy::StatusHint Enemy::status_hint() const {
  StatusHint hint;

  for (auto const& effect : status_effects_) {
    if (effect->expired()) continue;
    switch (effect->type()) {
    case StatusEffect::Type::Slow:
      hint.slow = true;
      break;
    case StatusEffect::Type::Poison:
      hint.poison = true;
      break;
    case StatusEffect::Type::Regen:
      hint.regen = true;
      break;
    }
  }

  return hint;
}

void Enemy::update(Game& game, float dt) {
  bool rooted = false;
  float slow_mult = 1.0f;

  // Compute the effective speed multiplier from all active status effects and terrain.
  for (auto const& effect : status_effects_) {
    if (!effect->expired()) {
      rooted = rooted || effect->roots();
      slow_mult *= effect->speed_multiplier();
    }
  }

  Tile const* tile = game.map().tile_at(position());
  float terrain_speed_factor = tile ? tile->enemy_speed_factor() : 1.0f;

  // Update velocity factor and position based on terrain and status effects.
  float velocity_factor;
  if (rooted) {
    velocity_factor = 0.0f;
  } else {
    velocity_factor = terrain_speed_factor * std::min(1.0f, slow_mult * slow_resist_factor());
  }

  if (path_) {
    path_distance_ += speed_ * dt * velocity_factor;
    bounds_.center = path_->position_at(path_distance_);
  }

  // Update all status effects and remove expired ones.
  for (auto& effect : status_effects_) {
    effect->update(*this, dt);
  }

  status_effects_.erase(
    std::remove_if(
      status_effects_.begin(),
      status_effects_.end(),
      [](auto const& effect) { return effect->expired(); }
    ),
    status_effects_.end()
  );

  // Update the portal cooldown timer.
  portal_cooldown_.update(dt);
}

ResistantEnemy::ResistantEnemy(
  std::string const& type,
  Rect bounds,
  float speed,
  int health,
  int tower_damage,
  float tower_damage_cooldown,
  float slow_resist,
  float splash_resist
)
    : Enemy(type, bounds, speed, health, tower_damage, tower_damage_cooldown),
      slow_resist_(slow_resist),
      splash_resist_(splash_resist) { }

float ResistantEnemy::slow_resist_factor() const { return slow_resist_; }
float ResistantEnemy::splash_damage_factor() const { return splash_resist_; }

SplitterEnemy::SplitterEnemy(
  std::string const& type,
  Rect bounds,
  float speed,
  int health,
  int tower_damage,
  float tower_damage_cooldown,
  ChildSpec const& child_spec
)
    : Enemy(type, bounds, speed, health, tower_damage, tower_damage_cooldown),
      child_spec_(child_spec) { }

void SplitterEnemy::on_death(Game& game) {
  for (int i = 0; i < child_spec_.count; ++i) {
    auto child = std::make_shared<Enemy>(
      "normal",
      Rect(bounds_.center, child_spec_.width, child_spec_.height),
      child_spec_.speed,
      child_spec_.health,
      child_spec_.tower_damage,
      child_spec_.tower_damage_cooldown
    );
    child->set_path(path_);
    // Perturb the child's path distance so siblings spread along the path
    // instead of stacking on the exact same spot.
    float dist = path_distance_;
    if (child_spec_.perturbation > 0.0f) {
      float offset =
        std::uniform_real_distribution<float>(-child_spec_.perturbation, child_spec_.perturbation)(
          game.rng()
        );
      dist = std::max(0.0f, dist + offset);
      if (path_ && dist >= path_->total_length())
        dist = path_->total_length() - 0.01f;  // don't reach the exit
    }
    child->set_path_distance(dist);
    child->sync_position();
    child->set_score(child_spec_.score);
    game.spawn_enemy(std::move(child));
  }
}

BossEnemy::BossEnemy(
  std::string const& type,
  Rect bounds,
  float speed,
  int health,
  int tower_damage,
  float tower_damage_cooldown,
  int shield,
  float regen_amount,
  float regen_duration,
  float regen_interval
)
    : Enemy(type, bounds, speed, health, tower_damage, tower_damage_cooldown),
      shield_(shield),
      regen_amount_(regen_amount),
      regen_duration_(regen_duration),
      regen_interval_(regen_interval),
      regen_timer_(regen_interval, true) { }

int BossEnemy::mitigate(int damage) const { return std::max(0, damage - shield_); }

void BossEnemy::update(Game& game, float dt) {
  Enemy::update(game, dt);

  if (regen_interval_ > 0.0f) {
    regen_timer_.update(dt);
    if (regen_timer_.is_finished()) {
      apply_status_effect(std::make_unique<RegenerationEffect>(regen_amount_, regen_duration_));
      regen_timer_.reset();
    }
  }
}
