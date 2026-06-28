// Effect system that manages sound and visual effects in the game.

#pragma once

#include <array>

#include "collision.h"
#include "ecs.h"
#include "resource.h"
#include "role.h"
#include "scene.h"
#include "spawn.h"

class EffectSystem : public System {
private:
  std::shared_ptr<World> world;
  std::shared_ptr<BitmapManager> bm;
  std::shared_ptr<SoundManager> sm;

  SoundId menubgm_sound, gamebgm_sound, gamewin_sound, gamelose_sound;
  SoundId current_bgm;
  SoundId playerspawn_sound, playereat_sound, playereaten_sound;
  SoundId playersizeup_sound;
  SoundId mineexplode_sound;

  std::array<ImageId, 6> plus;
  std::vector<ImageId> explode;

  SubscriptionId sc_id, ps_id, fe_id, fsu_id, me_id;

  void on_scene_changed(Scene sc);

  void on_player_spawned();
  void on_fish_eaten(Entity eater, Entity prey);
  void on_fish_size_up(Entity fish);

  void on_mine_exploded(Entity mine, Entity victim);

public:
  EffectSystem(std::shared_ptr<World> world)
      : world(world), bm(world->get_ctx<BitmapManager>()), sm(world->get_ctx<SoundManager>()) {
    menubgm_sound = sm->load(L".\\assets\\sounds\\menubgm.wav");
    gamebgm_sound = sm->load(L".\\assets\\sounds\\gamebgm.wav");
    gamewin_sound = sm->load(L".\\assets\\sounds\\gamewin.wav");
    gamelose_sound = sm->load(L".\\assets\\sounds\\gamelose.wav");
    current_bgm = 0;
    playerspawn_sound = sm->load(L".\\assets\\sounds\\playerspawn.wav");
    playereat_sound = sm->load(L".\\assets\\sounds\\playereat.wav");
    playereaten_sound = sm->load(L".\\assets\\sounds\\playereaten.wav");
    playersizeup_sound = sm->load(L".\\assets\\sounds\\playersizeup.wav");
    mineexplode_sound = sm->load(L".\\assets\\sounds\\mineexplode.wav");

    for (size_t i = 1; i <= 5; ++i)
      plus[i] = bm->load(std::format(L".\\assets\\flashes\\plus{}.bmp", i));
    for (size_t i = 1; i <= 5; ++i)
      explode.emplace_back(bm->load(std::format(L".\\assets\\mine\\explode.{}.bmp", i)));

    sc_id =
      world->sub<SceneChanged>([this](const SceneChanged& ev) { on_scene_changed(ev.target); });
    ps_id = world->sub<PlayerSpawned>([this](const PlayerSpawned&) { on_player_spawned(); });
    fe_id =
      world->sub<FishEaten>([this](const FishEaten& ev) { on_fish_eaten(ev.eater, ev.prey); });
    fsu_id = world->sub<FishSizeUp>([this](const FishSizeUp& ev) { on_fish_size_up(ev.ent); });
    me_id = world->sub<MineExploded>([this](const MineExploded& ev) {
      on_mine_exploded(ev.mine, ev.victim);
    });
  }

  ~EffectSystem() {
    world->unsub<SceneChanged>(sc_id);
    world->unsub<PlayerSpawned>(ps_id);
    world->unsub<FishEaten>(fe_id);
    world->unsub<FishSizeUp>(fsu_id);
    world->unsub<MineExploded>(me_id);
  }

  void update(float) override { /* No operations. */ }
};
