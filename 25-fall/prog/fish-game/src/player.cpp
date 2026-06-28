#include "player.h"

#include <format>

#include "role.h"
#include "ui.h"

void PlayerControlSystem::update(float) {
  if (!use_key) {
    // Movable background image.
    world->for_each<BackgroundImage, Coordinate, Velocity>([this](
                                                             Entity,
                                                             std::shared_ptr<BackgroundImage> bg,
                                                             std::shared_ptr<Coordinate> pos,
                                                             std::shared_ptr<Velocity> v
                                                           ) {
      int dx = -mousex + win_width / 2, dy = -mousey + win_height / 2;
      v->x = static_cast<float>(dx) * 0.2f;
      v->y = static_cast<float>(dy) * 0.2f;

      // Boundary checks.
      if (v->x > 0.0f && pos->x - bg->width / 2 >= -5) v->x = 0.0f;
      if (v->x < 0.0f && pos->x + bg->width / 2 <= win_width + 5) v->x = 0.0f;
      if (v->y > 0.0f && pos->y - bg->height / 2 >= -5) v->y = 0.0f;
      if (v->y < 0.0f && pos->y + bg->height / 2 <= win_height + 5) v->y = 0.0f;
    });

    // Player fish.
    world->for_each<Player, Fish, Coordinate, Velocity, Collidable>(
      [this](
        Entity ent,
        std::shared_ptr<Player>,
        std::shared_ptr<Fish> fish,
        std::shared_ptr<Coordinate> pos,
        std::shared_ptr<Velocity> v,
        std::shared_ptr<Collidable> rect
      ) {
        // Calculate velocity towards mouse cursor.
        int dx = mousex - pos->x, dy = mousey - pos->y;
        v->x = static_cast<float>(dx) * fish->speed * 2.0f;
        v->y = static_cast<float>(dy) * fish->speed * 2.0f;

        // U-turn if needed.
        if ((fish->dir == FishDirection::Right && dx < 0) ||
            (fish->dir == FishDirection::Left && dx > 0))
          world->emit(FishTurned(ent));

        // Switch between idle and swim states based on velocity.
        if (fish->state == FishState::Swim &&
            (std::abs(v->x) < static_cast<float>(rect->width) / 2.0 &&
             std::abs(v->y) < static_cast<float>(rect->height) / 2.0))
          world->emit(FishStateChanged(ent, FishState::Idle));
        if (fish->state == FishState::Idle &&
            (std::abs(v->x) >= static_cast<float>(rect->width) / 2.0 ||
             std::abs(v->y) >= static_cast<float>(rect->height) / 2.0))
          world->emit(FishStateChanged(ent, FishState::Swim));
      }
    );
  } else {
    using namespace keycodes;

    std::array<std::unordered_map<int, size_t>, 2> keymaps;
    keymaps[0][W] = W;
    keymaps[0][S] = S;
    keymaps[0][A] = A;
    keymaps[0][D] = D;
    keymaps[0][SHIFT] = LSHIFT;
    keymaps[1][W] = I;
    keymaps[1][S] = K;
    keymaps[1][A] = J;
    keymaps[1][D] = L;
    keymaps[1][SHIFT] = RSHIFT;

    // Player fish.
    world->for_each<Player, Fish, Coordinate, Velocity, Collidable>(
      [&keymaps, this](
        Entity ent,
        std::shared_ptr<Player> player,
        std::shared_ptr<Fish> fish,
        std::shared_ptr<Coordinate> pos,
        std::shared_ptr<Velocity> v,
        std::shared_ptr<Collidable> rect
      ) {
        // Reset velocity first.
        v->x = 0.0f;
        v->y = 0.0f;

        if (keydown[keymaps[player->id][W]]) v->y = fish->speed * -200.0f;
        if (keydown[keymaps[player->id][S]]) v->y = fish->speed * 200.0f;
        if (keydown[keymaps[player->id][A]]) v->x = fish->speed * -200.0f;
        if (keydown[keymaps[player->id][D]]) v->x = fish->speed * 200.0f;
        // Speed boost with Shift.
        if (keydown[keymaps[player->id][SHIFT]]) {
          v->x *= 1.5f;
          v->y *= 1.5f;
        }

        // Boundary checks.
        if (v->x < 0.0f && pos->x - rect->width / 4 <= 0) v->x = 0.0f;
        if (v->x > 0.0f && pos->x + rect->width / 4 >= win_width) v->x = 0.0f;
        if (v->y < 0.0f && pos->y - rect->height / 4 <= 0) v->y = 0.0f;
        if (v->y > 0.0f && pos->y + rect->height / 4 >= win_height) v->y = 0.0f;

        // U-turn if needed.
        if ((fish->dir == FishDirection::Right && v->x < 0) ||
            (fish->dir == FishDirection::Left && v->x > 0))
          world->emit(FishTurned(ent));

        // Switch between idle and swim states based on velocity.
        if (fish->state == FishState::Swim && (v->x == 0.0f && v->y == 0.0f))
          world->emit(FishStateChanged(ent, FishState::Idle));
        if (fish->state == FishState::Idle && (v->x != 0.0f || v->y != 0.0f))
          world->emit(FishStateChanged(ent, FishState::Swim));
      }
    );
  }
}

