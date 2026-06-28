# MIT License
#
# Copyright (c) 2026 registerGen
#
# Compile-time error tests for crjson library

cmake_minimum_required(VERSION 3.20)

project(crjson_ct_error LANGUAGES CXX)

if(NOT DEFINED CRJSON_SOURCE_DIR)
  message(FATAL_ERROR "CRJSON_SOURCE_DIR is required")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cases.cmake")

set(CRJSON_CT_ERROR_DIR "${CMAKE_BINARY_DIR}")
file(MAKE_DIRECTORY "${CRJSON_CT_ERROR_DIR}")

function(crjson_try_compile_fail name source)
  set(src "${CRJSON_CT_ERROR_DIR}/${name}.cpp")
  file(WRITE "${src}" "${source}")
  file(REMOVE_RECURSE "${CRJSON_CT_ERROR_DIR}/${name}-build")
  try_compile(result "${CRJSON_CT_ERROR_DIR}/${name}-build"
    SOURCES "${src}"
    CMAKE_FLAGS
      "-DCMAKE_CXX_STANDARD=20"
      "-DCMAKE_CXX_STANDARD_REQUIRED=ON"
      "-DCMAKE_CXX_EXTENSIONS=OFF"
  )
  if(result)
    message(FATAL_ERROR "Expected ${name} to fail compilation but it succeeded.")
  endif()
endfunction()

set(CRJSON_CT_PARSE_TEMPLATE [=[
#include "@CRJSON_HEADER@"
using namespace crjson;
constexpr auto doc = static_parse<@JSON@_fs>{};
int main() { (void)doc; return 0; }
]=])

set(CRJSON_CT_BIND_TEMPLATE [=[
#include "@CRJSON_HEADER@"
using namespace crjson;
using schema = @SCHEMA@;
constexpr static_bind<@JSON@_fs, schema> doc{};
int main() { (void)doc; return 0; }
]=])

foreach(case ${CRJSON_CT_PARSE_CASES})
  string(REPLACE "|" ";" pair "${case}")
  list(GET pair 0 name)
  list(GET pair 1 json)
  set(CRJSON_HEADER "${CRJSON_SOURCE_DIR}/include/crjson.h")
  set(JSON "${json}")
  string(CONFIGURE "${CRJSON_CT_PARSE_TEMPLATE}" source @ONLY)
  crjson_try_compile_fail("ct_parse_${name}" "${source}")
endforeach()

foreach(case ${CRJSON_CT_BIND_CASES})
  string(REPLACE "|" ";" pair "${case}")
  list(GET pair 0 name)
  list(GET pair 1 schema)
  list(GET pair 2 json)
  set(CRJSON_HEADER "${CRJSON_SOURCE_DIR}/include/crjson.h")
  set(SCHEMA "${schema}")
  set(JSON "${json}")
  string(CONFIGURE "${CRJSON_CT_BIND_TEMPLATE}" source @ONLY)
  crjson_try_compile_fail("ct_bind_${name}" "${source}")
endforeach()
