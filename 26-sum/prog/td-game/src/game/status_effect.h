#pragma once

#include "timer.h"

class Enemy;

/// A status effect applied to an enemy (e.g. slow, poison, regen). Effects are
/// stored polymorphically on Enemy and ticked each frame; they expire automatically
/// when their timer runs out.
class StatusEffect : public CountdownTimer {
public:
  /// Which status this effect represents, for the render hint.
  enum class Type { Slow, Poison, Regen };

  virtual ~StatusEffect();

  /// Called once when the effect is first applied to the enemy.
  virtual void on_apply(Enemy&);
  /// Per-frame tick. The base implementation just counts down the timer.
  virtual void update(Enemy&, float dt);
  /// Whether the effect has run its course and should be removed.
  virtual bool expired() const;
  /// Whether the effect freezes the enemy in place (root/stun).
  virtual bool roots() const;
  /// Movement speed multiplier while active (<1 slows, 1 = no effect).
  virtual float speed_multiplier() const;
  /// Which status this effect represents (for rendering). Base returns None.
  virtual Type type() const = 0;

protected:
  explicit StatusEffect(float duration);
};

/// Reduces an enemy's movement speed for a duration.
class SlowEffect : public StatusEffect {
  float factor_;

public:
  SlowEffect(float factor, float duration);

  float speed_multiplier() const override;
  Type type() const override;
  /// Scale the slow factor (used by the slow tower to amplify the effect on ice).
  void scale_factor(float multiplier);
};

/// Deals one-shot damage on application and roots the enemy for a duration.
class PoisonEffect : public StatusEffect {
  int damage_;

public:
  PoisonEffect(int damage, float duration);

  void on_apply(Enemy& enemy) override;
  bool roots() const override;
  Type type() const override;
};

/// Heals the enemy over time for a duration (damage-over-time in reverse).
/// Used by the Boss's regeneration ability.
class RegenerationEffect : public StatusEffect {
  int heal_per_second_;
  float accumulator_ = 0.0f;

public:
  RegenerationEffect(int heal_per_second, float duration);

  void update(Enemy& enemy, float dt) override;
  Type type() const override;
};
