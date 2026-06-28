#pragma once

#include <random>

#include "ecs.h"
#include "player.h"

class SpawnSystem : public System {
private:
  std::shared_ptr<World> world;
  std::shared_ptr<std::mt19937> rng;
  int win_width, win_height;

  SubscriptionId ps_id, pd_id;

  void on_player_spawned(const PlayerSpawned& ev);
  void on_player_died(const PlayerDied& ev);

public:
  SpawnSystem(std::shared_ptr<World> world)
      : world(world),
        rng(world->get_ctx<std::mt19937>()),
        win_width(world->get_ctx<WindowConfig>()->width),
        win_height(world->get_ctx<WindowConfig>()->height),
        pd_id(0) {
    disable();
    ps_id = world->sub<PlayerSpawned>([this](const PlayerSpawned& ev) { on_player_spawned(ev); });
    pd_id = world->sub<PlayerDied>([this](const PlayerDied& ev) { on_player_died(ev); });
  }

  ~SpawnSystem() {
    world->unsub<PlayerSpawned>(ps_id);
    world->unsub<PlayerDied>(pd_id);
  }

  void update(float dt) override;
};
