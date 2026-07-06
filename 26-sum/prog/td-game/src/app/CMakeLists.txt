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
  LevelEditor.cpp
  MainWindow.cpp
  Screens.cpp
)

target_link_libraries(td_app PRIVATE
  td_project::game
  Qt${QT_VERSION_MAJOR}::Widgets
)

# The app locates config/ next to the executable at runtime, so bundle the
# config data into the runtime output directory.
add_custom_command(TARGET td_app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/../config
          $<TARGET_FILE_DIR:td_app>/config
  COMMENT "Bundling config/ next to td_app"
)

# Publish the built app + config there on every build.
set(TD_BIN_DIR "${PROJECT_SOURCE_DIR}/../bin" CACHE PATH "Where to publish td_app + config")
add_custom_command(TARGET td_app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${TD_BIN_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:td_app> ${TD_BIN_DIR}/
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/../config ${TD_BIN_DIR}/config
  COMMENT "Publishing td_app + config to ${TD_BIN_DIR}"
)

if(MSVC)
  target_compile_options(td_app PRIVATE /W4 /permissive-)
else()
  target_compile_options(td_app PRIVATE -Wall -Wextra -Wpedantic)
endif()
