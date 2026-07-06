#pragma once

#ifdef TD_USE_RUST_MODEL

# include <cstddef>
# include <memory>
# include <string>
# include <vector>

# include "GameModel.h"
# include "td_game_rs_cxxbridge/cxxbridge.h"  // cxx-generated: td::ffi::*

/// `GameModel` backed by the Rust core via the cxx bridge. Owns
/// `rust::Box<td::ffi::Game>` + `rust::Box<td::ffi::LevelRegistry>` and calls
/// the `td::ffi::*` free functions, converting the cxx view types to the
/// `game/views.h` POD structs at the boundary. Compiled only when
/// `TD_USE_RUST_MODEL` is ON.
class RustGameModel : public GameModel {
public:
  /// Factory: builds the registry + game in the correct order, then constructs
  /// the object. Use this instead of a public constructor — `rust::Box` has no
  /// default ctor, so a constructor that inits `game_` from `*registry_` would
  /// be fragile w.r.t. member declaration order. The factory passes both boxes
  /// as params (both valid at init time, regardless of member order).
  static std::unique_ptr<RustGameModel> create(std::string config_dir);

  bool tick(float dt) override;
  void restart() override;
  void pause() override;
  void resume() override;
  void apply_cheat(std::string code) override;
  bool start_level_json(std::string json) override;
  bool advance_level() override;
  void select_level(int index) override;
  int place_tower(std::string type, Vec2 pos) override;
  bool can_place_at(Vec2 pos) const override;
  GameState state() const override;
  bool paused() const override;
  bool over() const override;
  int score() const override;
  float elapsed_time() const override;
  int current_wave() const override;
  int resource_amount() const override;
  int level_index() const override;
  std::string level_name() const override;
  float map_width() const override;
  float map_height() const override;
  bool has_next_level() const override;
  std::vector<TowerView> tower_views() const override;
  std::vector<EnemyView> enemy_views() const override;
  std::vector<BulletView> bullet_views() const override;
  LevelView level_view() const override;
  GameResultView last_result_view() const override;
  std::vector<GameEventView> take_events() override;
  int current_level_index() const override;
  std::size_t level_count() const override;
  std::vector<LevelInfoView> official_infos() const override;
  std::vector<LevelInfoView> infos() const override;

private:
  RustGameModel(rust::Box<td::ffi::LevelRegistry> registry, rust::Box<td::ffi::Game> game);
  rust::Box<td::ffi::LevelRegistry> registry_;
  rust::Box<td::ffi::Game> game_;
};

#endif  // TD_USE_RUST_MODEL
