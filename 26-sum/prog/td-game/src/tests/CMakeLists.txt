# Comprehensive unit + integration tests for the game model.
add_executable(td_tests
  test_config.cpp
  test_factory.cpp
  test_game.cpp
  test_geometry.cpp
  test_level.cpp
  test_path.cpp
  test_resource.cpp
  test_status_effect.cpp
  test_targeting.cpp
  test_timer.cpp
  test_wave.cpp
)

target_link_libraries(td_tests PRIVATE
  td_project::game
  Catch2::Catch2WithMain
)

# Let tests locate the runtime config data (towers.json, enemies.json, levels/).
target_compile_definitions(td_tests PRIVATE TD_CONFIG_DIR="${TD_CONFIG_DIR}")

if(MSVC)
  target_compile_options(td_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(td_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

catch_discover_tests(td_tests)
