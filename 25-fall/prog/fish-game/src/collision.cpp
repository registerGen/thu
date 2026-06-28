#include "collision.h"

#include "player.h"
#include "role.h"

// These macros are defined in windows.h, so we get rid of them.
#ifdef max
# undef max
#endif
#ifdef min
# undef min
#endif

void CollisionSystem::eat_fish(Entity eater, Entity prey) {
  world->emit(FishStateChanged(eater, FishState::Eat));
  world->emit(FishEaten(eater, prey));
}

void CollisionSystem::explode_mine(Entity mine, Entity victim) {
  world->emit(MineExploded(mine, victim));
}

void CollisionSystem::collect_star(Entity collector, Entity star) {
  world->emit(FishStateChanged(collector, FishState::Eat));
  world->emit(StarCollected(collector, star));
}

void CollisionSystem::update(float) {
  // Calculate the area of overlapping part of two rectangles,
  // divided by the maximum overlap that can achieve.
  auto overlap = [](
                   const Collidable& rect1,
                   const Coordinate& pos1,
                   const Collidable& rect2,
                   const Coordinate& pos2
                 ) {
    Coordinate lt1 = Coordinate(pos1.x - rect1.width / 2, pos1.y - rect1.height / 2);
    Coordinate rb1 = Coordinate(pos1.x + rect1.width / 2, pos1.y + rect1.height / 2);
    Coordinate lt2 = Coordinate(pos2.x - rect2.width / 2, pos2.y - rect2.height / 2);
    Coordinate rb2 = Coordinate(pos2.x + rect2.width / 2, pos2.y + rect2.height / 2);

    int w = std::max(0, std::min(rb1.x, rb2.x) - std::max(lt1.x, lt2.x));
    int h = std::max(0, std::min(rb1.y, rb2.y) - std::max(lt1.y, lt2.y));
    int overlap = w * h;
    int maximum = std::min(rect1.width, rect2.width) * std::min(rect1.height, rect2.height);
    maximum = std::max(1, maximum);  // Prevent division by zero.
    return static_cast<float>(overlap) / static_cast<float>(maximum);
  };

  auto collidables = world->get<Collidable, Coordinate>();
  for (const auto& [ent1, rect1, pos1] : collidables)
    for (const auto& [ent2, rect2, pos2] : collidables) {
      if (ent1 >= ent2) continue;

      // Fish vs Fish: eat if overlap ratio >= 70%.
      if (world->has<Fish>(ent1) && world->has<Fish>(ent2) &&
          overlap(*rect1, *pos1, *rect2, *pos2) >= 0.7f) {
        bool ent1_player = world->has<Player>(ent1);
        bool ent2_player = world->has<Player>(ent2);
        if (ent1_player && ent2_player) continue;

        auto fish1 = world->get<Fish>(ent1);
        auto fish2 = world->get<Fish>(ent2);

        bool ent1_protection = ent1_player && world->get<Player>(ent1)->protection;
        bool ent2_protection = ent2_player && world->get<Player>(ent2)->protection;

        if (fish1->level > fish2->level && !ent2_protection)
          eat_fish(ent1, ent2);
        else if (fish1->level < fish2->level && !ent1_protection)
          eat_fish(ent2, ent1);
        // Player can eat fish with the same level.
        // Ugly logic, hmm...
        else if (ent1_player && fish1->level == fish2->level)
          eat_fish(ent1, ent2);
        else if (ent2_player && fish1->level == fish2->level)
          eat_fish(ent2, ent1);
      }

      // Mine vs Fish: explode if overlap ratio >= 20%.
      if (world->has<Mine>(ent1) && world->has<Fish>(ent2) &&
          overlap(*rect1, *pos1, *rect2, *pos2) >= 0.2f) {
        bool ent2_protection = false;
        if (world->has<Player>(ent2)) ent2_protection = world->get<Player>(ent2)->protection;
        if (!ent2_protection) explode_mine(ent1, ent2);
      }
      // ...or vice versa.
      if (world->has<Mine>(ent2) && world->has<Fish>(ent1) &&
          overlap(*rect2, *pos2, *rect1, *pos1) >= 0.2f) {
        bool ent1_protection = false;
        if (world->has<Player>(ent1)) ent1_protection = world->get<Player>(ent1)->protection;
        if (!ent1_protection) explode_mine(ent2, ent1);
      }

      // Star vs Player fish: collect if overlap ratio >= 50%.
      if (world->has<Star>(ent1) && world->has<Player>(ent2) &&
          overlap(*rect1, *pos1, *rect2, *pos2) >= 0.5f) {
        collect_star(ent2, ent1);
      }
      // ...or vice versa.
      if (world->has<Star>(ent2) && world->has<Player>(ent1) &&
          overlap(*rect2, *pos2, *rect1, *pos1) >= 0.5f) {
        collect_star(ent1, ent2);
      }
    }
}
