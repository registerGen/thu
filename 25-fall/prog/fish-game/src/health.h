// Health system that updates the health point (and HUD) of the players.

#pragma once

#include "collision.h"
#include "ecs.h"

class HealthSystem : public System {
private:
  std::shared_ptr<World> world;
  SubscriptionId fe_id, me_id, sc_id;

  void on_fish_eaten(Entity eater, Entity prey);
  void on_mine_exploded(Entity mine, Entity victim);
  void on_star_collected(Entity collector, Entity star);

public:
  HealthSystem(std::shared_ptr<World> world) : world(world), fe_id(0) { disable(); }

  void enable() {
    fe_id =
      world->sub<FishEaten>([this](const FishEaten& ev) { on_fish_eaten(ev.eater, ev.prey); });
    me_id = world->sub<MineExploded>([this](const MineExploded& ev) {
      on_mine_exploded(ev.mine, ev.victim);
    });
    sc_id = world->sub<StarCollected>([this](const StarCollected& ev) {
      on_star_collected(ev.collector, ev.star);
    });
    enabled = true;
  }

  void disable() {
    world->unsub<FishEaten>(fe_id);
    world->unsub<MineExploded>(me_id);
    world->unsub<StarCollected>(sc_id);
    enabled = false;
  }

  void update(float) override { /* No operations. */ }
};
