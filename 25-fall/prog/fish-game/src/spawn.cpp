#include "spawn.h"

#include "ai.h"
#include "game.h"
#include "role.h"
#include "timer.h"

// Spawn a new player when the player is spawned.
void SpawnSystem::on_player_spawned(const PlayerSpawned& ev) {
  auto config = world->get_ctx<GameConfig>();
  auto visuals = world->get_ctx<GameVisualData>();
  auto score_prog = std::clamp(
    static_cast<float>(ev.score) / static_cast<float>(config->target_score()),
    0.0f,
    1.0f
  );
  auto health_prog =
    std::clamp(static_cast<float>(ev.health) / static_cast<float>(config->init_health), 0.0f, 1.0f);
  auto level_prog =
    std::clamp(static_cast<float>(ev.level) / static_cast<float>(config->levels()), 0.0f, 1.0f);
  const auto& angelfish = visuals->fish_visuals.at("angelfish");

  ev.ent = make_fish(
    world,
    ev.speed,
    ev.size,
    ev.level,
    win_width / 2,
    win_height / 2,
    angelfish.width,
    angelfish.height,
    FishDirection::Left,
    angelfish.swim,
    angelfish.eat,
    angelfish.turn,
    angelfish.idle
  );
  make_player(
    world,
    ev.ent,
    ev.id,
    ev.score,
    ev.health,
    ev.level,
    ev.protection,
    score_prog,
    health_prog,
    level_prog,
    visuals->hud_font.chars,
    visuals->hud_font.width,
    visuals->hud_font.height,
    150,
    15,
    win_width * (ev.id + 1) / 3,
    30,
    win_width * (ev.id + 1) / 3,
    60,
    win_width * (ev.id + 1) / 3,
    90,
    win_width * (ev.id + 1) / 3,
    120,
    win_width * (4 * ev.id + 1) / 6,
    30,
    win_width * (4 * ev.id + 1) / 6,
    60,
    win_width * (4 * ev.id + 1) / 6,
    90
  );
  world->emit(FishStateChanged(ev.ent, FishState::Swim));

  // Set up protection timer if the player has protection.
  if (ev.protection && world->get_ctx<GameConfig>()->mode != GameMode::Multiplayer) {
    Entity prot_timer = make_countdown_timer(world, 0, 2, false);
    auto cheats = world->get_ctx<GameCheatState>();
    Entity protected_player = ev.ent;
    world->sub_once<TimeUp>([this, protected_player, prot_timer, cheats](const TimeUp& prot_ev) {
      if (prot_ev.ent != prot_timer) return false;
      if (cheats && cheats->shield) return true;
      auto player = world->get<Player>(protected_player);
      if (player) {
        player->protection = false;
        world->remove<Shown>(player->protection_hud);
      }
      return true;
    });
  }
}

// Re-spawn the player after death.
void SpawnSystem::on_player_died(const PlayerDied& ev) {
  auto cheats = world->get_ctx<GameCheatState>();
  const uint8_t player_id = ev.id;
  const int player_score = ev.score;
  const int player_health = ev.health;
  const int player_level = ev.level;
  const float fish_speed = ev.speed;
  const float fish_size = ev.size;

  // Wait for 1 second before re-spawning the player.
  Entity resp_timer = make_countdown_timer(world, 0, 1, false);
  world->sub_once<TimeUp>([this,
                           player_id,
                           player_score,
                           player_health,
                           player_level,
                           fish_speed,
                           fish_size,
                           resp_timer,
                           cheats](const TimeUp& resp_ev) {
    if (resp_ev.ent != resp_timer) return false;

    Entity new_player;
    world->emit_now(PlayerSpawned(
      new_player,
      player_id,
      player_score,
      player_health,
      world->get_ctx<GameConfig>()->mode != GameMode::Multiplayer,
      fish_speed,
      fish_size,
      player_level
    ));

    return true;
  });
}

