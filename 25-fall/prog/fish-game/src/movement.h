// Movement system that updates entity positions based on their velocities.

#pragma once

#include "config.h"
#include "ecs.h"

class MovementSystem : public System {
private:
  std::shared_ptr<World> world;
  int win_width, win_height;

public:
  MovementSystem(std::shared_ptr<World> world)
      : world(world),
        win_width(world->get_ctx<WindowConfig>()->width),
        win_height(world->get_ctx<WindowConfig>()->height) {
    disable();
  }

  void update(float dt) override;
};
