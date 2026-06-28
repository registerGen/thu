// Timer component and system.

#pragma once

#include <chrono>

#include "ecs.h"
#include "resource.h"
#include "ui.h"

#ifdef min
# undef min
#endif

class TimeUp : public Event {
public:
  Entity ent;
  TimeUp(Entity ent) : ent(ent) { }
};

class Timer : public Component {
public:
  int min0;  // Initial min set.
  int sec0;  // Initial sec set.
  int min;
  int sec;
  float cur;
  Timer(int min0 = 0, int sec0 = 0) : min0(min0), sec0(sec0), min(min0), sec(sec0), cur(0.0f) { }
  ~Timer() = default;
  void reset() {
    min = min0;
    sec = sec0;
    cur = 0.0f;
  }
  virtual void tick() = 0;
};

class CountupTimer : public Timer {
public:
  using Timer::Timer;

  void tick() override {
    ++sec;
    if (sec >= 60) {
      ++min;
      sec = 0;
    }
  }
};

class CountdownTimer : public Timer {
public:
  bool reusable = false;  // Whether the timer can restart after time is up.

  using Timer::Timer;

  CountdownTimer(int min0 = 0, int sec0 = 0, bool reusable = false)
      : Timer(min0, sec0), reusable(reusable) { }

  void tick() override {
    --sec;
    if (sec < 0) {
      --min;
      sec = 59;
    }
  }
};

class TimerSystem : public System {
private:
  std::shared_ptr<World> world;

public:
  TimerSystem(std::shared_ptr<World> world) : world(world) { disable(); }

  // Reset all timers.
  void reset();

  void update(float dt) override;
};

Entity make_countup_timer(
  std::shared_ptr<World> world,
  int min0,
  int sec0,
  bool shown = false,
  const Font& font = {},
  int fontw = 0,
  int fonth = 0,
  int x = 0,
  int y = 0,
  int zidx = 0,
  Alignment align = Alignment::Left,
  float scale = 1.0f
);

Entity make_countdown_timer(
  std::shared_ptr<World> world,
  int min0,
  int sec0,
  bool reusable,
  bool shown = false,
  const Font& font = {},
  int fontw = 0,
  int fonth = 0,
  int x = 0,
  int y = 0,
  int zidx = 0,
  Alignment align = Alignment::Left,
  float scale = 1.0f
);
