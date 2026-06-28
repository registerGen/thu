#include "ui.h"

void AnimationSystem::update(float dt) {
  std::vector<Entity> finished;

  world->for_each<Animation, Shown, Image>([&finished, dt](
                                             Entity ent,
                                             std::shared_ptr<Animation> anim,
                                             std::shared_ptr<Shown>,
                                             std::shared_ptr<Image> img
                                           ) {
    if (anim->imgs.size() == 0) return;

    anim->cur += dt;

    // Advance frames as needed.
    while (anim->cur >= anim->elapse) {
      anim->cur -= anim->elapse;
      ++anim->frame;

      // Loop or finish.
      if (anim->frame == anim->imgs.size()) {
        anim->frame = 0;
        if (anim->once == true) {
          finished.emplace_back(ent);
          break;
        }
      }
    }

    img->id = anim->imgs[anim->frame];
  });

  for (auto ent : finished) {
    world->remove<Shown>(ent);
    world->emit(AnimationDone(ent));
  }
}

void FlashSystem::update(float dt) {
  std::vector<Entity> finished, used;

  world->for_each<Flash, Shown>(
    [&finished, &used, dt](Entity ent, std::shared_ptr<Flash> flash, std::shared_ptr<Shown>) {
      flash->cur += dt;

      // Check if flash is done.
      if (flash->cur >= flash->elapse) {
        finished.emplace_back(ent);
        if (!flash->reusable) used.emplace_back(ent);  // Mark for destruction.
      }
    }
  );

  for (auto ent : finished) {
    world->remove<Shown>(ent);
    world->emit(FlashDone(ent));
  }

  for (auto ent : used) world->destroy(ent);
}

void ButtonSystem::on_hover(const MouseMoved& ev) {
  world->for_each<Button, Shown, Image, Coordinate>([this, ev](
                                                      Entity ent,
                                                      std::shared_ptr<Button> btn,
                                                      std::shared_ptr<Shown>,
                                                      std::shared_ptr<Image> img,
                                                      std::shared_ptr<Coordinate> pos
                                                    ) {
    int width = btn->width, height = btn->height;
    auto transform = world->get<ImageTransform>(ent);

    // Apply scaling if any.
    if (transform && transform->scale != 1.0f) {
      width = static_cast<int>((float)width * transform->scale);
      height = static_cast<int>((float)height * transform->scale);
    }

    // Switch image based on hover state.
    if ((pos->x - width / 2 <= ev.x && ev.x <= pos->x + width / 2) &&
        (pos->y - height / 2 <= ev.y && ev.y <= pos->y + height / 2)) {
      if (img->id != btn->hover) sm->play_once(hover_sound);

      img->id = btn->hover;
    } else
      img->id = btn->normal;
  });
}

void ButtonSystem::on_click(const LeftMouseDown& ev) {
  world->for_each<Button, Shown, Image, Coordinate>([this, ev](
                                                      Entity ent,
                                                      std::shared_ptr<Button> btn,
                                                      std::shared_ptr<Shown>,
                                                      std::shared_ptr<Image>,
                                                      std::shared_ptr<Coordinate> pos
                                                    ) {
    int width = btn->width, height = btn->height;
    auto transform = world->get<ImageTransform>(ent);

    // Apply scaling if any.
    if (transform && transform->scale != 1.0f) {
      width = static_cast<int>((float)width * transform->scale);
      height = static_cast<int>((float)height * transform->scale);
    }

    // Check if clicked inside button area.
    if ((pos->x - width / 2 <= ev.x && ev.x <= pos->x + width / 2) &&
        (pos->y - height / 2 <= ev.y && ev.y <= pos->y + height / 2)) {
      sm->play_once(click_sound);
      btn->on_click(world);
    }
  });
}

Entity make_image(
  std::shared_ptr<World> world,
  ImageId id,
  int x,
  int y,
  int zidx,
  float scale,
  bool mirror
) {
  Entity ent = world->make_entity();
  world->add<Coordinate>(ent, x, y);
  world->add<Image>(ent, id, zidx);
  world->add<ImageTransform>(ent, scale, mirror);
  return ent;
}

Entity make_text(
  std::shared_ptr<World> world,
  const std::string& text,
  const Font& font,
  int fontw,
  int fonth,
  int x,
  int y,
  int zidx,
  Alignment align,
  float scale
) {
  Entity ent = world->make_entity();
  world->add<Coordinate>(ent, x, y);
  world->add<Image>(ent, ImageType::IMG_TEXT, zidx);
  world->add<Text>(ent, text, font, fontw, fonth, align);
  world->add<ImageTransform>(ent, scale, false);
  return ent;
}

Entity make_progress_bar(
  std::shared_ptr<World> world,
  float percent,
  int width,
  int height,
  COLORREF draw,
  COLORREF fill,
  int x,
  int y,
  int zidx,
  Alignment align
) {
  Entity ent = world->make_entity();
  world->add<Coordinate>(ent, x, y);
  world->add<Image>(ent, ImageType::IMG_PROGBAR, zidx);
  world->add<ProgressBar>(ent, percent, width, height, draw, fill, align);
  return ent;
}

Entity make_animation(
  std::shared_ptr<World> world,
  const std::vector<ImageId>& ids,
  int x,
  int y,
  int zidx,
  float elapse,
  bool once,
  float scale,
  bool mirror,
  size_t frame
) {
  if (ids.empty()) return 0;
  Entity ent = world->make_entity();
  world->add<Coordinate>(ent, x, y);
  world->add<Image>(ent, ids[frame], zidx);
  world->add<Animation>(ent, ids, frame, elapse, once);
  world->add<ImageTransform>(ent, scale, mirror);
  return ent;
}

Entity make_flash(
  std::shared_ptr<World> world,
  ImageId id,
  int x,
  int y,
  int zidx,
  float elapse,
  bool reusable,
  float scale,
  bool mirror
) {
  Entity ent = world->make_entity();
  world->add<Coordinate>(ent, x, y);
  world->add<Image>(ent, id, zidx);
  world->add<Flash>(ent, elapse, reusable);
  world->add<ImageTransform>(ent, scale, mirror);
  return ent;
}

Entity make_button(
  std::shared_ptr<World> world,
  ImageId id_normal,
  ImageId id_hover,
  int x,
  int y,
  int zidx,
  int width,
  int height,
  std::function<void(std::shared_ptr<World>)> on_click,
  float scale,
  bool mirror
) {
  Entity ent = world->make_entity();
  world->add<Coordinate>(ent, x, y);
  world->add<Image>(ent, id_normal, zidx);
  world->add<Button>(ent, id_normal, id_hover, width, height, on_click);
  world->add<ImageTransform>(ent, scale, mirror);
  return ent;
}
