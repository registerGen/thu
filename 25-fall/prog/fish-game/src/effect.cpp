#include "effect.h"

#include "player.h"
#include "role.h"
#include "ui.h"

void EffectSystem::on_scene_changed(Scene sc) {
  SoundId next_bgm = 0;

  switch (sc) {
  case Scene::Invalid:
  case Scene::count:
  case Scene::Loading:
    break;

  case Scene::StartMenu:
  case Scene::Help:
  case Scene::LevelSelect:
  case Scene::LevelLocked:
  case Scene::LevelDesc:
    next_bgm = menubgm_sound;
    break;

  case Scene::Game:
  case Scene::GamePause:
    next_bgm = gamebgm_sound;
    break;

  case Scene::GameWin:
    next_bgm = gamewin_sound;
    break;

  case Scene::GameLose:
    next_bgm = gamelose_sound;
    break;
  }

  if (next_bgm == current_bgm) return;
  sm->stop();
  sm->play_loop(next_bgm);
  current_bgm = next_bgm;
}

void EffectSystem::on_player_spawned() { sm->play_once(playerspawn_sound); }

void EffectSystem::on_fish_eaten(Entity eater, Entity prey) {
  auto player_eater = world->get<Player>(eater);
  auto player_prey = world->get<Player>(prey);

  if (player_eater) {
    auto fish_prey = world->get<Fish>(prey);
    auto pos = world->get<Coordinate>(eater);
    if (!fish_prey || !pos) return;

    Entity flash = make_flash(
      world,
      plus[static_cast<size_t>(fish_prey->level)],
      pos->x,
      pos->y,
      zindex::UI,
      0.5f
    );
    world->add<Shown>(flash);

    sm->play_once(playereat_sound);
  } else if (player_prey) {
    sm->play_once(playereaten_sound);
  }
}

void EffectSystem::on_fish_size_up(Entity fish) {
  auto player = world->get<Player>(fish);
  if (!player) return;

  sm->play_once(playersizeup_sound);
}

void EffectSystem::on_mine_exploded(Entity, Entity victim) {
  auto pos = world->get<Coordinate>(victim);
  if (!pos) return;

  Entity anim = make_animation(world, explode, pos->x, pos->y, zindex::UI, 0.1f, true);
  world->add<Shown>(anim);

  sm->play_once(mineexplode_sound);
}
