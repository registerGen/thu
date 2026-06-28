// Player component and player control system.

#pragma once

#include <array>

#include "collision.h"
#include "config.h"
#include "ecs.h"
#include "input.h"
#include "resource.h"

class Player : public Component {
public:
  std::uint8_t id;
  int score;
  int health;
  int level;
  bool protection;  // whether the player is under protection (invincible)
  Entity score_hud;
  Entity health_hud;
  Entity level_hud;
  Entity protection_hud;
  Entity score_bar_hud;
  Entity health_bar_hud;
  Entity level_bar_hud;
  Player(
    std::uint8_t id = 0,
    int score = 0,
    int health = 0,
    int level = 0,
    bool protection = false,
    Entity score_hud = 0,
    Entity health_hud = 0,
    Entity level_hud = 0,
    Entity protection_hud = 0,
    Entity score_bar_hud = 0,
    Entity health_bar_hud = 0,
    Entity level_bar_hud = 0
  )
      : id(id),
        score(score),
        health(health),
        level(level),
        protection(protection),
        score_hud(score_hud),
        health_hud(health_hud),
        level_hud(level_hud),
        protection_hud(protection_hud),
        score_bar_hud(score_bar_hud),
        health_bar_hud(health_bar_hud),
        level_bar_hud(level_bar_hud) { };
};

class PlayerSpawned : public Event {
public:
  Entity& ent;
  uint8_t id;
  int score;
  int health;
  bool protection;
  float speed;
  float size;
  int level;
  PlayerSpawned(
    Entity& ent,
    uint8_t id,
    int score,
    int health,
    bool protection,
    float speed,
    float size,
    int level
  )
      : ent(ent),
        id(id),
        score(score),
        health(health),
        protection(protection),
        speed(speed),
        size(size),
        level(level) { }
};

class PlayerDied : public Event {
public:
  Entity ent;
  uint8_t id;
  int score;
  int health;
  int level;
  float speed;
  float size;
  PlayerDied(Entity ent, uint8_t id, int score, int health, int level, float speed, float size)
      : ent(ent), id(id), score(score), health(health), level(level), speed(speed), size(size) { }
};

class PlayerControlSystem : public System {
private:
  std::shared_ptr<World> world;
  HWND hwnd;
  int win_width, win_height;
  bool use_key;
  int mousex, mousey;
  std::array<bool, 256> keydown;
  SubscriptionId mm_id, kd_id, ku_id;

public:
  PlayerControlSystem(std::shared_ptr<World> world)
      : world(world),
        hwnd(*world->get_ctx<HWND>()),
        win_width(world->get_ctx<WindowConfig>()->width),
        win_height(world->get_ctx<WindowConfig>()->height),
        use_key(false),
        mousex(0),
        mousey(0),
        mm_id(0),
        kd_id(0),
        ku_id(0) {
    std::fill(keydown.begin(), keydown.end(), false);

    disable();
  }

  ~PlayerControlSystem() {
    world->unsub<MouseMoved>(mm_id);
    world->unsub<KeyDown>(kd_id);
    world->unsub<KeyUp>(ku_id);
  }

  void enable() {
    std::tie(mousex, mousey) = InputSystem::cursor_pos(hwnd);
    kd_id = world->sub<KeyDown>([this](const KeyDown& ev) {
      keydown[static_cast<size_t>(ev.keycode)] = true;
    });
    ku_id = world->sub<KeyUp>([this](const KeyUp& ev) {
      keydown[static_cast<size_t>(ev.keycode)] = false;
    });
    mm_id = world->sub<MouseMoved>([this](const MouseMoved& ev) {
      mousex = ev.x;
      mousey = ev.y;
    });
    enabled = true;
  }

  void disable() {
    world->unsub<MouseMoved>(mm_id);
    world->unsub<KeyDown>(kd_id);
    world->unsub<KeyUp>(ku_id);
    enabled = false;
    std::fill(keydown.begin(), keydown.end(), false);
  }

  // Disable mouse control and enable key control.
  void key_ctrl() { use_key = true; }

  // Enable mouse control and disable key control.
  void mouse_ctrl() { use_key = false; }

  void update(float dt) override;
};

void make_player(
  std::shared_ptr<World> world,
  Entity fish_ent,
  std::uint8_t id,
  int score,
  int health,
  int level,
  bool protection,
  float score_prog,
  float health_prog,
  float level_prog,
  const Font& font,
  int fontw,
  int fonth,
  int bar_width,
  int bar_height,
  int score_x,
  int score_y,
  int health_x,
  int health_y,
  int level_x,
  int level_y,
  int protection_x,
  int protection_y,
  int score_bar_x,
  int score_bar_y,
  int health_bar_x,
  int health_bar_y,
  int level_bar_x,
  int level_bar_y
);
