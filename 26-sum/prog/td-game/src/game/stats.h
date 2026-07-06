#pragma once

#include <map>
#include <string>

#include "tower.h"  // for AttackTower::Targeting

/// Statistics for one tower type, loaded from config. Fields not relevant to a
/// given type are left at their defaults.
struct TowerStats {
  int health = 100;
  int cost = 0;
  // attack towers:
  float attack_cooldown = 0.0f;
  float range = 0.0f;
  AttackTower::Targeting targeting = AttackTower::Targeting::First;
  float min_speed = 0.0f;
  float max_speed = 0.0f;
  float max_angle_deviation = 0.0f;
  int health_damage = 0;
  // slow tower:
  float slow_factor = 1.0f;
  float slow_duration = 0.0f;
  // poison tower:
  int poison_damage = 0;
  float poison_duration = 0.0f;
  // splash tower:
  float radius = 0.0f;
  // laser tower:
  float width = 0.0f;
  // resource tower:
  int resource_amount = 0;
  float resource_cooldown = 0.0f;
};

using TowerStatsTable = std::map<std::string, TowerStats>;

/// Statistics for one enemy type, loaded from config.
struct EnemyStats {
  int health = 1;
  float speed = 1.0f;
  int tower_damage = 0;
  float tower_damage_cooldown = 0.0f;
  float size = 0.5f;
  int score = 10;  // points awarded when killed
  // resistant:
  float slow_resist = 1.0f;
  float splash_resist = 1.0f;
  // splitter children:
  int child_count = 0;
  int child_health = 1;
  float child_speed = 1.0f;
  int child_tower_damage = 0;
  float child_tower_damage_cooldown = 0.0f;
  float child_width = 0.4f;
  float child_height = 0.4f;
  int child_score = 5;
  float child_perturbation = 0.2f;
  // boss:
  int shield = 0;
  float regen_amount = 0.0f;
  float regen_duration = 0.0f;
  float regen_interval = 0.0f;
};

using EnemyStatsTable = std::map<std::string, EnemyStats>;
