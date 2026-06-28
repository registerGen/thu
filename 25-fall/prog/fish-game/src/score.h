// Scoring system that updates the score (and HUD) of the players.

#pragma once

#include <memory>

#include "collision.h"
#include "ecs.h"

class ScoreSystem : public System {
private:
  std::shared_ptr<World> world;
  SubscriptionId fe_id;

  void on_fish_eaten(Entity eater, Entity prey);

public:
  ScoreSystem(std::shared_ptr<World> world) : world(world), fe_id(0) { disable(); }

  void enable() {
    fe_id =
      world->sub<FishEaten>([this](const FishEaten& ev) { on_fish_eaten(ev.eater, ev.prey); });
    enabled = true;
  }

  void disable() {
    world->unsub<FishEaten>(fe_id);
    enabled = false;
  }

  void update(float) override { /* No operations. */ }
};