// Spawn fish and mine according to the spawn configurations.
void SpawnSystem::update(float) {
  auto config = world->get_ctx<GameConfig>();
  auto visuals = world->get_ctx<GameVisualData>();

  for (const auto& [name, spawn_cfg] : config->fish_spawns) {
    if (std::bernoulli_distribution(spawn_cfg.spawn_prob)(*rng)) {
      AIMode ai_mode = AIMode::Invalid;
      float ai_rand = std::uniform_real_distribution<float>(0.0f, 1.0f)(*rng);
      float acc_prob = 0.0f;
      for (size_t i = 1U; i < std::to_underlying(AIMode::count); ++i) {
        acc_prob += spawn_cfg.ai_mode_probs[i];
        if (ai_rand <= acc_prob) {
          ai_mode = static_cast<AIMode>(i);
          break;
        }
      }

      const auto& fish_vis = visuals->fish_visuals.at(name);

      FishDirection dir =
        std::bernoulli_distribution(0.5)(*rng) ? FishDirection::Left : FishDirection::Right;
      int x = dir == FishDirection::Left
                ? static_cast<int>(static_cast<float>(fish_vis.width) * spawn_cfg.size / 2) - 5 +
                    win_width
                : -static_cast<int>(static_cast<float>(fish_vis.width) * spawn_cfg.size / 2) + 5;
      int y = std::uniform_int_distribution<int>(0, win_height - 1)(*rng);

      Entity bot = make_fish(
        world,
        std::uniform_real_distribution<float>(spawn_cfg.min_speed, spawn_cfg.max_speed)(*rng),
        spawn_cfg.size,
        spawn_cfg.level,
        x,
        y,
        fish_vis.width,
        fish_vis.height,
        dir,
        fish_vis.swim,
        fish_vis.eat,
        fish_vis.turn,
        fish_vis.idle
      );

      switch (ai_mode) {
      case AIMode::Invalid:
      case AIMode::count:
        break;

      case AIMode::Random:
        world->add<AIRandom>(bot, dir);
        break;

      case AIMode::Eater:
        world->add<AIEater>(bot);
        break;

      case AIMode::PlayerKiller:
        world->add<AIPlayerKiller>(bot);
        break;
      }

      world->emit(FishStateChanged(bot, FishState::Swim));
    }
  }

  if (auto& mine_cfg = config->mine_spawn; std::bernoulli_distribution(mine_cfg.spawn_prob)(*rng)) {
    int x, y;
    auto players = world->get<Player, Coordinate, Collidable>();

    auto min_player_dist = [&players](int mx, int my) {
      float min_dist = std::numeric_limits<float>::max();
      for (const auto& [ent, _, pos, rect] : players) {
        if (pos) {
          int dx = pos->x - mx;
          int dy = pos->y - my;
          float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy)) -
                       static_cast<float>(std::max(rect->width, rect->height)) / 2.0f;
          if (dist < min_dist) {
            min_dist = dist;
          }
        }
      }
      return min_dist;
    };

    do {
      // Spawn at a random position.
      x = std::uniform_int_distribution<int>(0, win_width - 1)(*rng);
      y = std::uniform_int_distribution<int>(0, win_height - 1)(*rng);
    } while (min_player_dist(x, y) <= 300.0f);  // Avoid spawning mines too close to players.

    Entity mine = make_mine(
      world,
      std::uniform_real_distribution<float>(mine_cfg.min_speed, mine_cfg.max_speed)(*rng),
      visuals->mine,
      x,
      y,
      visuals->mine_width,
      visuals->mine_height
    );
    world->add<Shown>(mine);
  }

  if (auto star_cfg = config->star_spawn; std::bernoulli_distribution(star_cfg.spawn_prob)(*rng)) {
    // Fall from a random position at the top.
    int x = std::uniform_int_distribution<int>(0, win_width - 1)(*rng);
    int y = std::uniform_int_distribution<int>(0, 20)(*rng);

    Entity star = make_star(
      world,
      std::uniform_real_distribution<float>(star_cfg.min_speed, star_cfg.max_speed)(*rng),
      visuals->star,
      x,
      y,
      visuals->star_width,
      visuals->star_height
    );
    world->add<Shown>(star);
  }
}
