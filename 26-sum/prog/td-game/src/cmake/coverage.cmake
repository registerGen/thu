# Test-coverage instrumentation and HTML report generation via gcov/lcov/genhtml.
#
# Activates when -DTD_COVERAGE=ON (and -DTD_BUILD_TESTS=ON) on a native
# GCC/Clang build. It is deliberately a no-op everywhere else, in particular
# under cross-compilation (the Windows clang/MSVC toolchain has no gcov
# runtime, so instrumenting it would fail to link).
#
# Usage:
#   cmake -S . -B build-cov -DTD_BUILD_TESTS=ON -DTD_COVERAGE=ON
#   cmake --build build-cov
#   cmake --build build-cov --target coverage
#   # open build-cov/coverage/index.html
#
# td_setup_coverage() is called once from the top-level CMakeLists *after* the
# game/tests targets exist, so the function can scope the instrumentation to
# exactly those two targets (the code under test and the test driver) rather
# than instrumenting the whole project.

# Instruments the code-under-test (td_game) and the test driver (td_tests) and
# registers a `coverage` target that runs the suite and writes an lcov report.
function(td_setup_coverage)
  if(NOT TD_BUILD_TESTS)
    message(FATAL_ERROR "TD_COVERAGE=ON requires -DTD_BUILD_TESTS=ON")
  endif()
  if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "TD_COVERAGE is not supported under cross-compilation "
                         "(use the native gcc/clang build, not the Windows toolchain)")
  endif()
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "TD_COVERAGE needs GCC or Clang (got ${CMAKE_CXX_COMPILER_ID})")
  endif()

  find_program(LCOV_PATH    lcov    REQUIRED)
  find_program(GENHTML_PATH genhtml REQUIRED)

  option(TD_COVERAGE_BRANCH "Include branch coverage in the report" ON)

  # --coverage == -fprofile-arcs -ftest-coverage (and the gcov runtime at link).
  # -O0 -g keeps line/branch attribution accurate; without it the optimizer
  # reorders/inlines code and the per-line hit counts become misleading.
  set(_cov_flags -O0 -g --coverage)

  if(TARGET td_game)
    target_compile_options(td_game PRIVATE ${_cov_flags})
  endif()
  if(TARGET td_tests)
    target_compile_options(td_tests PRIVATE ${_cov_flags})
    # The gcov runtime lives in the final executable, so the test driver must
    # link it for the library objects to flush their .gcda counters on exit.
    target_link_options(td_tests PRIVATE --coverage)
  endif()

  set(_info "${CMAKE_BINARY_DIR}/coverage.info")
  set(_out  "${CMAKE_BINARY_DIR}/coverage")

  set(_branch_args)
  if(TD_COVERAGE_BRANCH)
    set(_branch_args --rc branch_coverage=1)
  endif()

  # Whitelist the project's own model code rather than enumerating everything
  # to exclude (system headers, vendored Catch2/json, the test harness, fetched
  # deps). That stays correct no matter where Catch2 or the build tree lives.
  set(_keep_pattern ${CMAKE_SOURCE_DIR}/game/*)

  add_custom_target(coverage
    COMMAND ${CMAKE_COMMAND} -E echo "Resetting coverage counters..."
    COMMAND ${LCOV_PATH} --directory ${CMAKE_BINARY_DIR} --zerocounters
    COMMAND ${CMAKE_COMMAND} -E echo "Running the test suite..."
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    COMMAND ${CMAKE_COMMAND} -E echo "Capturing coverage data..."
    COMMAND ${LCOV_PATH} --directory ${CMAKE_BINARY_DIR} --capture
            ${_branch_args} --ignore-errors mismatch
            --output-file ${_info}
    COMMAND ${CMAKE_COMMAND} -E echo "Keeping only the game/ model sources..."
    COMMAND ${LCOV_PATH} --extract ${_info} ${_keep_pattern}
            ${_branch_args} --ignore-errors unused
            --output-file ${_info}
    COMMAND ${GENHTML_PATH} ${_info} ${_branch_args}
            --prefix ${CMAKE_SOURCE_DIR} --title "td_game coverage"
            --legend --output-directory ${_out}
    COMMAND ${CMAKE_COMMAND} -E echo "Done. Open the report: file://${_out}/index.html"
    DEPENDS td_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running tests and building the coverage report (${_out}/index.html)"
    VERBATIM
  )
endfunction()
