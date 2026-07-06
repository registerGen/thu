//! Rust rewrite of the `game/` model core. Framework-agnostic.

// The core's `pub` items live in private modules, so they read as "dead code"
// unless an in-crate adapter consumes them. With `cxx-bridge` that's `cxxbridge.rs`;
// for the `web` build it'll be the wasm adapter. Silence the false positive
// when no cxx bridge is present.
#![cfg_attr(not(feature = "cxx-bridge"), allow(dead_code))]

mod bullet;
mod config;
mod enemy;
mod game;
mod geometry;
mod level;
mod map;
mod path;
mod resource;
mod status_effect;
mod tile;
mod timer;
mod tower;
mod wave;

#[cfg(test)]
mod test_util;

#[cfg(feature = "cxx-bridge")]
mod cxxbridge;

#[cfg(feature = "web")]
mod web;
