// Configuration structures for the game.

#pragma once

#include <windows.h>

#include <array>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "json.hpp"

using json = nlohmann::json;

// Implement JSON serialization for std::wstring.

inline std::string narrow(const std::wstring& w) {
  if (w.empty()) return {};
  int len = WideCharToMultiByte(
    CP_UTF8,
    0,
    w.c_str(),
    static_cast<int>(w.size()),
    nullptr,
    0,
    nullptr,
    nullptr
  );
  if (len <= 0) return {};
  std::string result(static_cast<size_t>(len), '\0');
  WideCharToMultiByte(
    CP_UTF8,
    0,
    w.c_str(),
    static_cast<int>(w.size()),
    result.data(),
    len,
    nullptr,
    nullptr
  );
  return result;
}

inline std::wstring widen(const std::string& s) {
  if (s.empty()) return {};
  int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
  if (len <= 0) return {};
  std::wstring result(static_cast<size_t>(len), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), result.data(), len);
  return result;
}

NLOHMANN_JSON_NAMESPACE_BEGIN

template <>
struct adl_serializer<std::wstring> {
  static void to_json(json& j, const std::wstring& value) { j = narrow(value); }

  static void from_json(const json& j, std::wstring& value) {
    if (j.is_null()) {
      value.clear();
      return;
    }
    value = widen(j.get<std::string>());
  }
};

template <typename T>
struct adl_serializer<std::optional<T>> {
  static void to_json(json& j, const std::optional<T>& opt) {
    if (opt.has_value()) {
      j = *opt;
    } else {
      j = nullptr;
    }
  }

  static void from_json(const json& j, std::optional<T>& opt) {
    if (j.is_null()) {
      opt.reset();
    } else {
      opt = j.get<T>();
    }
  }
};

NLOHMANN_JSON_NAMESPACE_END

// JSON file loading and saving.

template <typename Json>
inline Json load_json(const std::wstring& filename) {
  std::ifstream input(filename);
  if (!input.is_open()) {
    MessageBox(nullptr, std::format(L"Failed to open {}", filename).c_str(), L"Error", MB_OK);
    return Json();
  }

  json data;
  try {
    input >> data;
  } catch (const std::exception& e) {
    MessageBox(
      nullptr,
      std::format(L"Failed to parse {}: {}", filename, widen(std::string(e.what()))).c_str(),
      L"Error",
      MB_OK
    );
    return Json();
  }

  try {
    return data.get<Json>();
  } catch (const std::exception& e) {
    MessageBox(
      nullptr,
      std::format(L"Invalid data in {}: {}", filename, widen(std::string(e.what()))).c_str(),
      L"Error",
      MB_OK
    );
    return Json();
  }
}

template <typename Json>
inline bool save_json(const std::wstring& filename, const Json& j) {
  std::ofstream output(filename);
  if (!output.is_open()) {
    MessageBox(nullptr, std::format(L"Failed to open {}", filename).c_str(), L"Error", MB_OK);
    return false;
  }

  json data = j;
  output << data.dump(2);
  return static_cast<bool>(output);
}

struct WindowConfig {
  int width;
  int height;

  WindowConfig() = default;
  WindowConfig(int width, int height) : width(width), height(height) { }
};

struct FishVisualConfig {
  std::wstring swim, eat, turn, idle;                 // File paths.
  int swim_count, eat_count, turn_count, idle_count;  // Frame counts.
  int width, height;                                  // Frame dimensions.

  FishVisualConfig() = default;
  FishVisualConfig(
    std::wstring swim,
    std::wstring eat,
    std::wstring turn,
    std::wstring idle,
    int swim_count,
    int eat_count,
    int turn_count,
    int idle_count,
    int width,
    int height
  )
      : swim(std::move(swim)),
        eat(std::move(eat)),
        turn(std::move(turn)),
        idle(std::move(idle)),
        swim_count(swim_count),
        eat_count(eat_count),
        turn_count(turn_count),
        idle_count(idle_count),
        width(width),
        height(height) { }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
  FishVisualConfig,
  swim,
  eat,
  turn,
  idle,
  swim_count,
  eat_count,
  turn_count,
  idle_count,
  width,
  height
)

