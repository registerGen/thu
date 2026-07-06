# Qt6 application driving the td_game model.
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(td_app
  main.cpp
  GameController.cpp
  GameWidget.cpp
  MainWindow.cpp
  Screens.cpp
)

# game/ headers (game/views.h, game/geometry.h) are always needed for view
# types — they're header-only PODs with no link dependency. When the C++
# backend is OFF, td_project::game already provides this path; when ON (Rust
# backend, game/ not linked), add it explicitly. The source root (not game/
# itself) is needed because headers use the "game/" prefix (#include "game/views.h").
target_include_directories(td_app PRIVATE ${PROJECT_SOURCE_DIR})

if(TD_USE_RUST_MODEL)
  target_sources(td_app PRIVATE RustGameModel.cpp)
  target_compile_definitions(td_app PRIVATE TD_USE_RUST_MODEL)
  set_target_properties(td_app PROPERTIES OUTPUT_NAME "td-rs")
  target_link_libraries(td_app PRIVATE
    td_game_rs_cxxbridge
    td_project::game_rs
    Qt${QT_VERSION_MAJOR}::Widgets
  )
else()
  # The level editor feature is C++ only now.
  target_sources(td_app PRIVATE CppGameModel.cpp LevelEditor.cpp)
  set_target_properties(td_app PROPERTIES OUTPUT_NAME "td")
  target_link_libraries(td_app PRIVATE
    td_project::game
    Qt${QT_VERSION_MAJOR}::Widgets
  )
endif()

# The app locates config/ next to the executable at runtime, so bundle the
# config data into the runtime output directory.
add_custom_command(TARGET td_app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/../config
          $<TARGET_FILE_DIR:td_app>/config
  COMMENT "Bundling config/ next to the app"
)

# Publish the built app + config there on every build.
set(TD_BIN_DIR "${PROJECT_SOURCE_DIR}/../bin" CACHE PATH "Where to publish app + config")
add_custom_command(TARGET td_app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${TD_BIN_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:td_app> ${TD_BIN_DIR}/
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/../config ${TD_BIN_DIR}/config
  COMMENT "Publishing app + config to ${TD_BIN_DIR}"
)

if(MSVC)
  target_compile_options(td_app PRIVATE /W4 /permissive-)
else()
  target_compile_options(td_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Build a GUI subsystem executable on Windows so no console terminal pops up
# alongside the window. WIN32_EXECUTABLE sets /SUBSYSTEM:WINDOWS (CMake wraps
# it for the linker); /ENTRY:mainCRTStartup keeps the standard main() entry
# point (no WinMain). The clang++ cross-compile driver forwards the entry flag
# to lld-link via -Wl, (one token, passed verbatim); a native MSVC build takes
# /ENTRY directly.
if(WIN32)
  set_target_properties(td_app PROPERTIES WIN32_EXECUTABLE TRUE)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_link_options(td_app PRIVATE -Wl,/ENTRY:mainCRTStartup)
  else()
    target_link_options(td_app PRIVATE /ENTRY:mainCRTStartup)
  endif()
endif()
