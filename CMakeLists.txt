cmake_minimum_required(VERSION 3.28)

project(Deepforge
    VERSION 0.4.0
    DESCRIPTION "Project Deepforge - a 2D mining progression game"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "" FORCE)
endif()

option(DEEPFORGE_BUILD_TESTS "Build the automated test suite" ON)
option(DEEPFORGE_BUILD_TOOLS "Build developer tools (content validator, balance simulator)" ON)
option(DEEPFORGE_BUILD_CLIENT "Build the SDL3 game client" ON)
option(DEEPFORGE_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)
option(DEEPFORGE_USE_SYSTEM_SDL "Use an installed SDL3 instead of fetching it" OFF)
option(DEEPFORGE_ENABLE_LTO "Enable interprocedural optimization (LTO) when the toolchain supports it" OFF)
set(DEEPFORGE_SANITIZER "" CACHE STRING "Sanitizer: address, undefined, thread or empty")

include(DeepforgeWarnings)
include(CheckIPOSupported)
include(CTest)

# LTO is opt-in per preset (the packaging build enables it). check_ipo_supported
# keeps toolchains without a working plugin (historically MinGW cross) from
# breaking the build: we simply fall back to no LTO and say so.
if(DEEPFORGE_ENABLE_LTO)
    check_ipo_supported(RESULT DEEPFORGE_LTO_SUPPORTED OUTPUT DEEPFORGE_LTO_CHECK_MSG)
    if(DEEPFORGE_LTO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
        message(STATUS "LTO: enabled")
    else()
        message(STATUS "LTO: requested but unsupported by this toolchain, disabled (${DEEPFORGE_LTO_CHECK_MSG})")
    endif()
endif()

# ---------------------------------------------------------------------------
# Sanitizers
# ---------------------------------------------------------------------------
if(DEEPFORGE_SANITIZER AND NOT MSVC)
    add_compile_options(-fsanitize=${DEEPFORGE_SANITIZER} -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=${DEEPFORGE_SANITIZER})
    if(DEEPFORGE_SANITIZER STREQUAL "undefined")
        add_compile_options(-fno-sanitize-recover=all)
    endif()
endif()

# ---------------------------------------------------------------------------
# Dependencies - every version is pinned. See docs/decisions/0002-dependencies.md
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

set(DEEPFORGE_JSON_VERSION "3.11.3")
FetchContent_Declare(deepforge_json
    URL "https://github.com/nlohmann/json/releases/download/v${DEEPFORGE_JSON_VERSION}/include.zip"
    URL_HASH SHA256=a22461d13119ac5c78f205d3df1db13403e58ce1bb1794edc9313677313f4a9d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

if(DEEPFORGE_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
        GIT_SHALLOW TRUE
    )
endif()

if(DEEPFORGE_BUILD_CLIENT AND NOT DEEPFORGE_USE_SYSTEM_SDL)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.16
        GIT_SHALLOW TRUE
    )
endif()

# nlohmann/json: header-only, consumed via an interface target.
if(NOT TARGET deepforge_json_headers)
    FetchContent_MakeAvailable(deepforge_json)
    add_library(deepforge_json_headers INTERFACE)
    target_include_directories(deepforge_json_headers SYSTEM INTERFACE
        "${deepforge_json_SOURCE_DIR}/include" "${deepforge_json_SOURCE_DIR}")
endif()

if(DEEPFORGE_BUILD_TESTS)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(Catch)
endif()

# Third-party include directories are marked SYSTEM so their own warnings never
# reach us. This is what makes DEEPFORGE_WARNINGS_AS_ERRORS usable in CI.
function(deepforge_mark_system target)
    if(NOT TARGET ${target})
        return()
    endif()
    get_target_property(_aliased ${target} ALIASED_TARGET)
    if(_aliased)
        set(target ${_aliased})
    endif()
    get_target_property(_type ${target} TYPE)
    if(_type STREQUAL "INTERFACE_LIBRARY")
        get_target_property(_dirs ${target} INTERFACE_INCLUDE_DIRECTORIES)
    else()
        get_target_property(_dirs ${target} INTERFACE_INCLUDE_DIRECTORIES)
    endif()
    if(_dirs)
        set_target_properties(${target} PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_dirs}")
    endif()
endfunction()

if(DEEPFORGE_BUILD_CLIENT)
    if(DEEPFORGE_USE_SYSTEM_SDL)
        find_package(SDL3 REQUIRED CONFIG)
    else()
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(SDL3)
    endif()
    foreach(_sdl_target SDL3::SDL3 SDL3::SDL3-shared SDL3::Headers SDL3::SDL3-static)
        deepforge_mark_system(${_sdl_target})
    endforeach()
endif()

# ---------------------------------------------------------------------------
# Version stamp
# ---------------------------------------------------------------------------
string(TIMESTAMP DEEPFORGE_BUILD_DATE "%Y-%m-%d" UTC)
set(DEEPFORGE_VERSION "${PROJECT_VERSION}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/Version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/core/Version.hpp"
    @ONLY
)

add_subdirectory(src)

if(DEEPFORGE_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

if(DEEPFORGE_BUILD_TESTS AND BUILD_TESTING)
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Packaging
# ---------------------------------------------------------------------------
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/" DESTINATION "share/deepforge/data")
# NOTE: no assets/ install rule — the directory does not exist in the repo
# (the old OPTIONAL rule silently installed nothing; removed by t_08f6def3).
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
              "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
              "${CMAKE_CURRENT_SOURCE_DIR}/docs/THIRD_PARTY_NOTICES.md"
        DESTINATION "share/deepforge")

set(CPACK_PACKAGE_NAME "Deepforge")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "deepforge-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
include(CPack)

message(STATUS "Deepforge ${PROJECT_VERSION} | client=${DEEPFORGE_BUILD_CLIENT} tests=${DEEPFORGE_BUILD_TESTS} tools=${DEEPFORGE_BUILD_TOOLS} sanitizer='${DEEPFORGE_SANITIZER}'")
