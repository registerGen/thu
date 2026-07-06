# Rust core: built by Cargo via Corrosion, exposed to CMake as a linkable
# static library. corrosion_add_cxxbridge runs the cxx codegen so C++ can
# #include the generated "td_game_rs_cxxbridge/ffi.h" and call into td::ffi::Game.
#
# Does NOT touch the existing game/ or app/ code — only adds the
# td-game-rs crate target and the cxx C++ bridge target.

# Import the crate from Cargo. IMPORTED_CRATES captures the generated target
# name (so we don't hard-code the crate's dash/underscore spelling).
#
# The crate's default feature is `web` (for the Wasm/browser build, which pulls
# in wasm-bindgen/tsify). The Qt/C++ build needs the `cxx-bridge` feature
# instead (it compiles src/cxxbridge.rs and runs the cxx codegen). So disable
# default features and enable only cxx-bridge here — this keeps wasm-bindgen/
# tsify out of the native staticlib and avoids dead-code warnings from web.rs.
corrosion_import_crate(
  MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
  IMPORTED_CRATES _td_crate_targets
  NO_DEFAULT_FEATURES
  FEATURES cxx-bridge
)
list(GET _td_crate_targets 0 _td_crate)
set(td_rust_crate "${_td_crate}" CACHE INTERNAL "imported td-game-rs crate target")

# Generate the cxx C++ header + runtime (cxx.cc). Creates the
# `td_game_rs_cxxbridge` interface target carrying the include directory.
corrosion_add_cxxbridge(
  td_game_rs_cxxbridge
  CRATE "${td_rust_crate}"
  FILES cxxbridge.rs
)

# Alias matching the td_project:: convention used elsewhere, so app/ can later
# link `td_project::game_rs`.
if(NOT TARGET td_project::game_rs)
  add_library(td_project::game_rs ALIAS "${td_rust_crate}")
endif()
