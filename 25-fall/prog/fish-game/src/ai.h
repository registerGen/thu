// AI system with certain movement behaviors.

#pragma once

#include <random>

#include "config.h"
#include "ecs.h"
#include "role.h"

class AIRandom : public Component {
public:
  FishDirection dir;
  AIRandom(FishDirection dir = FishDirection::Left) : dir(dir) { }
};

class AIEater : public Component { };

class AIPlayerKiller : public Component { };

class AIMine : public Component { };

class AIStar : public Component { };

class AISystem : public System {
private:
  std::shared_ptr<World> world;
  int win_width, win_height;
  std::shared_ptr<std::mt19937> rng;

public:
  AISystem(std::shared_ptr<World> world)
      : world(world),
        win_width(world->get_ctx<WindowConfig>()->width),
        win_height(world->get_ctx<WindowConfig>()->height),
        rng(world->get_ctx<std::mt19937>()) {
    disable();
  }

  void update(float dt) override;
};
