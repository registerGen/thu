#ifdef TD_USE_RUST_MODEL

# include "RustGameModel.h"

# include <utility>

namespace {

// --- cxx `td::ffi::*` -> app/ view POD (self-contained Vec2/views from
// Views.h, pulled in via RustGameModel.h -> GameModel.h -> Views.h) ---

Vec2 to_vec2(td::ffi::Vec2 v) { return Vec2(v.x, v.y); }

TowerView to_tower_view(td::ffi::TowerView const& v) {
  return {to_vec2(v.pos), to_vec2(v.aim), std::string(v.kind), v.health, v.max_health};
}
EnemyView to_enemy_view(td::ffi::EnemyView const& v) {
  return {
    to_vec2(v.pos),
    v.half_width,
    v.half_height,
    std::string(v.kind),
    v.health,
    v.max_health,
    StatusFlags{v.status_hint.slow, v.status_hint.poison, v.status_hint.regen}
  };
}
BulletView to_bullet_view(td::ffi::BulletView const& v) {
  return {to_vec2(v.pos), to_vec2(v.vel), std::string(v.kind), v.radius};
}
TileView to_tile_view(td::ffi::TileView const& v) { return {std::string(v.terrain)}; }
LevelInfoView to_info_view(td::ffi::LevelInfoView const& v) {
  return {v.index, std::string(v.name)};
}
TowerCostView to_cost_view(td::ffi::TowerCostView const& v) {
  return {std::string(v.kind), v.cost};
}
GameResultView to_result_view(td::ffi::GameResultView const& v) {
  return {v.cleared, v.cheated, v.time, v.score};
}
GameEventView to_event_view(td::ffi::GameEventView const& v) {
  return {std::string(v.kind), to_vec2(v.pos), v.a, std::string(v.type_tag), v.has_boss, v.is_last};
}

GameState to_game_state(td::ffi::GameState s) {
  switch (s) {
  case td::ffi::GameState::Victory:
    return GameState::Victory;
  case td::ffi::GameState::Defeat:
    return GameState::Defeat;
  default:
    return GameState::Playing;
  }
}

td::ffi::Vec2 from_vec2(Vec2 v) { return td::ffi::Vec2{v.x, v.y}; }

std::vector<LevelInfoView> to_infos(rust::Vec<td::ffi::LevelInfoView> const& v) {
  std::vector<LevelInfoView> out;
  for (auto const& i : v) out.push_back(to_info_view(i));
  return out;
}

LevelView to_level_view(td::ffi::LevelView const& v) {
  LevelView out;
  out.index = v.index;
  out.name = std::string(v.name);
  out.map_width = v.map_width;
  out.map_height = v.map_height;
  for (auto const& t : v.terrain) out.terrain.push_back(to_tile_view(t));
  for (auto const& path : v.path_waypoints) {
    std::vector<Vec2> wps;
    for (auto const& w : path.waypoints) wps.push_back(to_vec2(w));
    out.path_waypoints.push_back(std::move(wps));
  }
  for (auto const& t : v.available_towers) out.available_towers.push_back(std::string(t));
  for (auto const& c : v.tower_costs) out.tower_costs.push_back(to_cost_view(c));
  return out;
}

}  // namespace

std::unique_ptr<RustGameModel> RustGameModel::create(std::string config_dir) {
  auto registry = td::ffi::new_registry(rust::String(std::move(config_dir)));
  auto game = td::ffi::new_game(*registry);
  return std::unique_ptr<RustGameModel>(new RustGameModel(std::move(registry), std::move(game)));
}

RustGameModel::RustGameModel(
  rust::Box<td::ffi::LevelRegistry> registry,
  rust::Box<td::ffi::Game> game
)
    : registry_(std::move(registry)), game_(std::move(game)) { }

bool RustGameModel::tick(float dt) { return td::ffi::tick(*game_, dt); }
void RustGameModel::restart() { td::ffi::restart(*game_); }
void RustGameModel::pause() { td::ffi::pause(*game_); }
void RustGameModel::resume() { td::ffi::resume(*game_); }
void RustGameModel::apply_cheat(std::string code) {
  td::ffi::apply_cheat(*game_, rust::String(std::move(code)));
}

bool RustGameModel::start_level_json(std::string json) {
  try {
    td::ffi::start_level_json(*game_, rust::String(std::move(json)));
    return true;
  } catch (...) {
    return false;
  }
}

bool RustGameModel::advance_level() {
  if (!td::ffi::has_next_official(*registry_)) return false;
  td::ffi::advance_level(*game_, *registry_);
  return true;
}

void RustGameModel::select_level(int index) { td::ffi::select_level(*game_, *registry_, index); }

int RustGameModel::place_tower(std::string type, Vec2 pos) {
  return td::ffi::place_tower(*game_, rust::String(std::move(type)), from_vec2(pos));
}

bool RustGameModel::can_place_at(Vec2 pos) const {
  return td::ffi::can_place_at(*game_, from_vec2(pos));
}

GameState RustGameModel::state() const { return to_game_state(td::ffi::state(*game_)); }
bool RustGameModel::paused() const { return td::ffi::paused(*game_); }
bool RustGameModel::over() const { return state() != GameState::Playing; }
int RustGameModel::score() const { return td::ffi::score(*game_); }
float RustGameModel::elapsed_time() const { return td::ffi::elapsed_time(*game_); }
int RustGameModel::current_wave() const { return td::ffi::current_wave(*game_); }
int RustGameModel::resource_amount() const { return td::ffi::resource_amount(*game_); }
int RustGameModel::level_index() const { return td::ffi::level_index(*game_); }
std::string RustGameModel::level_name() const { return std::string(td::ffi::level_name(*game_)); }
float RustGameModel::map_width() const { return td::ffi::map_width(*game_); }
float RustGameModel::map_height() const { return td::ffi::map_height(*game_); }
bool RustGameModel::has_next_level() const { return td::ffi::has_next_official(*registry_); }

std::vector<TowerView> RustGameModel::tower_views() const {
  std::vector<TowerView> out;
  for (auto const& v : td::ffi::tower_views(*game_)) out.push_back(to_tower_view(v));
  return out;
}
std::vector<EnemyView> RustGameModel::enemy_views() const {
  std::vector<EnemyView> out;
  for (auto const& v : td::ffi::enemy_views(*game_)) out.push_back(to_enemy_view(v));
  return out;
}
std::vector<BulletView> RustGameModel::bullet_views() const {
  std::vector<BulletView> out;
  for (auto const& v : td::ffi::bullet_views(*game_)) out.push_back(to_bullet_view(v));
  return out;
}
LevelView RustGameModel::level_view() const { return to_level_view(td::ffi::level_view(*game_)); }
GameResultView RustGameModel::last_result_view() const {
  return to_result_view(td::ffi::last_result_view(*game_));
}
std::vector<GameEventView> RustGameModel::take_events() {
  std::vector<GameEventView> out;
  for (auto const& v : td::ffi::take_events(*game_)) out.push_back(to_event_view(v));
  return out;
}

int RustGameModel::current_level_index() const { return td::ffi::current_level_index(*registry_); }
std::size_t RustGameModel::level_count() const { return td::ffi::registry_size(*registry_); }
std::vector<LevelInfoView> RustGameModel::official_infos() const {
  return to_infos(td::ffi::official_infos(*registry_));
}
std::vector<LevelInfoView> RustGameModel::infos() const {
  return to_infos(td::ffi::registry_infos(*registry_));
}

#endif  // TD_USE_RUST_MODEL
