use crate::timer::CountdownTimer;

/// Resource hold by the player. There is only a single type of resource in the game.
#[derive(Debug, Clone)]
pub struct Resource {
    pub amount: i32,
    /// Automatically increase the resource amount over time.
    pub auto_inc_amount: i32,
    auto_inc_cooldown_timer: CountdownTimer,
}

impl Resource {
    /// Auto increase by `auto_inc_amount` per `auto_inc_interval` seconds.
    pub fn new(amount: i32, auto_inc_amount: i32, auto_inc_interval: f32) -> Self {
        Self {
            amount,
            auto_inc_amount,
            // The timer starts active so the first increase does not happen immediately.
            auto_inc_cooldown_timer: CountdownTimer::new(auto_inc_interval, true),
        }
    }

    pub fn increase(&mut self, amount: i32) {
        self.amount += amount;
    }

    /// Returns `true` if there was enough resource to decrease, false otherwise.
    pub fn decrease(&mut self, amount: i32) -> bool {
        if self.amount >= amount {
            self.amount -= amount;
            true
        } else {
            false
        }
    }

    /// Update the auto increase state.
    pub fn update(&mut self, dt: f32) {
        if self.auto_inc_cooldown_timer.update(dt) {
            self.increase(self.auto_inc_amount);
            self.auto_inc_cooldown_timer.reset();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn increase_decrease() {
        let mut r = Resource::new(100, 0, 1.0);
        assert_eq!(r.amount, 100);

        r.increase(50);
        assert_eq!(r.amount, 150);

        assert!(r.decrease(30));
        assert_eq!(r.amount, 120);

        // Cannot go below zero: decrease fails.
        assert!(!r.decrease(1000));
        assert_eq!(r.amount, 120); // unchanged on failure

        // Exact amount is allowed.
        assert!(r.decrease(120));
        assert_eq!(r.amount, 0);
    }

    #[test]
    fn auto_increase_over_time() {
        let mut r = Resource::new(0, 10, 2.0);
        assert_eq!(r.amount, 0);

        // Just under the interval: no increase yet.
        r.update(1.9);
        assert_eq!(r.amount, 0);

        // Cross the interval: +10 and the timer resets.
        r.update(0.2);
        assert_eq!(r.amount, 10);

        // Next interval.
        r.update(2.0);
        assert_eq!(r.amount, 20);

        // Partial progress toward the third interval does not add yet.
        r.update(1.0);
        assert_eq!(r.amount, 20);
    }

    #[test]
    fn auto_increase_accumulates_multiple_intervals() {
        let mut r = Resource::new(0, 5, 1.0);
        // 0.5s steps for 3.5s -> three 1-second intervals elapse -> +15.
        for _ in 0..7 {
            r.update(0.5);
        }
        assert_eq!(r.amount, 15);
    }
}
