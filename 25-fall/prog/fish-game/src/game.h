// Game system that manages the game life-cycle and cheat codes.

#pragma once

#include <array>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

#include "collision.h"
#include "config.h"
#include "ecs.h"
#include "player.h"
#include "ui.h"

struct FishVisualData {
  std::vector<ImageId> swim, eat, turn, idle;
  int width, height;

  FishVisualData() = default;
  FishVisualData(
    std::vector<ImageId> swim,
    std::vector<ImageId> eat,
    std::vector<ImageId> turn,
    std::vector<ImageId> idle,
    int width,
    int height
  )
      : swim(std::move(swim)),
        eat(std::move(eat)),
        turn(std::move(turn)),
        idle(std::move(idle)),
        width(width),
        height(height) { }
};

struct FontVisualData {
  Font chars;
  int width, height;

  FontVisualData() = default;
  FontVisualData(Font chars, int width, int height)
      : chars(std::move(chars)), width(width), height(height) { }
};

struct GameVisualData {
  std::unordered_map<std::string, FishVisualData> fish_visuals;
  ImageId mine;
  int mine_width, mine_height;
  ImageId star;
  int star_width, star_height;
  FontVisualData hud_font;
  ImageId bg, desc, win, lose;
  int bg_width, bg_height;
  Entity desc_bg_ent, desc_ent, desc_pb_ent, game_bg_ent, win_ent, win_time_ent, lose_ent;

  GameVisualData() = default;
};

struct GameCheatState {
  bool shield = false;
  bool time = false;

  bool cheated() const { return shield || time; }
};

class GamePrepared : public Event {
public:
  std::wstring config;
  GamePrepared(const std::wstring& config) : config(config) { }
};

class GameStarted : public Event { };
class GamePaused : public Event { };
class GameResumed : public Event { };
class GameRestarted : public Event { };
class GameQuit : public Event {
public:
  bool win;
  GameQuit(bool win = false) : win(win) { }
};
class GameFinished : public Event {
public:
  bool win;  // In Multiplayer mode, whether player 0 wins.
  GameFinished(bool win) : win(win) { }
};

class GameSystem : public System {
private:
  std::shared_ptr<World> world;
  HWND hwnd;
  int win_width, win_height;
  bool running;

  Entity timer;

  SubscriptionId ga_id, gs_id, gp_id, gr_id, ge_id, gq_id, gf_id, kd_id, tu_id;

  std::future<std::shared_ptr<GameVisualData>> prepare_future;
  bool preparing;

  void init();
  void cleanup();
  void enable_sys();
  void disable_sys();

  void on_prepared(const std::wstring& config_file);
  void finalize_prepared(std::shared_ptr<GameVisualData> data);
  void on_started();
  void on_paused();
  void on_resumed();
  void on_restarted();
  void on_quit(bool win);
  void on_finished(bool win);

public:
  GameSystem(std::shared_ptr<World> world)
      : world(world),
        hwnd(*world->get_ctx<HWND>()),
        win_width(world->get_ctx<WindowConfig>()->width),
        win_height(world->get_ctx<WindowConfig>()->height),
        running(false),
        timer(0),
        kd_id(0),
        tu_id(0),
        preparing(false) {
    ga_id = world->sub<GamePrepared>([this](const GamePrepared& ev) { on_prepared(ev.config); });
    gs_id = world->sub<GameStarted>([this](const GameStarted&) { on_started(); });
    gp_id = world->sub<GamePaused>([this](const GamePaused&) { on_paused(); });
    gr_id = world->sub<GameResumed>([this](const GameResumed&) { on_resumed(); });
    ge_id = world->sub<GameRestarted>([this](const GameRestarted&) { on_restarted(); });
    gq_id = world->sub<GameQuit>([this](const GameQuit& ev = GameQuit()) { on_quit(ev.win); });
    gf_id = world->sub<GameFinished>([this](const GameFinished& ev) { on_finished(ev.win); });
  }

  ~GameSystem() {
    world->unsub<GamePrepared>(ga_id);
    world->unsub<GameStarted>(gs_id);
    world->unsub<GamePaused>(gp_id);
    world->unsub<GameResumed>(gr_id);
    world->unsub<GameRestarted>(ge_id);
    world->unsub<GameQuit>(gq_id);
    world->unsub<GameFinished>(gf_id);
  }

  bool is_gaming() const { return running; }

  void update(float) override;
};
