if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Target system
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Compiler
set(CMAKE_C_COMPILER clang-23)
set(CMAKE_CXX_COMPILER clang++-23)
set(CMAKE_RC_COMPILER llvm-rc-23)

# Target triple
set(TARGET_TRIPLE x86_64-pc-windows-msvc)
set(CMAKE_C_COMPILER_TARGET ${TARGET_TRIPLE})
set(CMAKE_CXX_COMPILER_TARGET ${TARGET_TRIPLE})

# Use lld-link (correct for MSVC target — don't set -fuse-ld manually,
# CMake's Windows-Clang module handles it)
set(CMAKE_LINKER lld-link-23)

# xwin root
set(XWIN_ROOT /opt/xwin)

# Include paths
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} \
  -isystem ${XWIN_ROOT}/crt/include \
  -isystem ${XWIN_ROOT}/sdk/include/ucrt \
  -isystem ${XWIN_ROOT}/sdk/include/shared \
  -isystem ${XWIN_ROOT}/sdk/include/um")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
  -isystem ${XWIN_ROOT}/crt/include \
  -isystem ${XWIN_ROOT}/sdk/include/ucrt \
  -isystem ${XWIN_ROOT}/sdk/include/shared \
  -isystem ${XWIN_ROOT}/sdk/include/um")

# Use static CRT instead of DLL CRT
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_MT -Xclang --dependent-lib=libcmt")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_MT -Xclang --dependent-lib=libcmt")

# Library paths — must use /libpath: for lld-link, not -L
set(XWIN_LIBPATHS
  "-Xlinker /libpath:${XWIN_ROOT}/crt/lib/x86_64"
  "-Xlinker /libpath:${XWIN_ROOT}/sdk/lib/ucrt/x86_64"
  "-Xlinker /libpath:${XWIN_ROOT}/sdk/lib/um/x86_64"
)
string(JOIN " " XWIN_LIBPATHS_STR ${XWIN_LIBPATHS})
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${XWIN_LIBPATHS_STR}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${XWIN_LIBPATHS_STR}")

# Sysroot isolation
set(CMAKE_FIND_ROOT_PATH ${XWIN_ROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
