cmake_minimum_required(VERSION 3.20)

project(crjson VERSION 0.1.0 LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
  option(CRJSON_DEV "dev targets" ON)
else()
  option(CRJSON_DEV "dev targets" OFF)
endif()

add_library(crjson INTERFACE)
add_library(crjson::crjson ALIAS crjson)

target_compile_features(crjson INTERFACE cxx_std_20)

include(GNUInstallDirs)
target_include_directories(crjson
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(TARGETS crjson EXPORT crjsonTargets)
install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
  EXPORT crjsonTargets
  FILE crjsonTargets.cmake
  NAMESPACE crjson::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/crjson
)

if(NOT PROJECT_IS_TOP_LEVEL)
  message(STATUS "Using crjson ${PROJECT_VERSION}")
endif()

if(CRJSON_DEV)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

  include(FetchContent)

  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.13.0
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(Catch2)
  if(TARGET Catch2)
    set_target_properties(Catch2 PROPERTIES CXX_STANDARD 20)
  endif()
  if(TARGET Catch2WithMain)
    set_target_properties(Catch2WithMain PROPERTIES CXX_STANDARD 20)
  endif()

  enable_testing()
  add_subdirectory(tests)

  if(NOT "$ENV{GITHUB_ACTIONS}" STREQUAL "true")
    # Course report
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/report")
      add_subdirectory(report)
    endif()

    # google/benchmark
    FetchContent_Declare(
      benchmark
      GIT_REPOSITORY https://github.com/google/benchmark.git
      GIT_TAG        v1.9.5
      GIT_SHALLOW    TRUE
    )
    set(BENCHMARK_ENABLE_TESTING     OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL     OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)

    # nlohmann/json
    FetchContent_Declare(
      nlohmann_json
      GIT_REPOSITORY https://github.com/nlohmann/json.git
      GIT_TAG        v3.12.0
      GIT_SHALLOW    TRUE
    )
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set(JSON_Install    OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(nlohmann_json)

    # simdjson
    FetchContent_Declare(
      simdjson
      GIT_REPOSITORY https://github.com/simdjson/simdjson.git
      GIT_TAG        v4.3.1
      GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(simdjson)

    # glaze
    FetchContent_Declare(
      glaze
      GIT_REPOSITORY https://github.com/stephenberry/glaze.git
      GIT_TAG        v7.2.0
      GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(glaze)

    # RapidJSON
    FetchContent_Declare(
      rapidjson_src
      GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
      GIT_TAG        v1.1.0
      GIT_SHALLOW    TRUE
    )
    FetchContent_GetProperties(rapidjson_src)
    if(NOT rapidjson_src_POPULATED)
      FetchContent_Populate(rapidjson_src)
    endif()
    add_library(rapidjson INTERFACE)
    target_include_directories(rapidjson INTERFACE ${rapidjson_src_SOURCE_DIR}/include)
  endif()  # Not in GitHub CI
endif()  # CRJSON_DEV
