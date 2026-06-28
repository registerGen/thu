#include "score.h"

#include <algorithm>
#include <format>

#include "game.h"
#include "player.h"
#include "role.h"
#include "ui.h"

void ScoreSystem::on_fish_eaten(Entity eater, Entity prey) {
  auto player_eater = world->get<Player>(eater);
  auto player_prey = world->get<Player>(prey);

  if (!player_eater && !player_prey) return;
  if (player_prey) return;

  auto fish_prey = world->get<Fish>(prey);
  if (!fish_prey) return;

  bool level_up = false;
  auto config = world->get_ctx<GameConfig>();

  // Check for win or level-up conditions based on game mode.
  switch (config->mode) {
  case GameMode::Invalid:
  case GameMode::count:
    break;

  case GameMode::Career: {
    const auto& score_targets = config->score_targets.value();

    // If reached the last target, declare victory.
    if (player_eater->score < score_targets.back() &&
        player_eater->score + fish_prey->level >= score_targets.back()) {
      world->emit(GameFinished(true));
      return;
    }

    // If reached any other target, upgrade to the next level.
    level_up = std::upper_bound(score_targets.begin(), score_targets.end(), player_eater->score) !=
               std::upper_bound(
                 score_targets.begin(),
                 score_targets.end(),
                 player_eater->score + fish_prey->level
               );

    break;
  }

    // They share the same level-up logic.
  case GameMode::Time:
  case GameMode::Multiplayer: {
    const int score_per_lv = std::max(1, config->score_per_lv.value());
    const int next_target = score_per_lv * (player_eater->score / score_per_lv + 1);

    level_up =
      player_eater->score < next_target && player_eater->score + fish_prey->level >= next_target;

    if (level_up && next_target == score_per_lv * config->level_count.value()) {
      world->emit(GameFinished(player_eater->id == 0));
      return;
    }

    break;
  }
  }

  if (level_up) {
    auto fish_eater = world->get<Fish>(eater);
    ++player_eater->level;
    ++fish_eater->level;
    fish_eater->speed *= 1.2f;
    world->emit(FishSizeUp(eater, 1.2f));

    auto hud = world->get<Text>(player_eater->level_hud);
    if (hud) hud->text = std::format("LV {:>2}", player_eater->level);

    auto bar_hud = world->get<ProgressBar>(player_eater->level_bar_hud);
    if (bar_hud)
      bar_hud->percent = std::clamp(
        static_cast<float>(player_eater->level) / static_cast<float>(config->levels()),
        0.0f,
        1.0f
      );
  }

  player_eater->score += fish_prey->level;

  auto hud = world->get<Text>(player_eater->score_hud);
  if (hud) hud->text = std::format("P{} {:>3}pts", player_eater->id, player_eater->score);

  auto bar_hud = world->get<ProgressBar>(player_eater->score_bar_hud);
  if (bar_hud)
    bar_hud->percent = std::clamp(
      static_cast<float>(player_eater->score) / static_cast<float>(config->target_score()),
      0.0f,
      1.0f
    );
}
