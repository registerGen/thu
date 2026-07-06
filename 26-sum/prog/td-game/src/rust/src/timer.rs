/// Timer that sync with the game loop.
/// Can be started immediately or later, and can be reset to the initial duration.
/// If started later, the timer is considered finished until it is started.
#[derive(Debug, Clone, Copy)]
pub struct CountdownTimer {
    pub duration: f32,
    remaining: f32,
}

impl CountdownTimer {
    pub fn new(duration: f32, start_now: bool) -> Self {
        Self {
            duration,
            remaining: if start_now { duration } else { 0.0 },
        }
    }

    /// Returns `true` if the timer is finished after the update.
    pub fn update(&mut self, dt: f32) -> bool {
        if self.remaining > 0.0 {
            self.remaining = (self.remaining - dt).max(0.0);
        }
        self.remaining <= 0.0
    }

    pub fn reset(&mut self) {
        self.remaining = self.duration;
    }

    pub fn is_finished(&self) -> bool {
        self.remaining <= 0.0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use crate::test_util::assert_approx_eq;

    #[test]
    fn started_immediately() {
        let mut t = CountdownTimer::new(2.0, true);
        assert!(!t.is_finished());
        assert_approx_eq!(t.remaining, 2.0);

        assert!(!t.update(0.5));
        assert_approx_eq!(t.remaining, 1.5);

        assert!(t.update(1.5));
        assert_approx_eq!(t.remaining, 0.0);
    }

    #[test]
    fn started_later_is_finished_until_started() {
        let mut t = CountdownTimer::new(1.0, false);
        assert!(t.is_finished()); // not started yet
        assert_approx_eq!(t.remaining, 0.0);

        assert!(t.update(5.0)); // no effect, not started — still finished
        assert_approx_eq!(t.remaining, 0.0);

        t.reset(); // start it
        assert!(!t.is_finished());
        assert_approx_eq!(t.remaining, 1.0);

        assert!(t.update(1.0));
    }

    #[test]
    fn reset_restarts_full_duration() {
        let mut t = CountdownTimer::new(3.0, true);
        assert!(!t.update(2.0));
        assert_approx_eq!(t.remaining, 1.0);

        t.reset();
        assert_approx_eq!(t.remaining, 3.0);
        assert!(!t.is_finished());
    }

    #[test]
    fn does_not_go_negative() {
        let mut t = CountdownTimer::new(1.0, true);
        assert!(t.update(10.0)); // over-advance
        assert_approx_eq!(t.remaining, 0.0);
    }

    #[test]
    fn duration_is_preserved() {
        let t = CountdownTimer::new(4.5, true);
        assert_approx_eq!(t.duration, 4.5);
    }
}
