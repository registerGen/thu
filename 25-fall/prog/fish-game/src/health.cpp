#include "health.h"

#include <format>

#include "game.h"
#include "player.h"
#include "role.h"
#include "timer.h"
#include "ui.h"

void HealthSystem::on_fish_eaten(Entity eater, Entity prey) {
  auto fish_prey = world->get<Fish>(prey);
  if (!fish_prey) return;

  auto cleanup_prey = [this, prey]() { destroy_fish(world, prey); };

  auto player_prey = world->get<Player>(prey);
  if (!player_prey) {
    cleanup_prey();
    return;
  }

  auto fish_eater = world->get<Fish>(eater);
  if (!fish_eater) {
    cleanup_prey();
    return;
  }

  int init_health = world->get_ctx<GameConfig>()->init_health;

  // Decrease health.
  player_prey->health -= fish_eater->level;
  if (player_prey->health <= 0) {
    world->emit(GameFinished(player_prey->id == 1));
    cleanup_prey();
    return;
  }

  // Update HUD.
  auto hud = world->get<Text>(player_prey->health_hud);
  if (hud) hud->text = std::format("HP {:>2}", player_prey->health);

  auto bar_hud = world->get<ProgressBar>(player_prey->health_bar_hud);
  if (bar_hud)
    bar_hud->percent = std::clamp(
      static_cast<float>(player_prey->health) / static_cast<float>(init_health),
      0.0f,
      1.0f
    );

  world->emit(PlayerDied(
    prey,
    player_prey->id,
    player_prey->score,
    player_prey->health,
    fish_prey->level,
    fish_prey->speed,
    fish_prey->size
  ));

  cleanup_prey();
}

void HealthSystem::on_mine_exploded(Entity mine, Entity victim) {
  auto player_victim = world->get<Player>(victim);
  if (player_victim) {
    // Boom!
    player_victim->health = 0;

    uint8_t victim_id = player_victim->id;
    Entity timer = make_countdown_timer(world, 0, 1, false);
    world->sub_once<TimeUp>([this, timer, victim_id](const TimeUp& ev) {
      if (ev.ent != timer) return false;
      world->emit(GameFinished(victim_id == 1));
      return true;
    });
  }

  if (world->has<Fish>(victim)) destroy_fish(world, victim);
  if (world->has<Mine>(mine)) world->destroy(mine);
}

void HealthSystem::on_star_collected(Entity collector, Entity star) {
  auto player_collector = world->get<Player>(collector);
  if (!player_collector) return;

  int init_health = world->get_ctx<GameConfig>()->init_health;

  // Increase health.
  player_collector->health = std::min(player_collector->health + 2, init_health);

  // Update HUD.
  auto hud = world->get<Text>(player_collector->health_hud);
  if (hud) hud->text = std::format("HP {:>2}", player_collector->health);

  auto bar_hud = world->get<ProgressBar>(player_collector->health_bar_hud);
  if (bar_hud)
    bar_hud->percent = std::clamp(
      static_cast<float>(player_collector->health) / static_cast<float>(init_health),
      0.0f,
      1.0f
    );

  if (world->has<Star>(star)) world->destroy(star);
}
