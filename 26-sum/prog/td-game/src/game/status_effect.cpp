#include "status_effect.h"

#include "enemy.h"

StatusEffect::StatusEffect(float duration) : CountdownTimer(duration, true) { }
StatusEffect::~StatusEffect() = default;

void StatusEffect::on_apply(Enemy&) { }
void StatusEffect::update(Enemy&, float dt) { CountdownTimer::update(dt); }
bool StatusEffect::expired() const { return is_finished(); }
float StatusEffect::speed_multiplier() const { return 1.0f; }

SlowEffect::SlowEffect(float factor, float duration) : StatusEffect(duration), factor_(factor) { }

float SlowEffect::speed_multiplier() const { return factor_; }
StatusEffect::Type SlowEffect::type() const { return Type::Slow; }
void SlowEffect::scale_factor(float multiplier) { factor_ *= multiplier; }

PoisonEffect::PoisonEffect(int damage, float duration) : StatusEffect(duration), damage_(damage) { }

void PoisonEffect::on_apply(Enemy& enemy) { enemy.decrease_health(damage_); }
float PoisonEffect::speed_multiplier() const { return 0.0f; }
StatusEffect::Type PoisonEffect::type() const { return Type::Poison; }

RegenerationEffect::RegenerationEffect(int regen_amount, float duration)
    : StatusEffect(duration), regen_amount_(regen_amount) { }

StatusEffect::Type RegenerationEffect::type() const { return Type::Regen; }

void RegenerationEffect::update(Enemy& enemy, float dt) {
  StatusEffect::update(enemy, dt);

  // Increase enemy health when regen accumulates to an integer value (health can only be integers).
  accumulator_ += static_cast<float>(regen_amount_) * dt;
  int heal = static_cast<int>(accumulator_);
  if (heal > 0) {
    enemy.increase_health(heal);
    accumulator_ -= static_cast<float>(heal);
  }
}
