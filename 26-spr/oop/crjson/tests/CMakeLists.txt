add_executable(tests test.cpp)
target_link_libraries(tests PRIVATE crjson Catch2::Catch2WithMain)

if(NOT "$ENV{GITHUB_ACTIONS}" STREQUAL "true")
  # Build coverage report
  target_compile_options(tests PRIVATE --coverage -O0 -g -fprofile-arcs -ftest-coverage)
  target_link_options(tests PRIVATE --coverage)

  find_program(LCOV lcov REQUIRED)
  find_program(GENHTML genhtml REQUIRED)

  add_custom_target(coverage
    DEPENDS tests
    COMMAND ${LCOV} --zerocounters --directory ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --output-on-failure

    COMMAND ${LCOV}
            --capture
            --rc branch_coverage=1
            --directory ${CMAKE_BINARY_DIR}
            --include "${CMAKE_SOURCE_DIR}/include/crjson.h"
            --output-file ${CMAKE_BINARY_DIR}/tests/coverage.info

    COMMAND ${GENHTML} ${CMAKE_BINARY_DIR}/tests/coverage.info
            --branch-coverage
            --output-directory ${CMAKE_BINARY_DIR}/tests/coverage_html
            --title "crjson coverage"

    COMMAND ${LCOV}
            --rc branch_coverage=1
            --list ${CMAKE_BINARY_DIR}/tests/coverage.info

    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running tests and generating coverage report..."
    VERBATIM
  )
endif()

if(NOT "$ENV{GITHUB_ACTIONS}" STREQUAL "true")
  add_executable(bench bench.cpp)
  target_link_libraries(bench PRIVATE
    benchmark::benchmark
    crjson
    nlohmann_json::nlohmann_json
    simdjson::simdjson
    rapidjson
    glaze::glaze)
  target_compile_options(bench PRIVATE -O2)
endif()

include(CTest)
include(Catch)
catch_discover_tests(tests)

add_test(
  NAME "compile-time parsing/schema errors"
  COMMAND ${CMAKE_COMMAND}
          -S ${CMAKE_SOURCE_DIR}/tests/ct_error
          -B ${CMAKE_BINARY_DIR}/ct_error
          -DCRJSON_SOURCE_DIR=${CMAKE_SOURCE_DIR}
          -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
          -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
)