struct FontConfig {
  std::wstring path;          // File path.
  std::vector<char> charset;  // Characters included in the font.
  int width, height;          // Character dimensions.

  FontConfig() = default;
  FontConfig(std::wstring path, std::vector<char> charset, int width, int height)
      : path(std::move(path)), charset(std::move(charset)), width(width), height(height) { }
};

inline void to_json(json& j, const FontConfig& cfg) {
  std::vector<std::string> charset;
  charset.reserve(cfg.charset.size());
  for (char c : cfg.charset) charset.emplace_back(1U, c);

  j = json{
    {"path", cfg.path},
    {"charset", charset},
    {"width", cfg.width},
    {"height", cfg.height},
  };
}

inline void from_json(const json& j, FontConfig& cfg) {
  j.at("path").get_to(cfg.path);
  j.at("width").get_to(cfg.width);
  j.at("height").get_to(cfg.height);

  const auto tokens = j.at("charset").get<std::vector<std::string>>();
  cfg.charset.clear();
  cfg.charset.reserve(tokens.size());
  for (const auto& token : tokens) {
    if (token.size() != 1) {
      throw json::type_error::create(302, "charset entries must be single-character strings", &j);
    }
    cfg.charset.push_back(token[0]);
  }
}

struct VisualConfig {
  std::unordered_map<std::string, FishVisualConfig>
    fish_visuals;       // Map of fish visual configurations, keyed by fish type.
  std::wstring mine;    // Mine image file path.
  std::wstring star;    // Star image file path.
  FontConfig hud_font;  // HUD font configuration.

  VisualConfig() = default;
  VisualConfig(
    std::unordered_map<std::string, FishVisualConfig> fish_visuals,
    std::wstring mine,
    std::wstring star,
    FontConfig hud_font
  )
      : fish_visuals(std::move(fish_visuals)),
        mine(std::move(mine)),
        star(std::move(star)),
        hud_font(std::move(hud_font)) { }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(VisualConfig, fish_visuals, mine, star, hud_font)

enum class GameMode { Invalid = 0, Career, Time, Multiplayer, count };
NLOHMANN_JSON_SERIALIZE_ENUM(
  GameMode,
  {
    {GameMode::Invalid, ""},
    {GameMode::Career, "career"},
    {GameMode::Time, "time"},
    {GameMode::Multiplayer, "multiplayer"},
    {GameMode::count, ""},
  }
)

enum class AIMode { Invalid = 0, Random, Eater, PlayerKiller, count };
NLOHMANN_JSON_SERIALIZE_ENUM(
  AIMode,
  {
    {AIMode::Invalid, ""},
    {AIMode::Random, "random"},
    {AIMode::Eater, "eater"},
    {AIMode::PlayerKiller, "player_killer"},
    {AIMode::count, ""},
  }
)

using AIModeProbArray = std::array<float, std::to_underlying(AIMode::count)>;

struct FishSpawnConfig {
  float spawn_prob;               // Probability of spawning this fish type per frame.
  AIModeProbArray ai_mode_probs;  // AI mode probabilities.
  float min_speed, max_speed;     // Speed multiplier range.
  float size;                     // Size multiplier.
  int level;                      // Fish level.

  FishSpawnConfig() = default;
  FishSpawnConfig(
    float spawn_prob,
    AIModeProbArray ai_mode_probs,
    float min_speed,
    float max_speed,
    float size,
    int level
  )
      : spawn_prob(spawn_prob),
        ai_mode_probs(std::move(ai_mode_probs)),
        min_speed(min_speed),
        max_speed(max_speed),
        size(size),
        level(level) { }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
  FishSpawnConfig,
  spawn_prob,
  ai_mode_probs,
  min_speed,
  max_speed,
  size,
  level
)

struct MineSpawnConfig {
  float spawn_prob;            // Probability of spawning a mine per frame.
  float min_speed, max_speed;  // Speed multiplier range.

  MineSpawnConfig() = default;
  MineSpawnConfig(float spawn_prob, float min_speed, float max_speed)
      : spawn_prob(spawn_prob), min_speed(min_speed), max_speed(max_speed) { }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MineSpawnConfig, spawn_prob, min_speed, max_speed)

struct StarSpawnConfig {
  float spawn_prob;            // Probability of spawning a star per frame.
  float min_speed, max_speed;  // Speed multiplier range.

