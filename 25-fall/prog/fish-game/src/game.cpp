#include "game.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <ranges>
#include <regex>

#include "ai.h"
#include "collision.h"
#include "health.h"
#include "movement.h"
#include "player.h"
#include "role.h"
#include "scene.h"
#include "score.h"
#include "spawn.h"
#include "timer.h"

// Initialize the game: create player, background, HUD, and set up input system and cheat codes.
void GameSystem::init() {
  auto config = world->get_ctx<GameConfig>();
  auto data = world->get_ctx<GameVisualData>();

  // Movable background only in non-multiplayer modes.
  if (config->mode != GameMode::Multiplayer)
    make_background_image(world, data->game_bg_ent, data->bg_width, data->bg_height);
  else
    world->remove<BackgroundImage>(data->game_bg_ent);

  Entity player0, player1 = 0;

  switch (config->mode) {
  case GameMode::Invalid:
  case GameMode::count:
    break;

  case GameMode::Career:
  case GameMode::Time:
    // Must create immediately to ensure the fish is properly shown.
    world->emit_now(PlayerSpawned(player0, 0, 0, config->init_health, false, 1.0f, 1.0f, 1));

    world->sys<PlayerControlSystem>()->mouse_ctrl();

    break;

  case GameMode::Multiplayer:
    world->emit_now(PlayerSpawned(player0, 0, 0, config->init_health, false, 1.0f, 1.0f, 1));
    world->emit_now(PlayerSpawned(player1, 1, 0, config->init_health, false, 1.0f, 1.0f, 1));

    world->sys<PlayerControlSystem>()->key_ctrl();

    break;
  }

  switch (config->mode) {
  case GameMode::Invalid:
  case GameMode::count:
    break;

  case GameMode::Career: {
    timer = make_countup_timer(
      world,
      0,
      0,
      true,
      data->hud_font.chars,
      data->hud_font.width,
      data->hud_font.height,
      win_width / 2,
      60,
      zindex::UI,
      Alignment::Center,
      1.5f
    );
    break;
  }

  case GameMode::Time:
  case GameMode::Multiplayer: {
    int min0 = config->tl_min.value();
    int sec0 = config->tl_sec.value();
    timer = make_countdown_timer(
      world,
      min0,
      sec0,
      true,
      true,
      data->hud_font.chars,
      data->hud_font.width,
      data->hud_font.height,
      win_width / 2,
      60,
      zindex::UI,
      Alignment::Center,
      1.5f
    );

    tu_id = world->sub<TimeUp>([this, config, player0, player1](const TimeUp& ev) {
      if (ev.ent != timer) return false;

      int target = config->target_score();

      // In Time mode, player wins if reaching target score.
      if (config->mode == GameMode::Time) {
        auto p0 = world->get<Player>(player0);

        if (!p0)  // Player already destroyed (dead but not yet re-spawned).
          world->emit(GameFinished(false));
        else
          world->emit(GameFinished(p0->score >= target));
      }
      // In Multiplayer mode, player wins if having higher score.
      else {
        auto p0 = world->get<Player>(player0);
        auto p1 = world->get<Player>(player1);
        if (!p0 && !p1)  // Both players destroyed (dead but not yet re-spawned), rematch.
          world->get<Timer>(timer)->reset();
        else if (!p0)
          world->emit(GameFinished(false));
        else if (!p1)
          world->emit(GameFinished(true));
        else {
          if (p0->score != p1->score)
            world->emit(GameFinished(p0->score > p1->score));
          else  // Rematch if tie.
            world->get<Timer>(timer)->reset();
        }
      }

      return true;
    });

    break;
  }
  }

  world->add<Shown>(timer);

  kd_id = world->sub<KeyDown>([this](const KeyDown& ev) {
    if (ev.keycode == keycodes::ESC) world->emit(GamePaused());
    if (ev.keycode == keycodes::SPACE) world->emit(GameResumed());
    if (ev.keycode == keycodes::R) world->emit(GameRestarted());
    if (ev.keycode == keycodes::Q) world->emit(GameQuit());

    auto cheats = world->get_ctx<GameCheatState>();

    // Cheat code shield: enable protection.
    if (world->sys<InputSystem>()->current_input().ends_with("SHIELD")) {
      world->for_each<Player>([this, cheats](Entity, std::shared_ptr<Player> player) {
        cheats->shield = true;
        player->protection = true;
        world->add<Shown>(player->protection_hud);
      });
    }

    // Cheat code time: reset timer.
    if (world->sys<InputSystem>()->current_input().ends_with("TIME")) {
      cheats->time = true;
      world->get<Timer>(timer)->reset();
    }
  });
}

