#include "resource.h"

Resource::AutoIncrease::AutoIncrease(int amount, float duration)
    : CountdownTimer(duration, true), amount(amount) { }

Resource::Resource(int amount, AutoIncrease const& auto_increase)
    : amount_(amount), auto_increase_(auto_increase) { }

int Resource::amount() const { return amount_; }
void Resource::increase(int amount) { amount_ += amount; }
bool Resource::decrease(int amount) {
  if (amount_ >= amount) {
    amount_ -= amount;
    return true;
  }
  return false;
}

void Resource::update(float dt) {
  auto_increase_.update(dt);
  if (auto_increase_.is_finished()) {
    increase(auto_increase_.amount);
    auto_increase_.reset();
  }
}
