if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER   clang-23)
set(CMAKE_CXX_COMPILER clang++-23)
set(CMAKE_RC_COMPILER  llvm-rc-23)
set(CMAKE_LINKER       lld-link-23)

set(TARGET_TRIPLE x86_64-pc-windows-msvc)
set(CMAKE_C_COMPILER_TARGET   ${TARGET_TRIPLE})
set(CMAKE_CXX_COMPILER_TARGET ${TARGET_TRIPLE})

# Static CRT — this is the key variable Qt actually checks
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "" FORCE)

# Prevent CMake from trying to run .exe during feature detection
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY CACHE STRING "" FORCE)

set(XWIN_ROOT /opt/xwin)

# Use CACHE FORCE to prevent Qt from doubling these up
set(CMAKE_C_FLAGS "\
  -isystem /usr/lib/llvm-23/lib/clang/23/include \
  -isystem /opt/xwin/crt/include \
  -isystem /opt/xwin/sdk/include/ucrt \
  -isystem /opt/xwin/sdk/include/shared \
  -isystem /opt/xwin/sdk/include/winrt \
  -isystem /opt/xwin/sdk/include/um \
  -D_MT -Xclang --dependent-lib=libcmt"
  CACHE STRING "" FORCE)

set(CMAKE_CXX_FLAGS "\
  -isystem /usr/lib/llvm-23/lib/clang/23/include \
  -isystem /opt/xwin/crt/include \
  -isystem /opt/xwin/sdk/include/ucrt \
  -isystem /opt/xwin/sdk/include/shared \
  -isystem /opt/xwin/sdk/include/winrt \
  -isystem /opt/xwin/sdk/include/um \
  -D_MT -Xclang --dependent-lib=libcmt"
  CACHE STRING "" FORCE)

set(CMAKE_RC_FLAGS "\
  -I /opt/xwin/sdk/include/um \
  -I /opt/xwin/sdk/include/shared \
  -I /opt/xwin/sdk/include/ucrt \
  -I /opt/xwin/crt/include"
  CACHE STRING "" FORCE)

set(XWIN_LIBPATHS "\
  -Xlinker /libpath:${XWIN_ROOT}/crt/lib/x86_64 \
  -Xlinker /libpath:${XWIN_ROOT}/sdk/lib/ucrt/x86_64 \
  -Xlinker /libpath:${XWIN_ROOT}/sdk/lib/um/x86_64")

set(CMAKE_EXE_LINKER_FLAGS    "${XWIN_LIBPATHS}" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "${XWIN_LIBPATHS}" CACHE STRING "" FORCE)

set(CMAKE_FIND_ROOT_PATH ${XWIN_ROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Prevent Threads::Threads from adding -lpthread on Windows target
set(CMAKE_THREAD_LIBS_INIT "" CACHE STRING "" FORCE)
set(CMAKE_USE_WIN32_THREADS_INIT 1 CACHE INTERNAL "" FORCE)
set(Threads_FOUND TRUE CACHE BOOL "" FORCE)

# Create a dummy Threads::Threads target that links nothing.
# IMPORTED_GLOBAL so Qt's qt_find_package skips promoting it from a subdirectory
# (which would fail with "not built in this directory").
if(NOT TARGET Threads::Threads)
  add_library(Threads::Threads INTERFACE IMPORTED)
  set_target_properties(Threads::Threads PROPERTIES
    INTERFACE_LINK_LIBRARIES ""
    INTERFACE_COMPILE_OPTIONS ""
    IMPORTED_GLOBAL TRUE
  )
endif()

# For Qt's own bundled dependencies
set(CMAKE_FIND_ROOT_PATH
  /opt/qt6-xwin
  /opt/xwin
)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

set(QT_HOST_PATH "$ENV{HOME}/.local/qt6-host" CACHE PATH "" FORCE)
set(QT_HOST_PATH_CMAKE_DIR "$ENV{HOME}/.local/qt6-host/lib/cmake" CACHE PATH "" FORCE)
