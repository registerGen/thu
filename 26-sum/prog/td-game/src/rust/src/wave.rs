use crate::{
    config::{EnemyConfigTable, make_enemy},
    enemy::Enemy,
    path::Path,
    timer::CountdownTimer,
};

/// A wave is a sequence of timed spawns, with prescribed path for each spawn.
pub type Wave = Vec<(String, f32, usize)>;

/// The wave schedule and the per-wave pre-delays.
/// `gaps[0]` elapses from game start to wave 0; `gaps[i]` (i>0) elapses between
/// the previous wave's last spawn and wave i.
#[derive(Debug, Clone)]
pub struct WaveSpec {
    pub waves: Vec<Wave>,
    pub gaps: Vec<f32>,
    pub current_wave: usize,
    pub all_done: bool,
    /// Seconds since the current wave started spawning.
    wave_time: f32,
    /// Which spawns of the current wave have fired.
    spawned: Vec<bool>,
    gap_timer: CountdownTimer,
    in_gap: bool,
}

impl WaveSpec {
    pub fn new(waves: Vec<Wave>, gaps: Vec<f32>) -> Self {
        assert_eq!(waves.len(), gaps.len());
        Self {
            all_done: waves.is_empty(), // put this first to pass the borrow checker
            waves,
            gaps,
            current_wave: 0,
            wave_time: 0.0,
            spawned: vec![],
            gap_timer: CountdownTimer::new(0.0, false),
            in_gap: false,
        }
    }

    pub fn current_wave_display(&self) -> i32 {
        if self.in_gap {
            self.current_wave as i32
        } else {
            (self.current_wave as i32 + 1).min(self.waves.len() as i32)
        }
    }

    pub fn start(&mut self) {
        if !self.waves.is_empty() {
            // Enter gap 0.
            self.in_gap = true;
            self.gap_timer = CountdownTimer::new(self.gaps[0], true);
        }
    }

    pub fn update(
        &mut self,
        dt: f32,
        enemy_configs: &EnemyConfigTable,
        paths: &[Path],
        mut spawn_enemy: impl FnMut(Enemy),
        mut emit_wave_started: impl FnMut(i32, bool, bool), // index, has_boss, is_last
    ) {
        if self.waves.is_empty() || self.all_done {
            return;
        }

        if self.in_gap {
            if self.gap_timer.update(dt) {
                self.in_gap = false;
                self.wave_time = 0.0;
                self.spawned.clear();
                self.spawned
                    .resize(self.waves[self.current_wave].len(), false);

                emit_wave_started(
                    self.current_wave as i32 + 1,
                    self.waves[self.current_wave]
                        .iter()
                        .any(|(kind, _, _)| kind == "boss"),
                    self.current_wave == self.waves.len() - 1,
                )
            }
        } else {
            self.wave_time += dt;

            let all_spawned = self.waves[self.current_wave]
                .iter()
                .zip(self.spawned.iter_mut())
                .fold(true, |acc, ((kind, time, path_index), spawned)| {
                    if !*spawned && self.wave_time >= *time {
                        if let Some(enemy) = make_enemy(kind, enemy_configs, *path_index, paths) {
                            spawn_enemy(enemy);
                        }
                        *spawned = true;
                    }
                    acc && *spawned
                });

            if all_spawned {
                self.current_wave += 1;
                if self.current_wave >= self.waves.len() {
                    self.all_done = true;
                } else {
                    self.in_gap = true;
                    self.gap_timer = CountdownTimer::new(self.gaps[self.current_wave], true);
                }
            }
        }
    }
}