// Clean up the game: destroy all entities and reset systems.
void GameSystem::cleanup() {
  if (kd_id) {
    world->unsub<KeyDown>(kd_id);
    kd_id = 0;
  }
  if (tu_id) {
    world->unsub<TimeUp>(tu_id);
    tu_id = 0;
  }

  auto movables = world->get<Movable>();
  std::ranges::for_each(
    movables,
    [this](const std::tuple<Entity, std::shared_ptr<Movable>> ent_comp) {
      Entity ent = std::get<0>(ent_comp);
      if (world->has<Fish>(ent))
        destroy_fish(world, ent);
      else if (!world->has<BackgroundImage>(ent))
        world->destroy(ent);
    }
  );

  auto flashes = world->get<Flash>();
  std::ranges::for_each(flashes, [this](const std::tuple<Entity, std::shared_ptr<Flash>> ent_comp) {
    Entity ent = std::get<0>(ent_comp);
    world->destroy(ent);
  });

  auto timers = world->get<Timer>();
  std::ranges::for_each(timers, [this](const std::tuple<Entity, std::shared_ptr<Timer>> ent_comp) {
    Entity ent = std::get<0>(ent_comp);
    world->destroy(ent);
  });

  timer = 0;
}

void GameSystem::enable_sys() {
  world->sys<TimerSystem>()->enable();
  world->sys<SpawnSystem>()->enable();
  world->sys<MovementSystem>()->enable();
  world->sys<PlayerControlSystem>()->enable();
  world->sys<AISystem>()->enable();
  world->sys<CollisionSystem>()->enable();
  world->sys<ScoreSystem>()->enable();
  world->sys<HealthSystem>()->enable();
}

void GameSystem::disable_sys() {
  world->sys<TimerSystem>()->disable();
  world->sys<SpawnSystem>()->disable();
  world->sys<MovementSystem>()->disable();
  world->sys<PlayerControlSystem>()->disable();
  world->sys<AISystem>()->disable();
  world->sys<CollisionSystem>()->disable();
  world->sys<ScoreSystem>()->disable();
  world->sys<HealthSystem>()->disable();
}

// Prepare the game part 1: load resources.
void GameSystem::on_prepared(const std::wstring& config_file) {
  if (preparing) return;

  world->add_ctx<GameConfig>(load_json<GameConfig>(config_file));

  auto config = world->get_ctx<GameConfig>();
  if (config->unlocked == false) {
    world->emit(SceneChanged(Scene::LevelLocked));
    return;
  }

  world->emit(SceneChanged(Scene::Loading));

  // Load assets asynchronously.
  preparing = true;
  prepare_future = std::async(std::launch::async, [this, config_file]() {
    auto config = world->get_ctx<GameConfig>();
    auto data = std::make_shared<GameVisualData>();
    auto visuals = world->get_ctx<VisualConfig>();
    auto bm = world->get_ctx<BitmapManager>();

    data->bg = bm->load(config->bg);
    data->desc = bm->load(config->desc);
    data->win = bm->load(config->win);
    data->lose = bm->load(config->lose);
    auto bg_dim = bm->get(data->bg);
    data->bg_width = std::get<1>(bg_dim);
    data->bg_height = std::get<2>(bg_dim);

    auto fish_visuals = visuals->fish_visuals;
    auto load_frames = [&bm](const std::wstring& prefix, int count) {
      return std::views::iota(1, count + 1) | std::views::transform([&bm, &prefix](int i) {
               return bm->load(std::format(L"{}{}.bmp", prefix, i));
             }) |
             std::ranges::to<std::vector>();
    };
    for (const auto& [name, vis] : fish_visuals) {
      data->fish_visuals[name] = FishVisualData(
        load_frames(vis.swim, vis.swim_count),
        load_frames(vis.eat, vis.eat_count),
        load_frames(vis.turn, vis.turn_count),
        load_frames(vis.idle, vis.idle_count),
        vis.width,
        vis.height
      );
    }

    data->mine = bm->load(visuals->mine);
    auto mine_dim = bm->get(data->mine);
    data->mine_width = std::get<1>(mine_dim);
    data->mine_height = std::get<2>(mine_dim);

    data->star = bm->load(visuals->star);
    auto star_dim = bm->get(data->star);
    data->star_width = std::get<1>(star_dim);
    data->star_height = std::get<2>(star_dim);

    auto hud_font = visuals->hud_font;
    data->hud_font.width = hud_font.width;
    data->hud_font.height = hud_font.height;
    for (auto c : hud_font.charset) {
      data->hud_font.chars[c] =
        bm->load(std::format(L"{}{}.bmp", hud_font.path, static_cast<int>(c)));
    }

    return data;
  });
}

