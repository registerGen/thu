#include "movement.h"

#include "role.h"

void MovementSystem::update(float dt) {
  world->for_each<Movable, Coordinate, Velocity>([this, dt](
                                                   Entity ent,
                                                   std::shared_ptr<Movable>,
                                                   std::shared_ptr<Coordinate> pos,
                                                   std::shared_ptr<Velocity> v
                                                 ) {
    pos->x = static_cast<int>(std::round(static_cast<float>(pos->x) + v->x * dt));
    pos->y = static_cast<int>(std::round(static_cast<float>(pos->y) + v->y * dt));

    // Update fish animations positions.
    if (auto fish = world->get<Fish>(ent); fish) {
      auto rect = world->get<Collidable>(ent);
      for (auto anim : fish->anims) {
        auto anim_pos = world->get<Coordinate>(anim);
        if (!anim_pos) continue;
        anim_pos->x = pos->x;
        anim_pos->y = pos->y;
      }
    }
  });
}
