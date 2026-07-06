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

// The fixed set of tower / enemy type names (spec-defined). Stats values come
// from JSON; the names are enumerated so we don't need object-key iteration.
const std::vector<std::string>& tower_types() {
  static const std::vector<std::string> t =
    {"normal", "slow", "poison", "splash", "laser", "resource", "wall"};
  return t;
}
const std::vector<std::string>& enemy_types() {
  static const std::vector<std::string> t =
    {"normal", "fast", "armored", "resistant", "splitter", "boss"};
  return t;
}

}  // namespace

TowerStatsTable load_towers(std::string const& path) {
  crjson::parse doc{read_file(path)};
  auto r = doc.root();

  TowerStatsTable table;
  for (auto const& type : tower_types()) {
    TowerStats s;
    try {
      auto t = r[type];
      s.health = integer(t, "health", 100);
      s.cost = integer(t, "cost", 0);
      s.attack_cooldown = num(t, "attack_cooldown", 0.0f);
      s.range = num(t, "range", 0.0f);
      s.targeting = targeting_from(str(t, "targeting", "first"));
      s.resource_amount = integer(t, "resource_amount", 0);
      s.resource_cooldown = num(t, "resource_cooldown", 0.0f);

      // Bullet block (absent for resource/wall towers).
      try {
        auto b = t["bullet"];
        s.min_speed = num(b, "min_speed", 0.0f);
        s.max_speed = num(b, "max_speed", 0.0f);
        s.max_angle_deviation = num(b, "max_angle_deviation", 0.0f);
        s.health_damage = integer(b, "health_damage", 0);
        s.radius = num(b, "radius", 0.0f);
        s.width = num(b, "width", 0.0f);
        try {
          auto bs = b["slow"];
          s.slow_factor = num(bs, "factor", 1.0f);
          s.slow_duration = num(bs, "duration", 0.0f);
        } catch (std::exception&) {
        }
        try {
          auto bp = b["poison"];
          s.poison_damage = integer(bp, "damage", 0);
          s.poison_duration = num(bp, "duration", 0.0f);
        } catch (std::exception&) {
        }
      } catch (std::exception&) {
      }
    } catch (std::exception&) {
      // Type absent from config; skip it.
      continue;
    }
    table[type] = s;
  }
  return table;
}

EnemyStatsTable load_enemies(std::string const& path) {
  crjson::parse doc{read_file(path)};
  auto r = doc.root();

  EnemyStatsTable table;
  for (auto const& type : enemy_types()) {
    EnemyStats s;
    try {
      auto t = r[type];
      s.health = integer(t, "health", 1);
      s.speed = num(t, "speed", 1.0f);
      s.tower_damage = integer(t, "tower_damage", 0);
      s.tower_damage_cooldown = num(t, "tower_damage_cooldown", 0.0f);
      s.size = num(t, "size", 0.5f);
      s.score = integer(t, "score", 10);
      s.slow_resist = num(t, "slow_resist", 1.0f);
      s.splash_resist = num(t, "splash_resist", 1.0f);
      s.shield = integer(t, "shield", 0);
      try {
        auto rg = t["regen"];
        s.regen_amount = num(rg, "amount", 0.0f);
        s.regen_duration = num(rg, "duration", 0.0f);
        s.regen_interval = num(rg, "interval", 0.0f);
      } catch (std::exception&) {
      }
      try {
        auto c = t["child"];
        s.child_count = integer(c, "count", 0);
        s.child_health = integer(c, "health", 1);
        s.child_speed = num(c, "speed", 1.0f);
        s.child_tower_damage = integer(c, "tower_damage", 0);
        s.child_tower_damage_cooldown = num(c, "tower_damage_cooldown", 0.0f);
        s.child_width = num(c, "width", 0.4f);
        s.child_height = num(c, "height", 0.4f);
        s.child_score = integer(c, "score", 5);
        s.child_perturbation = num(c, "perturbation", 0.2f);
      } catch (std::exception&) {
      }
    } catch (std::exception&) {
      continue;
    }
    table[type] = s;
  }
  return table;
}