// Prepare the game part 2: set up entities and scenes with loaded resources.
void GameSystem::finalize_prepared(std::shared_ptr<GameVisualData> data) {
  if (!data) return;

  world->add_ctx<GameVisualData>(*data);
  data = world->get_ctx<GameVisualData>();
  world->add_ctx<GameCheatState>();

  auto config = world->get_ctx<GameConfig>();
  auto pb_text = config->best_min == -1 || config->best_sec == -1
                   ? "--:--"
                   : std::format("{:02}:{:02}", config->best_min, config->best_sec);
  pb_text = "PB  " + pb_text;

  data->desc_bg_ent = make_image(world, data->bg, win_width / 2, win_height / 2, zindex::BG);
  data->desc_ent = make_image(world, data->desc, win_width / 2, 300, zindex::UI);
  if (config->mode != GameMode::Multiplayer)
    data->desc_pb_ent = make_text(
      world,
      pb_text,
      data->hud_font.chars,
      data->hud_font.width,
      data->hud_font.height,
      win_width / 2,
      450,
      zindex::UI,
      Alignment::Center
    );
  else
    data->desc_pb_ent = 0;

  data->game_bg_ent = make_image(world, data->bg, win_width / 2, win_height / 2, zindex::BG);

  data->win_ent = make_image(world, data->win, win_width / 2, 300, zindex::UI);
  data->lose_ent = make_image(world, data->lose, win_width / 2, 300, zindex::UI);

  auto scene = world->sys<SceneSystem>();
  scene->reg(Scene::LevelDesc, data->desc_bg_ent);
  scene->reg(Scene::LevelDesc, data->desc_ent);
  if (config->mode != GameMode::Multiplayer) scene->reg(Scene::LevelDesc, data->desc_pb_ent);
  scene->reg(Scene::Game, data->game_bg_ent);
  scene->reg(Scene::GameWin, data->win_ent);
  scene->reg(Scene::GameLose, data->lose_ent);

  world->emit(SceneChanged(Scene::LevelDesc));
}

// Start the game: initialize entities and enable systems.
void GameSystem::on_started() {
  if (running) return;

  InputSystem::clip_cursor(hwnd);

  enable_sys();
  init();

  world->emit(SceneChanged(Scene::Game));

  running = true;
}

// Pause the game: disable systems and show pause overlay.
void GameSystem::on_paused() {
  if (!running) return;

  disable_sys();

  world->emit(SceneOverlaid(Scene::GamePause));

  InputSystem::clip_cursor(nullptr);

  running = false;
}

// Resume the game: enable systems and hide pause overlay.
void GameSystem::on_resumed() {
  if (running) return;

  InputSystem::clip_cursor(hwnd);

  world->emit(SceneHidden(Scene::GamePause));

  enable_sys();

  running = true;
}

// Restart the game: clean up and re-initialize entities and systems.
void GameSystem::on_restarted() {
  if (running) return;

  InputSystem::clip_cursor(hwnd);

  cleanup();

  enable_sys();
  init();

  world->emit(SceneHidden(Scene::GamePause));

  running = true;
}