void make_player(
  std::shared_ptr<World> world,
  Entity fish_ent,
  std::uint8_t id,
  int score,
  int health,
  int level,
  bool protection,
  float score_prog,
  float health_prog,
  float level_prog,
  const Font& font,
  int fontw,
  int fonth,
  int bar_width,
  int bar_height,
  int score_x,
  int score_y,
  int health_x,
  int health_y,
  int level_x,
  int level_y,
  int protection_x,
  int protection_y,
  int score_bar_x,
  int score_bar_y,
  int health_bar_x,
  int health_bar_y,
  int level_bar_x,
  int level_bar_y
) {
  Entity score_hud = make_text(
    world,
    std::format("P{} {:>3}pts", id, score),
    font,
    fontw,
    fonth,
    score_x,
    score_y,
    zindex::UI,
    Alignment::Center
  );
  Entity health_hud = make_text(
    world,
    std::format("HP {:>2}", health),
    font,
    fontw,
    fonth,
    health_x,
    health_y,
    zindex::UI,
    Alignment::Center
  );
  Entity level_hud = make_text(
    world,
    std::format("LV {:>2}", level),
    font,
    fontw,
    fonth,
    level_x,
    level_y,
    zindex::UI,
    Alignment::Center
  );
  Entity protection_hud = make_text(
    world,
    "shield",
    font,
    fontw,
    fonth,
    protection_x,
    protection_y,
    zindex::UI,
    Alignment::Center
  );
  // Colors chose from https://github.com/sainnhe/sonokai. :)
  Entity score_bar_hud = make_progress_bar(
    world,
    score_prog,
    bar_width,
    bar_height,
    RGB(0, 0, 0),
    RGB(158, 208, 114),
    score_bar_x,
    score_bar_y,
    zindex::UI,
    id == 0 ? Alignment::Left : Alignment::Right
  );
  Entity health_bar_hud = make_progress_bar(
    world,
    health_prog,
    bar_width,
    bar_height,
    RGB(0, 0, 0),
    RGB(252, 93, 124),
    health_bar_x,
    health_bar_y,
    zindex::UI,
    id == 0 ? Alignment::Left : Alignment::Right
  );
  Entity level_bar_hud = make_progress_bar(
    world,
    level_prog,
    bar_width,
    bar_height,
    RGB(0, 0, 0),
    RGB(118, 204, 224),
    level_bar_x,
    level_bar_y,
    zindex::UI,
    id == 0 ? Alignment::Left : Alignment::Right
  );
  world->add<Shown>(score_hud);
  world->add<Shown>(health_hud);
  world->add<Shown>(level_hud);
  if (protection) world->add<Shown>(protection_hud);
  world->add<Shown>(score_bar_hud);
  world->add<Shown>(health_bar_hud);
  world->add<Shown>(level_bar_hud);
  world->add<Player>(
    fish_ent,
    id,
    score,
    health,
    level,
    protection,
    score_hud,
    health_hud,
    level_hud,
    protection_hud,
    score_bar_hud,
    health_bar_hud,
    level_bar_hud
  );
}
