# MinGW-w64 cross-compilation toolchain for Deepforge (Windows x86_64 target).
#
# Added by kanban task t_1e8e5fe7 to give the declared-but-never-built Windows
# target a locally reproducible compile path. This does NOT replace the MSVC
# build that .github/workflows/ci.yml runs on windows-2022 -- it is a second,
# cheaper gate that catches portability breakage without a Windows runner.
#
# Usage:
#   cmake -S . -B build/mingw-win64 -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake \
#         -DCMAKE_BUILD_TYPE=RelWithDebInfo
#
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(_triple x86_64-w64-mingw32)

set(CMAKE_C_COMPILER   ${_triple}-gcc)
set(CMAKE_CXX_COMPILER ${_triple}-g++)
set(CMAKE_RC_COMPILER  ${_triple}-windres)
set(CMAKE_AR           ${_triple}-ar)
set(CMAKE_RANLIB       ${_triple}-ranlib)

set(CMAKE_FIND_ROOT_PATH /usr/${_triple})

# Find host programs on the host, but libraries/headers only in the target root.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BEFORE)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