// Quit the game: clean up resources and return to start menu.
void GameSystem::on_quit(bool win) {
  if (running) return;

  cleanup();
  disable_sys();

  auto config = world->get_ctx<GameConfig>();
  auto data = world->get_ctx<GameVisualData>();

  auto bm = world->get_ctx<BitmapManager>();
  bm->unload(data->bg);
  bm->unload(data->desc);
  for (const auto& [name, vis] : data->fish_visuals) {
    for (auto id : vis.swim) bm->unload(id);
    for (auto id : vis.eat) bm->unload(id);
    for (auto id : vis.turn) bm->unload(id);
    for (auto id : vis.idle) bm->unload(id);
  }
  bm->unload(data->mine);
  bm->unload(data->star);
  for (const auto& [_, id] : data->hud_font.chars) bm->unload(id);

  InputSystem::clip_cursor(nullptr);

  world->emit(SceneHidden(Scene::GamePause));
  world->emit(SceneChanged(Scene::StartMenu));

  auto scene = world->sys<SceneSystem>();
  scene->unreg(Scene::LevelDesc, data->desc_bg_ent);
  scene->unreg(Scene::LevelDesc, data->desc_ent);
  if (config->mode != GameMode::Multiplayer) scene->unreg(Scene::LevelDesc, data->desc_pb_ent);
  scene->unreg(Scene::Game, data->game_bg_ent);
  scene->unreg(Scene::GameWin, data->win_ent);
  if (data->win_time_ent) scene->unreg(Scene::GameWin, data->win_time_ent);
  scene->unreg(Scene::GameLose, data->lose_ent);

  std::wstring hidden = config->hidden;
  world->remove_ctx<GameConfig>();
  world->remove_ctx<GameVisualData>();
  bool cheated = world->get_ctx<GameCheatState>()->cheated();
  world->remove_ctx<GameCheatState>();

  // Invoke hidden level.
  if (win && !cheated && !hidden.empty()) world->emit(GamePrepared(hidden));
}

// Finish the game: clean up and show win/lose Scene.
void GameSystem::on_finished(bool win) {
  if (!running) return;

  if (win && !world->get_ctx<GameCheatState>()->cheated()) {
    auto config = world->get_ctx<GameConfig>();

    if (config->mode != GameMode::Multiplayer) {
      // Show game time.
      auto time = world->get<Timer>(timer);
      auto to_sec = [](int m, int s) { return m * 60 + s; };
      int cur = std::abs(to_sec(time->min, time->sec) - to_sec(time->min0, time->sec0));

      auto data = world->get_ctx<GameVisualData>();
      data->win_time_ent = make_text(
        world,
        std::format("TIME  {:02}:{:02}", cur / 60, cur % 60),
        data->hud_font.chars,
        data->hud_font.width,
        data->hud_font.height,
        win_width / 2,
        450,
        zindex::UI,
        Alignment::Center
      );
      world->sys<SceneSystem>()->reg(Scene::GameWin, data->win_time_ent);

      // Update best time.
      int best = to_sec(config->best_min, config->best_sec);

      if (config->best_min == -1 || config->best_sec == -1 || cur < best) {
        config->best_min = cur / 60;
        config->best_sec = cur % 60;

        std::ifstream in(config->path);

        std::ostringstream oss;
        oss << in.rdbuf();
        std::string text = oss.str();
        in.close();

        text = std::regex_replace(
          text,
          std::regex(R"("best_min"\s*:\s*-?\d+)"),
          "\"best_min\": " + std::to_string(config->best_min)
        );
        text = std::regex_replace(
          text,
          std::regex(R"("best_sec"\s*:\s*-?\d+)"),
          "\"best_sec\": " + std::to_string(config->best_sec)
        );

        std::ofstream out(config->path, std::ios::trunc);
        out << text;
      }
    } else
      world->get_ctx<GameVisualData>()->win_time_ent = 0;

    // Unlock next levels.
    for (const auto& level : config->will_unlock) {
      std::ifstream in(level);

      std::ostringstream oss;
      oss << in.rdbuf();
      std::string text = oss.str();
      in.close();

      text =
        std::regex_replace(text, std::regex(R"("unlocked"\s*:\s*false)"), "\"unlocked\": true");

      std::ofstream out(level, std::ios::trunc);
      out << text;
    }
  }

  cleanup();
  disable_sys();

  InputSystem::clip_cursor(nullptr);

  world->emit(SceneChanged(win ? Scene::GameWin : Scene::GameLose));

  running = false;
}

void GameSystem::update(float) {
  if (!preparing || !prepare_future.valid()) return;

  // Go to part 2 when finishing loading.
  if (prepare_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
    finalize_prepared(prepare_future.get());
    preparing = false;
  }
}