namespace {

/// Find the index of [col,row] in the path waypoint list, or -1.
int path_index_of(std::vector<std::pair<int, int>> const& path_tiles, int col, int row) {
  for (std::size_t i = 0; i < path_tiles.size(); ++i) {
    if (path_tiles[i].first == col && path_tiles[i].second == row) return static_cast<int>(i);
  }
  return -1;
}

}  // namespace

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

  // --- routes (one or more enemy paths, each with its own portals) --------
  // Builds a LevelRoute from a JSON object that has "path" and "portals".
  auto build_route = [&](crjson::accessor const& route_obj) -> LevelRoute {
    std::vector<std::pair<int, int>> path_tiles;
    std::vector<Vec2> waypoints;
    auto path_arr = route_obj["path"];
    for (std::size_t i = 0; i < path_arr.size(); ++i) {
      int col = static_cast<int>(path_arr[i][0].as_num());
      int row = static_cast<int>(path_arr[i][1].as_num());
      path_tiles.emplace_back(col, row);
      waypoints.push_back(Vec2(col + 0.5f, row + 0.5f));
    }

    Path path(std::move(waypoints));

    // Portals (optional): both tiles are portals visually, but teleportation is
    // forward-only — the earlier portal sends enemies to the later one, which
    // prevents backward-jump loops that would soft-lock the level.
    std::vector<std::pair<Vec2, Vec2>> portal_pairs;
    std::vector<std::pair<Vec2, float>> portal_dest;
    try {
      auto portals = route_obj["portals"];
      for (std::size_t i = 0; i < portals.size(); ++i) {
        auto pair = portals[i];
        int cA = static_cast<int>(pair[0][0].as_num());
        int rA = static_cast<int>(pair[0][1].as_num());
        int cB = static_cast<int>(pair[1][0].as_num());
        int rB = static_cast<int>(pair[1][1].as_num());
        int idxA = path_index_of(path_tiles, cA, rA);
        int idxB = path_index_of(path_tiles, cB, rB);
        if (idxA < 0 || idxB < 0) throw std::runtime_error("portal not on path");
        // Each tile can only be part of one portal pair.
        auto& tileA = tiles[static_cast<std::size_t>(rA) * cols + cA];
        auto& tileB = tiles[static_cast<std::size_t>(rB) * cols + cB];
        if (tileA.is_portal() || tileB.is_portal()) {
          throw std::runtime_error("portal tile already used by another portal pair");
        }
        tileA.set_is_portal(true);
        tileB.set_is_portal(true);
        Vec2 posA(cA + 0.5f, rA + 0.5f);
        Vec2 posB(cB + 0.5f, rB + 0.5f);
        portal_pairs.emplace_back(posA, posB);
        float dA = path.cumulative_at(static_cast<std::size_t>(idxA));
        float dB = path.cumulative_at(static_cast<std::size_t>(idxB));
        if (dB > dA) {
          portal_dest.emplace_back(posA, dB);  // A -> B (forward)
        } else if (dA > dB) {
          portal_dest.emplace_back(posB, dA);  // B -> A (forward)
        }
      }
    } catch (std::exception&) {
      // No portals or malformed; leave empty.
    }

    return LevelRoute{std::move(path), std::move(portal_pairs), std::move(portal_dest)};
  };

  std::vector<LevelRoute> routes;
  try {
    auto routes_arr = r["routes"];
    for (std::size_t i = 0; i < routes_arr.size(); ++i) {
      routes.push_back(build_route(routes_arr[i]));
    }
  } catch (std::exception&) {
    // "routes" absent: fall back to the legacy single-path schema where "path"
    // and "portals" live at the level root.
  }
  if (routes.empty()) {
    routes.push_back(build_route(r));  // r has "path" and "portals"
  }

  // --- waves --------------------------------------------------------------
  auto waves_arr = r["waves"];
  std::vector<Wave> waves;
  for (std::size_t i = 0; i < waves_arr.size(); ++i) {
    Wave wave;
    auto spawns = waves_arr[i]["spawns"];
    for (std::size_t j = 0; j < spawns.size(); ++j) {
      int route = static_cast<int>(spawns[j]["route"].as_num());  // mandatory
      if (route < 0 || route >= static_cast<int>(routes.size())) {
        throw std::runtime_error(
          "level '" + name + "': wave " + std::to_string(i) + " spawn " + std::to_string(j) +
          " references unknown route " + std::to_string(route)
        );
      }
      wave.spawns.push_back(
        EnemySpawn{
          std::string(spawns[j]["type"].as_str()),
          static_cast<float>(spawns[j]["time"].as_num()),
          route,
        }
      );
    }
    waves.push_back(std::move(wave));
  }

  // --- economy / available towers ----------------------------------------
  int starting_resources = integer(r, "starting_resources", 100);
  float ai_amount = num(r["auto_increase"], "amount", 0.0f);
  float ai_interval = num(r["auto_increase"], "interval", 1.0f);
  std::vector<std::string> available;
  try {
    auto at = r["available_towers"];
    for (std::size_t i = 0; i < at.size(); ++i) available.emplace_back(at[i].as_str());
  } catch (std::exception&) {
  }

  // --- per-wave pre-delays (gaps) -----------------------------------------
  // One entry per wave, no defaults: gaps[0] from game start to wave 0,
  // gaps[i] from wave i-1's last spawn to wave i. Length must match waves.
  std::vector<float> gaps;
  auto gaps_arr = r["gaps"];
  for (std::size_t i = 0; i < gaps_arr.size(); ++i) {
    gaps.push_back(static_cast<float>(gaps_arr[i].as_num()));
  }
  if (gaps.size() != waves.size()) {
    throw std::runtime_error(
      "level '" + name + "': gaps must have exactly one entry per wave (got " +
      std::to_string(gaps.size()) + ", expected " + std::to_string(waves.size()) + ")"
    );
  }

  Map map(static_cast<float>(cols), static_cast<float>(rows), tiles);

  Level level{
    std::move(name),
    index,
    std::move(map),
    std::move(routes),
    std::move(waves),
    std::move(gaps),
    starting_resources,
    Resource::AutoIncrease(static_cast<int>(ai_amount), ai_interval),
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

  // Routes
  s += "  \"routes\": [\n";
  for (std::size_t ri = 0; ri < level.routes.size(); ++ri) {
    auto const& route = level.routes[ri];
    s += "    { \"path\": [";
    auto const& wps = route.path.waypoints();
    for (std::size_t i = 0; i < wps.size(); ++i) {
      if (i > 0) s += ", ";
      s += "[" + std::to_string(static_cast<int>(wps[i].x)) + "," +
           std::to_string(static_cast<int>(wps[i].y)) + "]";
    }
    s += "], \"portals\": [";
    for (std::size_t pi = 0; pi < route.portal_pairs.size(); ++pi) {
      if (pi > 0) s += ", ";
      auto const& [pa, pb] = route.portal_pairs[pi];
      s += "[[" + std::to_string(static_cast<int>(pa.x)) + "," +
           std::to_string(static_cast<int>(pa.y)) + "],[" + std::to_string(static_cast<int>(pb.x)) +
           "," + std::to_string(static_cast<int>(pb.y)) + "]]";
    }
    s += "]";
    s += "}";
    if (ri + 1 < level.routes.size()) s += ",";
    s += "\n";
  }
  s += "  ],\n";

  // Economy
  s += "  \"starting_resources\": " + std::to_string(level.starting_resources) + ",\n";
  s += "  \"auto_increase\": { \"amount\": " + std::to_string(level.auto_increase.amount) +
       ", \"interval\": " + std::to_string(level.auto_increase.duration()) + " },\n";

  // Available towers
  s += "  \"available_towers\": [";
  for (std::size_t i = 0; i < level.available_towers.size(); ++i) {
    if (i > 0) s += ", ";
    s += "\"" + level.available_towers[i] + "\"";
  }
  s += "],\n";

  // Gaps
  s += "  \"gaps\": [";
  for (std::size_t i = 0; i < level.gaps.size(); ++i) {
    if (i > 0) s += ", ";
    s += std::to_string(level.gaps[i]);
  }
  s += "],\n";

  // Waves
  s += "  \"waves\": [\n";
  for (std::size_t wi = 0; wi < level.waves.size(); ++wi) {
    auto const& wave = level.waves[wi];
    s += "    { \"spawns\": [";
    for (std::size_t si = 0; si < wave.spawns.size(); ++si) {
      if (si > 0) s += ", ";
      auto const& sp = wave.spawns[si];
      s += "{\"type\":\"" + sp.type + "\",\"time\":" + std::to_string(sp.time) +
           ",\"route\":" + std::to_string(sp.route) + "}";
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
