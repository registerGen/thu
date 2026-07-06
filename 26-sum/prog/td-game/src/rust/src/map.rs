use crate::{geometry::Vec2, tile::Tile};

/// A map is a rectangular area that contains tiles, defining the playable area.
#[derive(Debug, Clone)]
pub struct Map {
    pub width: f32,
    pub height: f32,
    pub tiles: Vec<Tile>,
}

impl Map {
    pub fn new(width: f32, height: f32, tiles: Vec<Tile>) -> Self {
        Self {
            width,
            height,
            tiles,
        }
    }

    pub fn tile_at(&self, position: Vec2) -> Option<&Tile> {
        self.tiles
            .iter()
            .find(|tile| tile.bounds.contains(position))
    }

    pub fn tile_at_mut(&mut self, position: Vec2) -> Option<&mut Tile> {
        self.tiles
            .iter_mut()
            .find(|tile| tile.bounds.contains(position))
    }

    pub fn tile_with_index_at(&self, position: Vec2) -> Option<(usize, &Tile)> {
        self.tiles
            .iter()
            .enumerate()
            .find(|(_, tile)| tile.bounds.contains(position))
    }
}
