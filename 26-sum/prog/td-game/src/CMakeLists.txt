cmake_minimum_required(VERSION 3.20)

project(td_project LANGUAGES CXX
  VERSION 0.1.0
  DESCRIPTION "2D grid-based tower defense (Qt + C++)"
)

# --- language settings --------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep build dirs out of the way; default to a single shared output.
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

# --- project-wide options -----------------------------------------------------
option(TD_BUILD_TESTS "Build the Catch2 test suite"              OFF)
option(TD_BUILD_APP   "Build the Qt application"                 ON)
option(TD_COVERAGE    "Enable test coverage + `coverage` target" OFF)

# Local CMake modules (coverage.cmake, toolchains).
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Absolute path to the runtime config data, derived from the source tree (no
# hardcoded machine paths in source code). Tests and the future app use this to
# locate towers.json / enemies.json / levels/*.json.
set(TD_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config" CACHE PATH "Path to game config data")

# --- game model library -------------------------------------------------------
add_subdirectory(game)

# --- tests --------------------------------------------------------------------
if(TD_BUILD_TESTS)
  # Prefer a system-installed Catch2 3.15.1; fall back to FetchContent so the
  # project is self-contained on machines without it.
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
