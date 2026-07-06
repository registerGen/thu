use crate::geometry::{Rect, Vec2};

/// A tile is a grid square on the map. Any tower may be placed on any non-rock,
/// non-portal tile — including path tiles (a tower on the path blocks enemies,
/// who will stop and attack it until it is destroyed). Terrain is encoded as
/// factor flags set by the level loader (grass / fertile / rock / ice / portal).
#[derive(Debug, Clone)]
pub struct Tile {
    pub bounds: Rect,
    /// Fertile: <1 (cheaper towers), else 1
    pub resource_cost_factor: f32,
    /// Grass/fertile/ice: true; rock/portal: false
    pub placeable: bool,
    /// Ice: >1 (faster), else 1
    pub enemy_speed_factor: f32,
    /// Ice: <1 (stronger slow), else 1
    pub slow_bullet_factor: f32,
    /// Teleports enemies to the paired portal
    pub is_portal: bool,
}

impl Tile {
    pub fn new(
        bounds: Rect,
        resource_cost_factor: f32,
        placeable: bool,
        enemy_speed_factor: f32,
        slow_bullet_factor: f32,
        is_portal: bool,
    ) -> Self {
        Self {
            bounds,
            resource_cost_factor,
            placeable,
            enemy_speed_factor,
            slow_bullet_factor,
            is_portal,
        }
    }

    /// Reverse of tile_from_terrain: infer the terrain name from a Tile's factors.
    pub fn terrain_name(&self) -> &'static str {
        if self.is_portal {
            "portal"
        } else if !self.placeable {
            "rock"
        } else if self.enemy_speed_factor > 1.0 {
            "ice"
        } else if self.resource_cost_factor < 1.0 {
            "fertile"
        } else {
            "grass"
        }
    }

    pub fn position(&self) -> Vec2 {
        self.bounds.center
    }
    pub fn can_place_tower(&self) -> bool {
        self.placeable && !self.is_portal
    }
}
