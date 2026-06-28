// Scene management system that controls what is displayed on the screen.

#pragma once

#include <array>
#include <cstdint>

#include "ecs.h"
#include "ui.h"

enum class Scene : std::uint8_t {
  Invalid = 0,
  Loading,
  StartMenu,
  Help,
  LevelSelect,
  LevelLocked,
  LevelDesc,
  Game,
  GamePause,
  GameWin,
  GameLose,
  count,
};

class SceneChanged : public Event {
public:
  Scene target;
  SceneChanged(Scene target) : target(target) { }
};

class SceneOverlaid : public Event {
public:
  Scene target;
  SceneOverlaid(Scene target) : target(target) { }
};

class SceneHidden : public Event {
public:
  Scene target;
  SceneHidden(Scene target) : target(target) { }
};

class SceneSystem : public System {
private:
  std::shared_ptr<World> world;
  Scene cur_scene;
  std::array<std::vector<Entity>, std::to_underlying(Scene::count)> ents;

  SubscriptionId sc_id, so_id, sh_id;

  void show(Scene scene);
  void hide(Scene scene);
  void change(Scene scene);

public:
  SceneSystem(std::shared_ptr<World> world) : world(world), cur_scene(Scene::Invalid) {
    sc_id = world->sub<SceneChanged>([this](const SceneChanged& ev) { this->change(ev.target); });
    so_id = world->sub<SceneOverlaid>([this](const SceneOverlaid& ev) { this->show(ev.target); });
    sh_id = world->sub<SceneHidden>([this](const SceneHidden& ev) { this->hide(ev.target); });
  }

  ~SceneSystem() {
    world->unsub<SceneChanged>(sc_id);
    world->unsub<SceneOverlaid>(so_id);
    world->unsub<SceneHidden>(sh_id);
  }

  // Get current Scene.
  Scene current() const { return cur_scene; }

  // Register a Entity for a Scene.
  void reg(Scene scene, Entity ent);

  // Un-register a Entity from a Scene.
  void unreg(Scene scene, Entity ent);

  // Un-register all Entities from a Scene.
  void clear(Scene scene);

  void update(float) override { /* No operations. */ }
};
