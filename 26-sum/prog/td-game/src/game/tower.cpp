#include "tower.h"

#include <algorithm>

#include "game.h"

Tower::TowerDamage::TowerDamage(int damage, float duration, std::shared_ptr<Enemy> enemy)
    : CountdownTimer(duration, true), damage(damage), enemy(enemy) { }

Tower::Tower(std::string const& type, Tile const& tile, int health, int resource_cost)
    : type_(type),
      tile_(&tile),
      health_(health),
      max_health_(health),
      resource_cost_(resource_cost) { }
Tower::~Tower() = default;

Tile const* Tower::tile() const { return tile_; }
std::string const& Tower::type() const { return type_; }
int Tower::health() const { return health_; }
int Tower::max_health() const { return max_health_; }
void Tower::set_health(int health) { health_ = health; }
void Tower::increase_health(int amount) { health_ += amount; }
void Tower::decrease_health(int amount) { health_ -= amount; }
bool Tower::is_destroyed() const { return health_ <= 0; }
int Tower::resource_cost() const {
  return std::max(
    1,
    static_cast<int>(static_cast<float>(resource_cost_) * tile_->resource_cost_factor())
  );
}

void Tower::apply_tower_damage(std::shared_ptr<Enemy> enemy) {
  for (auto const& td : tower_damage_) {
    if (td.enemy.lock() == enemy) return;  // should not be attacked by the same enemy twice
  }
  tower_damage_.emplace_back(enemy->tower_damage(), enemy->tower_damage_cooldown(), enemy);
}

bool Tower::can_place_on(Tile const& tile) const {
  return tile.can_place_tower() && !tile.occupied_by_tower();
}

AttackTower::AttackTower(
  std::string const& type,
  Tile const& tile,
  int health,
  int resource_cost,
  float attack_cooldown,
  float range,
  Targeting targeting,
  BulletFactory make_bullet,
  float bullet_speed
)
    : Tower(type, tile, health, resource_cost),
      attack_cooldown_(attack_cooldown, false),
      range_(range),
      targeting_(targeting),
      make_bullet_(std::move(make_bullet)),
      bullet_speed_(bullet_speed) { }

bool AttackTower::can_attack() const { return attack_cooldown_.is_finished(); }
void AttackTower::reset_attack_cooldown() { attack_cooldown_.reset(); }
Vec2 AttackTower::aim() const { return aim_; }

std::unique_ptr<Bullet> AttackTower::release_bullet(Vec2 target_pos) {
  return make_bullet_(tile()->position(), target_pos);
}
float AttackTower::bullet_speed() const { return bullet_speed_; }

ResourceTower::ResourceTower(
  std::string const& type,
  Tile const& tile,
  int health,
  int resource_cost,
  int resource_amount,
  float resource_cooldown
)
    : Tower(type, tile, health, resource_cost),
      resource_amount_(resource_amount),
      resource_cooldown_(resource_cooldown, false) { }

bool ResourceTower::can_generate_resource() const { return resource_cooldown_.is_finished(); }
void ResourceTower::reset_resource_cooldown() { resource_cooldown_.reset(); }
int ResourceTower::resource_amount() const { return resource_amount_; }

void Tower::update(Game& /*game*/, float dt) {
  for (auto it = tower_damage_.begin(); it != tower_damage_.end();) {
    if (!it->enemy.lock()) {
      // Enemy is no longer alive: drop the entry (swap-and-pop, order irrelevant).
      *it = std::move(tower_damage_.back());
      tower_damage_.pop_back();
      continue;
    }

    it->update(dt);
    if (it->is_finished()) {
      decrease_health(it->damage);
      it->reset();  // Reset the timer for the next enemy attack.
    }
    ++it;
  }
}

void AttackTower::update(Game& game, float dt) {
  Tower::update(game, dt);

  attack_cooldown_.update(dt);
  if (can_attack()) {
    if (Enemy* target = select_target(game.enemies())) {
      // Lead the target: aim where it will be when the bullet arrives.
      // Refine once: the bullet travels to the predicted point, whose distance
      // differs from the current distance, so recompute the lead from that.
      float speed = bullet_speed();
      Vec2 src = tile()->position();
      float lead = speed > 0.0f ? (target->position() - src).length() / speed : 0.0f;
      Vec2 aim = target->predicted_position(lead);
      if (speed > 0.0f) {
        lead = (aim - src).length() / speed;
        aim = target->predicted_position(lead);
      }
      aim_ = (aim - src).normalized();
      if (aim_.length_sq() == 0.0f) aim_ = {1.0f, 0.0f};

      game.spawn_bullet(release_bullet(aim));
      reset_attack_cooldown();
    }
  }
}

Enemy* AttackTower::select_target(std::vector<std::shared_ptr<Enemy>> const& enemies) const {
  Enemy* best = nullptr;
  float best_key = 0.0f;
  Vec2 const pos = tile()->position();

  for (auto const& enemy : enemies) {
    if (enemy->is_destroyed()) continue;
    Vec2 const to_enemy = enemy->position() - pos;
    if (to_enemy.length_sq() > range_ * range_) continue;

    float key = 0.0f;
    switch (targeting_) {
    case Targeting::First:
      key = enemy->path_distance();
      break;
    case Targeting::Closest:
      key = -to_enemy.length();
      break;
    case Targeting::Strongest:
      key = static_cast<float>(enemy->health());
      break;
    }

    if (best == nullptr || key > best_key) {
      best = enemy.get();
      best_key = key;
    }
  }

  return best;
}

Vec2 BulletSpecBase::aimed_velocity(std::mt19937& rng, Vec2 source, Vec2 target) const {
  Vec2 dir = (target - source).normalized();
  if (dir.length_sq() == 0.0f) dir = Vec2(1.0f, 0.0f);

  std::uniform_real_distribution<float> speed_dist(min_speed, max_speed);
  std::uniform_real_distribution<float> angle_dist(-max_angle_deviation, max_angle_deviation);

  return dir.rotated(angle_dist(rng)) * speed_dist(rng);
}

void ResourceTower::update(Game& game, float dt) {
  Tower::update(game, dt);

  resource_cooldown_.update(dt);
  if (can_generate_resource()) {
    game.grant_resource(resource_amount_);
    reset_resource_cooldown();
  }
}
