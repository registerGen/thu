cmake_minimum_required(VERSION 3.20)
project(FishGame CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h")
file(GLOB RESOURCES "*.rc")

add_executable(FishGame WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})

# Link Windows libraries
target_link_libraries(FishGame
  PRIVATE
  kernel32
  user32
  gdi32
  winmm
  shell32
  ucrt
  msvcrt
)

target_compile_definitions(FishGame PRIVATE UNICODE _UNICODE)

# Release build only
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
set_target_properties(FishGame PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Release"
)
