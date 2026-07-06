# Framework-agnostic game model: a static library that Qt (or any UI) can link.
add_library(td_game STATIC
  bullet.cpp
  config.cpp
  enemy.cpp
  factory.cpp
  game.cpp
  level.cpp
  map.cpp
  path.cpp
  resource.cpp
  status_effect.cpp
  tile.cpp
  tower.cpp
  wave.cpp
)
add_library(td_project::game ALIAS td_game)

# Consumers (tests, Qt app) include headers as "game/<header>.h".
# Game headers include each other by bare name, resolved via same-directory
# lookup, so the game/ directory itself is on the include path too.
target_include_directories(td_game PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(td_game PUBLIC cxx_std_20)

# The config loader uses crjson (header-only, vendored at third_party/).
# It is included via a relative path from config.cpp, so no extra include dir
# is needed; nothing in the public headers exposes crjson.

if(MSVC)
  target_compile_options(td_game PRIVATE /W4 /permissive- /constexpr:steps2000000)
else()
  target_compile_options(td_game PRIVATE -Wall -Wextra -Wpedantic)
endif()
