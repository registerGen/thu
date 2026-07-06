#pragma once

/// Timer that sync with the game loop.
/// Can be started immediately or later, and can be reset to the initial duration.
/// If started later, the timer is considered finished until it is started.
/// Intended as a base class (e.g. Resource::AutoIncrease, StatusEffect), so it
/// has a virtual destructor.
class CountdownTimer {
  float duration_;
  float remaining_time_;

public:
  CountdownTimer(float duration, bool start_now)
      : duration_(duration), remaining_time_(start_now ? duration : 0.0f) { }
  virtual ~CountdownTimer() = default;

  void update(float dt) {
    if (remaining_time_ > 0.0f) {
      remaining_time_ -= dt;
      if (remaining_time_ <= 0.0f) {
        remaining_time_ = 0.0f;
      }
    }
  }

  void reset() { remaining_time_ = duration_; }
  bool is_finished() const { return remaining_time_ <= 0.0f; }
  float duration() const { return duration_; }
  float remaining_time() const { return remaining_time_; }
};
