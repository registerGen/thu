cmake_minimum_required(VERSION 3.20)

project(td_project LANGUAGES CXX
  VERSION 0.1.0
  DESCRIPTION "2D grid-based tower defense (Qt + C++/Rust)"
)

# --- language settings --------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit compile_commands.json for clangd/IDE LSP on the C++ side.
# The Rust side uses rust-analyzer via Cargo.toml directly — no compile DB needed.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Keep build dirs out of the way; default to a single shared output.
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

# --- project-wide options -----------------------------------------------------
option(TD_USE_RUST_MODEL "Use the Rust model core"     OFF)
option(TD_BUILD_TESTS    "Build the Catch2 test suite" OFF)
option(TD_BUILD_APP      "Build the Qt application"    ON)
option(TD_COVERAGE       "Enable test coverage target" OFF)

# Local CMake modules (coverage.cmake, toolchains).
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Absolute path to the runtime config data, derived from the source tree (no
# hardcoded machine paths in source code). Tests and the future app use this to
# locate towers.json / enemies.json / levels/*.json.
set(TD_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config" CACHE PATH "Path to game config data")

# --- game model library (C++ backend) -----------------------------------------
# Built when the C++ backend is active, or when the Catch2 tests are enabled
# (the tests exercise the C++ model). Skipped entirely for a Rust-backend-only
# app build.
if(NOT TD_USE_RUST_MODEL OR TD_BUILD_TESTS)
  add_subdirectory(game)
endif()

# --- Rust core (optional) ----------------------------------------------------
# The Rust rewrite of the model core, built by Cargo via Corrosion and exposed
# to C++ through a cxx bridge. When TD_USE_RUST_MODEL is ON, app/ links the
# Rust core instead of the C++ model (see app/CMakeLists.txt).
if(TD_USE_RUST_MODEL)
  include(FetchContent)
  FetchContent_Declare(
    corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG        v0.6.1
  )
  FetchContent_MakeAvailable(corrosion)
  add_subdirectory(rust)
endif()

# --- tests --------------------------------------------------------------------
if(TD_BUILD_TESTS)
  find_package(Catch2 3.15.1 CONFIG QUIET)
  if(NOT Catch2_FOUND)
    include(FetchContent)
    FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG        v3.15.1
    )
    FetchContent_MakeAvailable(Catch2)
  endif()

  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
  enable_testing()
  include(Catch)
  add_subdirectory(tests)
endif()

# --- Qt application -----------------------------------------------------------
if(TD_BUILD_APP)
  add_subdirectory(app)
endif()

# --- test coverage ------------------------------------------------------------
# Applied last, after td_game/td_tests exist, so instrumentation can be scoped
# to just the code under test and the test driver (see cmake/coverage.cmake).
if(TD_COVERAGE)
  include(coverage)
  td_setup_coverage()
endif()
