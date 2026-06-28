#include "role.h"

#include <algorithm>

#include "player.h"

void make_background_image(std::shared_ptr<World> world, Entity ent, int width, int height) {
  world->add<BackgroundImage>(ent, width, height);
  world->add<Movable>(ent);
  world->add<Velocity>(ent, 0.0f, 0.0f);
}

// Directly change the state without phasing.
void FishRenderSystem::change(std::shared_ptr<Fish> fish, FishState state) {
  Entity anim = fish->anims[std::to_underlying(state)];
  if (!anim) return;

  if (fish->cur_anim) world->remove<Shown>(fish->cur_anim);
  world->add<Shown>(anim);
  fish->cur_anim = anim;
  fish->state = state;
}

void FishRenderSystem::on_anim_done(Entity ent) {
  world->for_each<Fish>([ent, this](Entity, std::shared_ptr<Fish> fish) {
    if (ent != fish->cur_anim) return;

    // Flip the images when a U-turn is finished.
    if (ent == fish->anims[std::to_underlying(FishState::Turn)]) {
      for (FishState s : {FishState::Swim, FishState::Eat, FishState::Idle}) {
        Entity anim = fish->anims[std::to_underlying(s)];
        auto trans = world->get<ImageTransform>(anim);
        world->add<ImageTransform>(anim, trans->scale, !trans->mirror);
      }
      // Also reverse the frames of fish turning animation.
      std::reverse(
        world->get<Animation>(ent)->imgs.begin(),
        world->get<Animation>(ent)->imgs.end()
      );
    }

    // Previous animation is complete, so we safely switch to the next state.
    FishState next = fish->next_state != FishState::Invalid ? fish->next_state : FishState::Swim;
    change(fish, next);
    fish->next_state = FishState::Invalid;
  });
}

void FishRenderSystem::on_state_changed(Entity ent, FishState state) {
  auto fish = world->get<Fish>(ent);
  if (!fish || fish->state == state || state == FishState::Invalid) return;

  if (fish->state == FishState::Invalid || fish->state == FishState::Swim ||
      fish->state == FishState::Idle) {
    fish->next_state = fish->state;  // So that we restore the previous state after eating/turning.
    change(fish, state);             // Change it without phasing.
  } else
    fish->next_state = state;  // Defer until the running animation is done.
}

void FishRenderSystem::on_turned(Entity ent) {
  auto fish = world->get<Fish>(ent);
  if (!fish || (fish->state != FishState::Swim && fish->state != FishState::Idle)) return;

  world->emit(FishStateChanged(ent, FishState::Turn));
  fish->dir = fish->dir == FishDirection::Left ? FishDirection::Right : FishDirection::Left;
}

void FishRenderSystem::on_size_up(Entity ent, float mult) {
  auto fish = world->get<Fish>(ent);
  auto rect = world->get<Collidable>(ent);
  if (!fish || !rect) return;

  fish->size *= mult;
  rect->width = static_cast<int>(static_cast<float>(rect->width) * mult);
  rect->height = static_cast<int>(static_cast<float>(rect->height) * mult);

  for (FishState s : {FishState::Swim, FishState::Eat, FishState::Turn, FishState::Idle}) {
    Entity anim = fish->anims[std::to_underlying(s)];
    world->get<ImageTransform>(anim)->scale *= mult;
  }
}

Entity make_fish(
  std::shared_ptr<World> world,
  float speed,
  float size,
  int level,
  int x,
  int y,
  int width,
  int height,
  FishDirection dir,
  const std::vector<ImageId>& swim,
  const std::vector<ImageId>& eat,
  const std::vector<ImageId>& turn,
  const std::vector<ImageId>& idle
) {
  std::vector<ImageId> turn_rvr(turn.size());
  std::reverse_copy(turn.begin(), turn.end(), turn_rvr.begin());
  width = static_cast<int>(static_cast<float>(width) * size);
  height = static_cast<int>(static_cast<float>(height) * size);
  Entity swim_ent =
    make_animation(world, swim, x, y, zindex::GAME, 0.1f, false, size, std::to_underlying(dir));
  Entity eat_ent =
    make_animation(world, eat, x, y, zindex::GAME, 0.1f, true, size, std::to_underlying(dir));
  Entity turn_ent = make_animation(
    world,
    dir == FishDirection::Right ? turn_rvr : turn,
    x,
    y,
    zindex::GAME,
    0.1f,
    true,
    size
  );
  Entity idle_ent =
    make_animation(world, idle, x, y, zindex::GAME, 0.1f, false, size, std::to_underlying(dir));
  Entity ent = world->make_entity();
  world->add<Coordinate>(ent, x, y);
  world->add<Velocity>(ent, 0.0f, 0.0f);
  world->add<Movable>(ent);
  world->add<Collidable>(ent, width, height);
  world->add<
    Fish>(ent, speed, size, level, FishState::Invalid, dir, swim_ent, eat_ent, turn_ent, idle_ent);
  return ent;
}

void destroy_fish(std::shared_ptr<World> world, Entity ent) {
  auto fish = world->get<Fish>(ent);
  if (!fish) return;

  std::ranges::for_each(fish->anims, [world](const Entity& anim) { world->destroy(anim); });

  if (auto player = world->get<Player>(ent); player) {
    world->destroy(player->score_hud);
    world->destroy(player->health_hud);
    world->destroy(player->level_hud);
    world->destroy(player->protection_hud);
    world->destroy(player->score_bar_hud);
    world->destroy(player->health_bar_hud);
    world->destroy(player->level_bar_hud);
  }

  world->destroy(ent);
}

Entity make_mine(
  std::shared_ptr<World> world,
  float speed,
  ImageId img,
  int x,
  int y,
  int width,
  int height
) {
  Entity ent = world->make_entity();
  world->add<Coordinate>(ent, x, y);
  world->add<Velocity>(ent, 0.0f, 0.0f);
  world->add<Movable>(ent);
  world->add<Collidable>(ent, width, height);
  world->add<Image>(ent, img, zindex::GAME);
  world->add<Mine>(ent, speed);
  return ent;
}

Entity make_star(
  std::shared_ptr<World> world,
  float speed,
  ImageId img,
  int x,
  int y,
  int width,
  int height
) {
  Entity ent = world->make_entity();
  world->add<Coordinate>(ent, x, y);
  world->add<Velocity>(ent, 0.0f, 0.0f);
  world->add<Movable>(ent);
  world->add<Collidable>(ent, width, height);
  world->add<Image>(ent, img, zindex::GAME);
  world->add<Star>(ent, speed);
  return ent;
}
