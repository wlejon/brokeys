cmake_minimum_required(VERSION 3.24)

project(brokeys VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(_brokeys_top_level OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_brokeys_top_level ON)
endif()

option(BROKEYS_BUILD_TESTS "Build the brokeys test suite" ${_brokeys_top_level})

# Coverage. Scoped to a top-level build: a consumer that add_subdirectory()s
# brokeys has no business having its own build instrumented from in here.
option(BROKEYS_COVERAGE "Instrument the build for gcov coverage (GCC/Clang)" OFF)
if(BROKEYS_COVERAGE AND _brokeys_top_level)
    if(MSVC)
        message(FATAL_ERROR "BROKEYS_COVERAGE requires GCC or Clang (gcov instrumentation)")
    endif()
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

# The MSVC CRT stays CMake's default, the DLL one (/MD, /MDd), as in bro. The
# api's test links bronze_runtime_shared.dll, and objects allocated on one side
# of that boundary are freed on the other: with the static CRT every module has
# its own heap, so that is a cross-heap free (in Debug, the CRT's
# __acrt_first_block == header assert).

function(brokeys_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /MP)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# ---- brosearch -----------------------------------------------------------------
# `when` clause `=~` runs on brosearch's linear-time regex engine. Resolved through
# cmake/bro_deps.cmake: an existing `brosearch` target (a superbuild already added it) wins;
# otherwise the ../brosearch working tree beside the top-level project; otherwise the pinned
# commit, fetched at configure.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/bro_deps.cmake)
bro_dependency(brosearch GITHUB wlejon/brosearch REF e408e853181e353e086d547b6d1d3e9cc1c0167f
               OPTIONS BROSEARCH_BUILD_TESTS=OFF BROSEARCH_BUILD_TOOLS=OFF)

file(GLOB BROKEYS_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(brokeys STATIC ${BROKEYS_SOURCES})
add_library(brokeys::brokeys ALIAS brokeys)

target_include_directories(brokeys PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(brokeys PRIVATE brosearch)

brokeys_warnings(brokeys)

# --- Standalone Bronze JavaScript API ---
# On by default only when brokeys is the top-level project. A library that
# consumes brokeys then never needs bronze checked out; bro, the one consumer
# that wants the api, forces BROKEYS_ENABLE_API ON.
option(BROKEYS_ENABLE_API "Build Bronze JavaScript API" ${PROJECT_IS_TOP_LEVEL})
if(BROKEYS_ENABLE_API)
    # bronze (with brass, its code generator), the JavaScript runtime the api
    # binds, through cmake/bro_deps.cmake: an outer project's target (under bro
    # this is bro's own bronze), else ../bronze beside the top-level project,
    # else the pinned commit, fetched at configure.
    bro_dependency(bronze GITHUB wlejon/bronze REF 15ce4cf5fc25d8afe03882985ae95adcba46a6a9 TARGET bronze_runtime_shared
                   OPTIONS BRONZE_BUILD_SHARED_RUNTIME=ON BRONZE_BUILD_TESTS=OFF)

    add_subdirectory(src/api)
endif()

if(BROKEYS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

