#include "ai.h"

#include "player.h"
#include "role.h"

void AISystem::update(float) {
  // Boundary checks, with tolerance.
  auto out_of_bound = [this](std::shared_ptr<Coordinate> pos, std::shared_ptr<Collidable> rect) {
    if (pos->x + rect->width / 2 < -win_width / 4) return true;
    if (pos->x - rect->width / 2 > win_width + win_width / 4) return true;
    if (pos->y + rect->height / 2 < -win_height / 4) return true;
    if (pos->y - rect->height / 2 > win_height + win_height / 4) return true;
    return false;
  };

  auto bg = world->get<BackgroundImage, Velocity>();
  std::shared_ptr<Velocity> bg_vel = bg.empty() ? nullptr : std::get<2>(bg[0]);

  std::vector<Entity> outs;

  // Random-walk AI: Enforces a fixed horizontal direction per fish and adds gentle vertical drift.
  world->for_each<AIRandom, Fish, Coordinate, Velocity, Collidable>(
    [&out_of_bound, &outs, bg_vel, this](
      Entity ent,
      std::shared_ptr<AIRandom> ai,
      std::shared_ptr<Fish> fish,
      std::shared_ptr<Coordinate> pos,
      std::shared_ptr<Velocity> v,
      std::shared_ptr<Collidable> rect
    ) {
      if (out_of_bound(pos, rect)) {
        outs.emplace_back(ent);
        return;
      }

      v->x = std::uniform_real_distribution<float>(80.0f, 120.0f)(*rng) * fish->speed;
      if (ai->dir == FishDirection::Left) v->x = -v->x;

      v->y += std::uniform_real_distribution<float>(-10.0f, 10.0f)(*rng);
      v->y = std::clamp(v->y, -50.0f, 50.0f);

      // Compensate for background movement.
      if (bg_vel) {
        v->x += bg_vel->x;
        v->y += bg_vel->y;
      }
    }
  );

  // Aggressive eater AI: Moves towards the nearest smaller fish.
  world->for_each<AIEater, Fish, Coordinate, Velocity, Collidable>(
    [&out_of_bound, &outs, bg_vel, this](
      Entity ent,
      std::shared_ptr<AIEater>,
      std::shared_ptr<Fish> fish,
      std::shared_ptr<Coordinate> pos,
      std::shared_ptr<Velocity> v,
      std::shared_ptr<Collidable> rect
    ) {
      if (out_of_bound(pos, rect)) {
        outs.emplace_back(ent);
        return;
      }

      // Find nearest smaller fish.
      Entity target = 0;
      float min_dist_sq = std::numeric_limits<float>::max();

      auto fishes = world->get<Fish, Coordinate>();
      for (const auto& [other_ent, other_fish, other_pos] : fishes) {
        if (other_ent == ent) continue;
        if (other_fish->level >= fish->level) continue;

        float dx = static_cast<float>(other_pos->x - pos->x);
        float dy = static_cast<float>(other_pos->y - pos->y);
        float dist_sq = dx * dx + dy * dy;
        if (dist_sq < min_dist_sq) {
          min_dist_sq = dist_sq;
          target = other_ent;
        }
      }

      if (target != 0) {
        auto target_pos = world->get<Coordinate>(target);
        float dx = static_cast<float>(target_pos->x - pos->x);
        float dy = static_cast<float>(target_pos->y - pos->y);
        float dist = std::max(0.001f, std::sqrt(dx * dx + dy * dy));

        v->x = (dx / dist) * fish->speed * 120.0f;
        v->y = (dy / dist) * fish->speed * 120.0f;
      } else {
        // No target: stay still.
        v->x = 0.0f;
        v->y = 0.0f;
      }

      // Handle fish turning.
      if ((fish->dir == FishDirection::Right && v->x < 0.0f) ||
          (fish->dir == FishDirection::Left && v->x > 0.0f))
        world->emit(FishTurned(ent));

      // Compensate for background movement.
      if (bg_vel) {
        v->x += bg_vel->x;
        v->y += bg_vel->y;
      }
    }
  );

  // Player killer AI: Moves towards the nearest player fish.
  world->for_each<AIPlayerKiller, Fish, Coordinate, Velocity, Collidable>(
    [&out_of_bound, &outs, bg_vel, this](
      Entity ent,
      std::shared_ptr<AIPlayerKiller>,
      std::shared_ptr<Fish> fish,
      std::shared_ptr<Coordinate> pos,
      std::shared_ptr<Velocity> v,
      std::shared_ptr<Collidable> rect
    ) {
      if (out_of_bound(pos, rect)) {
        outs.emplace_back(ent);
        return;
      }

      // Find nearest player fish.
      Entity target = 0;
      float min_dist_sq = std::numeric_limits<float>::max();

      auto players = world->get<Player, Fish, Coordinate>();
      for (const auto& [other_ent, _, other_fish, other_pos] : players) {
        if (other_fish->level >= fish->level) continue;

        float dx = static_cast<float>(other_pos->x - pos->x);
        float dy = static_cast<float>(other_pos->y - pos->y);
        float dist_sq = dx * dx + dy * dy;
        if (dist_sq < min_dist_sq) {
          min_dist_sq = dist_sq;
          target = other_ent;
        }
      }

      if (target != 0) {
        auto target_pos = world->get<Coordinate>(target);
        float dx = static_cast<float>(target_pos->x - pos->x);
        float dy = static_cast<float>(target_pos->y - pos->y);
        float dist = std::max(0.001f, std::sqrt(dx * dx + dy * dy));

        v->x = (dx / dist) * fish->speed * 150.0f;
        v->y = (dy / dist) * fish->speed * 150.0f;
      } else {
        // No target: stay still.
        v->x = 0.0f;
        v->y = 0.0f;
      }

      // Handle fish turning.
      if ((fish->dir == FishDirection::Right && v->x < 0.0f) ||
          (fish->dir == FishDirection::Left && v->x > 0.0f))
        world->emit(FishTurned(ent));

      // Compensate for background movement.
      if (bg_vel) {
        v->x += bg_vel->x;
        v->y += bg_vel->y;
      }
    }
  );

  // Mine AI: randomly and slowly drifts around.
  world->for_each<Mine, Coordinate, Velocity, Collidable>([&out_of_bound, &outs, bg_vel, this](
                                                            Entity ent,
                                                            std::shared_ptr<Mine> mine,
                                                            std::shared_ptr<Coordinate> pos,
                                                            std::shared_ptr<Velocity> v,
                                                            std::shared_ptr<Collidable> rect
                                                          ) {
    if (out_of_bound(pos, rect)) {
      outs.emplace_back(ent);
      return;
    }

    v->x += std::uniform_real_distribution<float>(-5.0f * mine->speed, 5.0f * mine->speed)(*rng);
    v->x = std::clamp(v->x, -40.0f * mine->speed, 40.0f * mine->speed);

    v->y += std::uniform_real_distribution<float>(-5.0f * mine->speed, 5.0f * mine->speed)(*rng);
    v->y = std::clamp(v->y, -40.0f * mine->speed, 40.0f * mine->speed);

    // Compensate for background movement.
    if (bg_vel) {
      v->x += bg_vel->x;
      v->y += bg_vel->y;
    }
  });

  // Star AI: fall downwards with random horizontal movement.
  world->for_each<Star, Coordinate, Velocity, Collidable>([&out_of_bound, &outs, bg_vel, this](
                                                            Entity ent,
                                                            std::shared_ptr<Star> star,
                                                            std::shared_ptr<Coordinate> pos,
                                                            std::shared_ptr<Velocity> v,
                                                            std::shared_ptr<Collidable> rect
                                                          ) {
    if (out_of_bound(pos, rect)) {
      outs.emplace_back(ent);
      return;
    }

    v->x += std::uniform_real_distribution<float>(-10.0f * star->speed, 10.0f * star->speed)(*rng);
    v->x = std::clamp(v->x, -30.0f * star->speed, 30.0f * star->speed);

    v->y += std::uniform_real_distribution<float>(-5.0f * star->speed, 5.0f * star->speed)(*rng);
    v->y = std::clamp(v->y, 50.0f * star->speed, 100.0f * star->speed);

    // Compensate for background movement.
    if (bg_vel) {
      v->x += bg_vel->x;
      v->y += bg_vel->y;
    }
  });

  // Anything that leaves the window bounds is culled.
  for (auto ent : outs) {
    if (world->has<Fish>(ent))
      destroy_fish(world, ent);
    else
      world->destroy(ent);
  }
}