  StarSpawnConfig() = default;
  StarSpawnConfig(float spawn_prob, float min_speed, float max_speed)
      : spawn_prob(spawn_prob), min_speed(min_speed), max_speed(max_speed) { }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(StarSpawnConfig, spawn_prob, min_speed, max_speed)

struct GameConfig {
  GameMode mode;      // Game mode.
  bool unlocked;      // Whether unlocked.
  std::wstring path;  // The path to this configuration file.
  std::vector<std::wstring>
    will_unlock;  // The configuration files to the levels to unlock upon completion.
  std::wstring
    hidden;  // The configuration file to the hidden level associated, empty string if none.

  int best_min, best_sec;  // Best time.

  std::wstring bg;    // Background image file path.
  std::wstring desc;  // Description image file path.
  std::wstring win;   // Win scene image file path.
  std::wstring lose;  // Lose scene image file path.

  std::unordered_map<std::string, FishSpawnConfig>
    fish_spawns;               // Fish spawn configurations, keyed by fish type.
  MineSpawnConfig mine_spawn;  // Mine spawn configuration.
  StarSpawnConfig star_spawn;  // Star spawn configuration.

  int init_health;                                // Initial health for the player.
  std::optional<std::vector<int>> score_targets;  // Target score for each stage, Career mode only.
  std::optional<int> tl_min, tl_sec;  // Time limit. Time mode and Multiplayer mode only.
  std::optional<int> score_per_lv;  // Target score per level. Time mode and Multiplayer mode only.
  std::optional<int> level_count;   // Number of levels. Time mode and Multiplayer mode only.

  GameConfig() = default;
  GameConfig(
    GameMode mode,
    bool unlocked,
    std::wstring path,
    std::vector<std::wstring> will_unlock,
    std::wstring hidden,
    std::wstring bg,
    std::wstring desc,
    std::wstring win,
    std::wstring lose,
    std::unordered_map<std::string, FishSpawnConfig> fish_spawns,
    MineSpawnConfig mine_spawn,
    StarSpawnConfig star_spawn,
    int init_health,
    std::optional<std::vector<int>> score_targets = std::nullopt,
    std::optional<int> tl_min = std::nullopt,
    std::optional<int> tl_sec = std::nullopt,
    std::optional<int> score_per_lv = std::nullopt,
    std::optional<int> level_count = std::nullopt
  )
      : mode(mode),
        unlocked(unlocked),
        path(std::move(path)),
        will_unlock(std::move(will_unlock)),
        hidden(std::move(hidden)),
        bg(std::move(bg)),
        desc(std::move(desc)),
        win(std::move(win)),
        lose(std::move(lose)),
        fish_spawns(std::move(fish_spawns)),
        mine_spawn(std::move(mine_spawn)),
        star_spawn(std::move(star_spawn)),
        init_health(init_health),
        score_targets(std::move(score_targets)),
        tl_min(tl_min),
        tl_sec(tl_sec),
        score_per_lv(score_per_lv),
        level_count(level_count) { }

  bool try_unlock() {
    unlocked = true;
    return true;
  }

  int target_score() const {
    switch (mode) {
    case GameMode::Invalid:
    case GameMode::count:
      break;
    case GameMode::Career:
      if (score_targets && !score_targets->empty()) return score_targets->back();
      break;
    case GameMode::Time:
    case GameMode::Multiplayer:
      if (score_per_lv && level_count) return (*score_per_lv) * (*level_count);
      break;
    }
    return 0;
  }

  int levels() const {
    switch (mode) {
    case GameMode::Invalid:
    case GameMode::count:
      break;
    case GameMode::Career:
      if (score_targets) return static_cast<int>(score_targets->size());
      break;
    case GameMode::Time:
    case GameMode::Multiplayer:
      if (level_count) return *level_count;
      break;
    }
    return 0;
  }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
  GameConfig,
  mode,
  unlocked,
  path,
  will_unlock,
  hidden,
  best_min,
  best_sec,
  bg,
  desc,
  win,
  lose,
  fish_spawns,
  mine_spawn,
  star_spawn,
  init_health,
  score_targets,
  tl_min,
  tl_sec,
  score_per_lv,
  level_count
)
