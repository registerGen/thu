// Collision system that handles collisions between entities.

#pragma once

#include "ecs.h"

class FishEaten : public Event {
public:
  Entity eater;
  Entity prey;
  FishEaten(Entity eater, Entity prey) : eater(eater), prey(prey) { }
};

class MineExploded : public Event {
public:
  Entity mine;
  Entity victim;
  MineExploded(Entity mine, Entity victim) : mine(mine), victim(victim) { }
};

class StarCollected : public Event {
public:
  Entity collector;
  Entity star;
  StarCollected(Entity collector, Entity star) : collector(collector), star(star) { }
};

class CollisionSystem : public System {
private:
  std::shared_ptr<World> world;

  void eat_fish(Entity eater, Entity prey);
  void explode_mine(Entity mine, Entity victim);
  void collect_star(Entity collector, Entity star);

public:
  CollisionSystem(std::shared_ptr<World> world) : world(world) { disable(); }

  void update(float dt) override;
};
