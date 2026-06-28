#include "timer.h"

void TimerSystem::reset() {
  world->for_each<Timer>([](Entity, std::shared_ptr<Timer> timer) { timer->reset(); });
}

void TimerSystem::update(float dt) {
  std::vector<Entity> used;

  world->for_each<Timer>([&used, dt, this](Entity ent, std::shared_ptr<Timer> timer) {
    timer->cur += dt;
    while (timer->cur >= 1.0f) {
      timer->tick();
      timer->cur -= 1.0f;

      // Check for countdown timer reaching zero.
      if (auto cd = std::dynamic_pointer_cast<CountdownTimer>(timer); cd)
        if (cd->min == 0 && cd->sec == 0) {
          world->emit(TimeUp(ent));
          if (!cd->reusable) {
            used.emplace_back(ent);
            break;
          }
        }
    }

    if (auto text = world->get<Text>(ent); text)
      text->text = std::format("{:02}:{:02}", timer->min, timer->sec);
  });

  for (auto ent : used) world->destroy(ent);
}

Entity make_countup_timer(
  std::shared_ptr<World> world,
  int min0,
  int sec0,
  bool shown,
  const Font& font,
  int fontw,
  int fonth,
  int x,
  int y,
  int zidx,
  Alignment align,
  float scale
) {
  Entity ent;
  if (shown)
    ent = make_text(
      world,
      std::format("{:02}:{:02}", min0, sec0),
      font,
      fontw,
      fonth,
      x,
      y,
      zidx,
      align,
      scale
    );
  else
    ent = world->make_entity();
  world->add_as<Timer, CountupTimer>(ent, min0, sec0);
  return ent;
}

Entity make_countdown_timer(
  std::shared_ptr<World> world,
  int min0,
  int sec0,
  bool reusable,
  bool shown,
  const Font& font,
  int fontw,
  int fonth,
  int x,
  int y,
  int zidx,
  Alignment align,
  float scale
) {
  Entity ent;
  if (shown)
    ent = make_text(
      world,
      std::format("{:02}:{:02}", min0, sec0),
      font,
      fontw,
      fonth,
      x,
      y,
      zidx,
      align,
      scale
    );
  else
    ent = world->make_entity();
  world->add_as<Timer, CountdownTimer>(ent, min0, sec0, reusable);
  return ent;
}
