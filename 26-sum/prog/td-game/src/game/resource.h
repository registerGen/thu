#pragma once

#include "timer.h"

/// Resource hold by the player. There is only a single type of resource in the game.
class Resource {
public:
  /// Automatically increase the resource amount over time. The timer starts
  /// active, so the first increase happens after `duration` (not immediately).
  struct AutoIncrease : public CountdownTimer {
    int amount;
    AutoIncrease(int amount, float duration);
  };

private:
  int amount_;
  AutoIncrease auto_increase_;

public:
  Resource(int amount, AutoIncrease const& auto_increase);

  int amount() const;
  void increase(int amount);
  /// Return true if there was enough resource to decrease, false otherwise.
  bool decrease(int amount);
  /// Update the auto increase state.
  void update(float dt);
};
