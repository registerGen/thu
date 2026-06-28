// Fishes and other roles components and systems.

#pragma once

#include <array>

#include "ecs.h"
#include "ui.h"

class Velocity : public Component {
public:
  float x;
  float y;
  Velocity(float x = 0.0f, float y = 0.0f) : x(x), y(y) { }
};

class Collidable : public Component {
public:
  int width;
  int height;
  Collidable(int width = 0, int height = 0) : width(width), height(height) { }
};

class Movable : public Component { };

// Movable background image

class BackgroundImage : public Component {
public:
  int width;
  int height;
  BackgroundImage(int width = 0, int height = 0) : width(width), height(height) { }
};

void make_background_image(std::shared_ptr<World> world, Entity ent, int width, int height);

// Fish

enum class FishState : uint8_t { Invalid = 0, Swim, Eat, Turn, Idle, count };
enum class FishDirection : bool { Left = false, Right = true };

class Fish : public Component {
public:
  float speed;  // speed multiplier
  float size;   // size multiplier
  int level;    // level of the fish
  FishState state;
  FishState next_state;  // The "queued" state to display.
  FishDirection dir;
  std::array<Entity, std::to_underlying(FishState::count)> anims;
  Entity cur_anim;

  Fish(
    float speed = 0.0f,
    float size = 1.0f,
    int level = 0,
    FishState state = FishState::Invalid,
    FishDirection dir = FishDirection::Left,
    Entity swim_ent = 0,
    Entity eat_ent = 0,
    Entity turn_ent = 0,
    Entity idle_ent = 0
  )
      : speed(speed),
        size(size),
        level(level),
        state(state),
        next_state(FishState::Invalid),
        dir(dir),
        anims({0, swim_ent, eat_ent, turn_ent, idle_ent}),
        cur_anim(0) { }
};

class FishStateChanged : public Event {
public:
  Entity ent;
  FishState state;
  FishStateChanged(Entity ent, FishState state) : ent(ent), state(state) { }
};

class FishTurned : public Event {
public:
  Entity ent;
  FishTurned(Entity ent) : ent(ent) { }
};

class FishSizeUp : public Event {
public:
  Entity ent;
  float mult;
  FishSizeUp(Entity ent, float mult) : ent(ent), mult(mult) { }
};

// Specially-designed fish rendering system that handles fish visual appearances
// (animation, size-up events, etc.).
class FishRenderSystem : public System {
private:
  std::shared_ptr<World> world;
  SubscriptionId fsc_id, ft_id, ad_id, fsu_id;

  void change(std::shared_ptr<Fish> fish, FishState state);
  void on_anim_done(Entity ent);
  void on_state_changed(Entity ent, FishState state);
  void on_turned(Entity ent);
  void on_size_up(Entity ent, float mult);

public:
  FishRenderSystem(std::shared_ptr<World> world) : world(world) {
    ad_id = world->sub<AnimationDone>([this](const AnimationDone& ev) { on_anim_done(ev.ent); });
    fsc_id = world->sub<FishStateChanged>([this](const FishStateChanged& ev) {
      on_state_changed(ev.ent, ev.state);
    });
    ft_id = world->sub<FishTurned>([this](const FishTurned& ev) { on_turned(ev.ent); });
    fsu_id = world->sub<FishSizeUp>([this](const FishSizeUp& ev) { on_size_up(ev.ent, ev.mult); });
  }

  ~FishRenderSystem() {
    world->unsub<AnimationDone>(ad_id);
    world->unsub<FishStateChanged>(fsc_id);
    world->unsub<FishTurned>(ft_id);
    world->unsub<FishSizeUp>(fsu_id);
  }

  void update(float) override { /* No operations. */ };
};

Entity make_fish(
  std::shared_ptr<World> world,
  float speed,
  float size,
  int level,
  int x,
  int y,
  int width,
  int height,
  FishDirection dir,
  const std::vector<ImageId>& swim,
  const std::vector<ImageId>& eat,
  const std::vector<ImageId>& turn,
  const std::vector<ImageId>& idle
);

void destroy_fish(std::shared_ptr<World> world, Entity ent);

// Mine

class Mine : public Component {
public:
  float speed;  // speed multiplier
  Mine(float speed = 0.0f) : speed(speed) { }
};

Entity make_mine(
  std::shared_ptr<World> world,
  float speed,
  ImageId img,
  int x,
  int y,
  int width,
  int height
);

// Star (HP pickup)

class Star : public Component {
public:
  float speed;  // speed multiplier
  Star(float speed = 0.0f) : speed(speed) { }
};

Entity make_star(
  std::shared_ptr<World> world,
  float speed,
  ImageId img,
  int x,
  int y,
  int width,
  int height
);
