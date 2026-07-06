#include "config.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "../third_party/crjson.h"

namespace config {

namespace {

/// Read a file into a string.
std::string read_file(std::string const& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open file: " + path);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// --- accessor helpers (optional fields fall back to defaults) ---------------

float num(crjson::accessor const& a, std::string_view key, float def) {
  try {
    return static_cast<float>(a[key].as_num());
  } catch (std::exception&) {
    return def;
  }
}

int integer(crjson::accessor const& a, std::string_view key, int def) {
  try {
    return static_cast<int>(a[key].as_num());
  } catch (std::exception&) {
    return def;
  }
}

std::string str(crjson::accessor const& a, std::string_view key, std::string def = "") {
  try {
    return std::string(a[key].as_str());
  } catch (std::exception&) {
    return def;
  }
}

AttackTower::Targeting targeting_from(std::string const& s) {
  if (s == "closest") return AttackTower::Targeting::Closest;
  if (s == "strongest") return AttackTower::Targeting::Strongest;
  return AttackTower::Targeting::First;
}

}  // namespace

TowerStatsTable load_towers(std::string const& path) {
  crjson::parse doc{read_file(path)};
  auto r = doc.root();

  TowerStatsTable table;
  for (std::size_t i = 0; i < r.size(); ++i) {
    auto t = r[i];
    std::string kind = str(t, "kind", "");
    if (kind.empty()) continue;

    TowerStats s;
    s.max_health = integer(t, "max_health", 100);
    s.resource_cost = integer(t, "resource_cost", 0);
    s.attack_interval = num(t, "attack_interval", 0.0f);
    s.attack_range = num(t, "attack_range", 0.0f);
    s.targeting = targeting_from(str(t, "targeting", "first"));
    s.resource_inc_amount = integer(t, "resource_inc_amount", 0);
    s.resource_inc_interval = num(t, "resource_inc_interval", 0.0f);
    // Bullet fields (absent for resource/wall — defaults are 0).
    s.min_speed = num(t, "min_speed", 0.0f);
    s.max_speed = num(t, "max_speed", 0.0f);
    s.max_angle_deviation = num(t, "max_angle_deviation", 0.0f);
    s.health_damage = integer(t, "health_damage", 0);
    s.radius = num(t, "radius", 0.0f);
    s.width = num(t, "width", 0.0f);
    // Slow (default factor 1.0 = no slow; only meaningful for "slow").
    s.slow_factor = num(t, "slow_factor", 1.0f);
    s.slow_duration = num(t, "slow_duration", 0.0f);
    // Poison (default 0; only meaningful for "poison").
    s.poison_damage = integer(t, "poison_damage", 0);
    s.poison_duration = num(t, "poison_duration", 0.0f);
    table[kind] = s;
  }
  return table;
}

EnemyStatsTable load_enemies(std::string const& path) {
  crjson::parse doc{read_file(path)};
  auto r = doc.root();

  EnemyStatsTable table;
  for (std::size_t i = 0; i < r.size(); ++i) {
    auto t = r[i];
    std::string kind = str(t, "kind", "");
    if (kind.empty()) continue;

    EnemyStats s;
    s.max_health = integer(t, "max_health", 1);
    s.speed = num(t, "speed", 1.0f);
    s.tower_damage = integer(t, "tower_damage", 0);
    s.tower_damage_interval = num(t, "tower_damage_interval", 0.0f);
    s.width = num(t, "width", 0.5f);
    s.height = num(t, "height", 0.5f);
    s.score = integer(t, "score", 10);
    // Resist fields (default 1.0 = no resistance; only meaningful for "resistant").
    s.slow_resist = num(t, "slow_resist", 1.0f);
    s.splash_resist = num(t, "splash_resist", 1.0f);
    // Boss fields (default 0 = no shield/regen; only meaningful for "boss").
    s.shield = integer(t, "shield", 0);
    s.regen_amount = num(t, "regen_amount", 0.0f);
    s.regen_duration = num(t, "regen_duration", 0.0f);
    s.regen_interval = num(t, "regen_interval", 0.0f);
    // Child spec (only meaningful for "splitter").
    try {
      auto c = t["child"];
      s.child_count = integer(c, "count", 0);
      s.child_max_health = integer(c, "max_health", 1);
      s.child_speed = num(c, "speed", 1.0f);
      s.child_tower_damage = integer(c, "tower_damage", 0);
      s.child_tower_damage_interval = num(c, "tower_damage_interval", 0.0f);
      s.child_width = num(c, "width", 0.4f);
      s.child_height = num(c, "height", 0.4f);
      s.child_score = integer(c, "score", 5);
      s.child_perturbation = num(c, "perturbation", 0.2f);
    } catch (std::exception&) {
    }
    table[kind] = s;
  }
  return table;
}

Level load_level(
  std::string const& path,
  TowerStatsTable const& towers,
  EnemyStatsTable const& enemies
) {
  return load_level_json(read_file(path), towers, enemies);
}

Level load_level_json(
  std::string_view json,
  TowerStatsTable const& towers,
  EnemyStatsTable const& enemies
) {
  crjson::parse doc{json};
  auto r = doc.root();

  std::string name = str(r, "name", "Level");
  int index = integer(r, "index", -1);  // 1-based official; -1 custom (default)

  // --- map ----------------------------------------------------------------
  auto m = r["map"];
  int rows = static_cast<int>(m["rows"].as_num());
  int cols = static_cast<int>(m["cols"].as_num());
  auto terrain = m["terrain"];
  std::vector<Tile> tiles;
  tiles.reserve(static_cast<std::size_t>(rows) * cols);
  for (int row = 0; row < rows; ++row) {
    auto trow = terrain[row];
    for (int col = 0; col < cols; ++col) {
      std::string tname{trow[col].as_str()};
      tiles.push_back(tile_from_terrain(tname, Vec2(col + 0.5f, row + 0.5f)));
    }
  }

  // --- paths (one or more enemy paths, each with its own portals) ---------
  // Builds a Path from a JSON object with "waypoints" and "portals".
  auto build_path = [&](crjson::accessor const& path_obj) -> Path {
    std::vector<std::pair<int, int>> path_tiles;
    std::vector<Vec2> waypoints;
    auto wps = path_obj["waypoints"];
    for (std::size_t i = 0; i < wps.size(); ++i) {
      int col = static_cast<int>(wps[i][0].as_num());
      int row = static_cast<int>(wps[i][1].as_num());
      path_tiles.emplace_back(col, row);
      waypoints.push_back(Vec2(col + 0.5f, row + 0.5f));
    }

    // Portals (optional): directional [src_idx, tgt_idx] waypoint-index pairs.
    // Both endpoint tiles are flagged as portals for rendering; only the source
    // teleports. The config specifies the direction; the loader trusts it.
    std::vector<std::pair<std::size_t, std::size_t>> portal_pairs;
    try {
      auto portals = path_obj["portals"];
      for (std::size_t i = 0; i < portals.size(); ++i) {
        auto pair = portals[i];
        auto src = static_cast<std::size_t>(pair[0].as_num());
        auto tgt = static_cast<std::size_t>(pair[1].as_num());
        if (src >= path_tiles.size() || tgt >= path_tiles.size()) {
          throw std::runtime_error("portal index out of range");
        }
        auto [cA, rA] = path_tiles[src];
        auto [cB, rB] = path_tiles[tgt];
        // Each tile can only be part of one portal pair.
        auto& tileA = tiles[static_cast<std::size_t>(rA) * cols + cA];
        auto& tileB = tiles[static_cast<std::size_t>(rB) * cols + cB];
        if (tileA.is_portal() || tileB.is_portal()) {
          throw std::runtime_error("portal tile already used by another portal pair");
        }
        tileA.set_is_portal(true);
        tileB.set_is_portal(true);
        portal_pairs.emplace_back(src, tgt);
      }
    } catch (std::exception&) {
      // No portals or malformed; leave empty.
    }

    return Path(std::move(waypoints), std::move(portal_pairs));
  };

  std::vector<Path> paths;
  auto paths_arr = r["paths"];
  for (std::size_t i = 0; i < paths_arr.size(); ++i) {
    paths.push_back(build_path(paths_arr[i]));
  }

  // --- waves --------------------------------------------------------------
  auto waves_arr = r["waves"];
  std::vector<Wave> waves;
  for (std::size_t i = 0; i < waves_arr.size(); ++i) {
    Wave wave;
    wave.gap = num(waves_arr[i], "gap", 0.0f);
    auto spawns = waves_arr[i]["spawns"];
    for (std::size_t j = 0; j < spawns.size(); ++j) {
      int path = static_cast<int>(spawns[j]["path"].as_num());  // mandatory
      if (path < 0 || path >= static_cast<int>(paths.size())) {
        throw std::runtime_error(
          "level '" + name + "': wave " + std::to_string(i) + " spawn " + std::to_string(j) +
          " references unknown path " + std::to_string(path)
        );
      }
      wave.spawns.push_back(
        EnemySpawn{
          std::string(spawns[j]["kind"].as_str()),
          static_cast<float>(spawns[j]["time"].as_num()),
          path,
        }
      );
    }
    waves.push_back(std::move(wave));
  }

  // --- economy / available towers ----------------------------------------
  int starting_resources = integer(r, "starting_resources", 100);
  int ai_amount = integer(r, "resource_auto_inc_amount", 0);
  float ai_interval = num(r, "resource_auto_inc_interval", 1.0f);
  std::vector<std::string> available;
  try {
    auto at = r["available_towers"];
    for (std::size_t i = 0; i < at.size(); ++i) available.emplace_back(at[i].as_str());
  } catch (std::exception&) {
  }

  // (gaps are now per-wave, inside each wave object)

  Map map(static_cast<float>(cols), static_cast<float>(rows), tiles);

  Level level{
    std::move(name),
    index,
    std::move(map),
    std::move(paths),
    std::move(waves),
    starting_resources,
    ai_amount,
    ai_interval,
    std::move(available),
    towers,
    enemies,
  };
  return level;
}

// --- Level serialization (for the level editor) ----------------------------

namespace {

std::string esc(std::string const& s) {
  std::string out;
  for (char ch : s) {
    if (ch == '"')
      out += "\\\"";
    else if (ch == '\\')
      out += "\\\\";
    else
      out += ch;
  }
  return out;
}

}  // namespace

std::string save_level_json(Level const& level) {
  std::string s;
  s += "{\n";
  s += "  \"name\": \"" + esc(level.name) + "\",\n";
  s += "  \"index\": " + std::to_string(level.index) + ",\n";
  s += "  \"map\": {\n";
  s += "    \"rows\": " + std::to_string(static_cast<int>(level.map.height())) + ",\n";
  s += "    \"cols\": " + std::to_string(static_cast<int>(level.map.width())) + ",\n";
  s += "    \"terrain\": [\n";
  for (int r = 0; r < static_cast<int>(level.map.height()); ++r) {
    s += "      [";
    for (int c = 0; c < static_cast<int>(level.map.width()); ++c) {
      if (c > 0) s += ", ";
      // Find the tile at (c, r) by position.
      Tile const* tile = level.map.tile_at(Vec2(c + 0.5f, r + 0.5f));
      s += "\"" + (tile ? terrain_name(*tile) : "grass") + "\"";
    }
    s += "]";
    if (r + 1 < static_cast<int>(level.map.height())) s += ",";
    s += "\n";
  }
  s += "    ]\n";
  s += "  },\n";

  // Paths
  s += "  \"paths\": [\n";
  for (std::size_t pi = 0; pi < level.paths.size(); ++pi) {
    auto const& path = level.paths[pi];
    s += "    { \"waypoints\": [";
    auto const& wps = path.waypoints();
    for (std::size_t i = 0; i < wps.size(); ++i) {
      if (i > 0) s += ", ";
      s += "[" + std::to_string(static_cast<int>(wps[i].x)) + "," +
           std::to_string(static_cast<int>(wps[i].y)) + "]";
    }
    s += "], \"portals\": [";
    auto const& portals = path.portal_pairs();
    for (std::size_t qi = 0; qi < portals.size(); ++qi) {
      if (qi > 0) s += ", ";
      s += "[" + std::to_string(portals[qi].first) + "," + std::to_string(portals[qi].second) + "]";
    }
    s += "]";
    s += "}";
    if (pi + 1 < level.paths.size()) s += ",";
    s += "\n";
  }
  s += "  ],\n";

  // Economy
  s += "  \"starting_resources\": " + std::to_string(level.starting_resources) + ",\n";
  s += "  \"resource_auto_inc_amount\": " + std::to_string(level.resource_auto_inc_amount) + ",\n";
  s +=
    "  \"resource_auto_inc_interval\": " + std::to_string(level.resource_auto_inc_interval) + ",\n";

  // Available towers
  s += "  \"available_towers\": [";
  for (std::size_t i = 0; i < level.available_towers.size(); ++i) {
    if (i > 0) s += ", ";
    s += "\"" + level.available_towers[i] + "\"";
  }
  s += "],\n";

  // Waves (each carries its own gap)
  s += "  \"waves\": [\n";
  for (std::size_t wi = 0; wi < level.waves.size(); ++wi) {
    auto const& wave = level.waves[wi];
    s += "    {\"gap\":" + std::to_string(wave.gap) + ", \"spawns\": [";
    for (std::size_t si = 0; si < wave.spawns.size(); ++si) {
      if (si > 0) s += ", ";
      auto const& sp = wave.spawns[si];
      s += "{\"kind\":\"" + sp.type + "\",\"time\":" + std::to_string(sp.time) +
           ",\"path\":" + std::to_string(sp.path) + "}";
    }
    s += "]}";
    if (wi + 1 < level.waves.size()) s += ",";
    s += "\n";
  }
  s += "  ]\n";
  s += "}\n";
  return s;
}

void save_level(std::string const& path, Level const& level) {
  std::ofstream out(path);
  if (!out) throw std::runtime_error("cannot open file for writing: " + path);
  out << save_level_json(level);
}

// --- terrain helpers (shared with the level editor) -------------------------

Tile tile_from_terrain(std::string const& terrain, Vec2 center) {
  float rcf = 1.0f, esf = 1.0f, sbf = 1.0f;
  bool placeable = true, portal = false;
  if (terrain == "grass") {
    // defaults
  } else if (terrain == "fertile") {
    rcf = 0.7f;
  } else if (terrain == "rock") {
    placeable = false;
  } else if (terrain == "ice") {
    esf = 1.5f;
    sbf = 0.5f;
  } else {
    // Unknown terrain: treat as grass.
  }
  return Tile(Rect(center, 1.0f, 1.0f), rcf, placeable, esf, sbf, portal);
}

std::string terrain_name(Tile const& c) {
  if (c.is_portal()) return "portal";
  if (c.enemy_speed_factor() > 1.0f) return "ice";
  if (!c.can_place_tower()) return "rock";
  if (c.resource_cost_factor() < 1.0f) return "fertile";
  return "grass";
}

}  // namespace config
