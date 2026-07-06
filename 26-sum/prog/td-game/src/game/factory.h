#pragma once

#include <memory>
#include <string>

#include "enemy.h"
#include "map.h"
#include "path.h"
#include "stats.h"
#include "tower.h"

/// Build a tower of the given type from its stats. `map` is needed by the slow
/// tower (to amplify slow on ice). Returns nullptr for an unknown type.
std::unique_ptr<Tower> make_tower(
  std::string const& type,
  Tile const& tile,
  TowerStats const& stats,
  std::mt19937& rng,
  Map const& map
);

/// Build an enemy of the given type from its stats, attached to `path` at the
/// entrance (path distance 0). Returns nullptr for an unknown type.
std::shared_ptr<Enemy>
make_enemy(std::string const& type, EnemyStats const& stats, Path const* path);
