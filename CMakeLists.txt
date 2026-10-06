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
# `when` clause `=~` runs on brosearch's linear-time regex engine. Resolved like the other
# siblings: an existing `brosearch` target (a superbuild already added it) wins; otherwise the
# standalone repo beside the top-level project (../brosearch, overridable with
# -DBROSEARCH_DIR=<path>); otherwise the third_party/brosearch submodule of the top-level
# project. A consumer vendoring brokeys carries brosearch flat in its own third_party/.
if(NOT TARGET brosearch)
    set(BROSEARCH_DIR "${CMAKE_SOURCE_DIR}/../brosearch" CACHE PATH
        "Path to the standalone brosearch repo (set to a missing path to force the submodule)")
    set(BROSEARCH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BROSEARCH_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    if(EXISTS "${BROSEARCH_DIR}/CMakeLists.txt")
        message(STATUS "brokeys: brosearch from ${BROSEARCH_DIR}")
        add_subdirectory("${BROSEARCH_DIR}" "${CMAKE_BINARY_DIR}/brosearch" EXCLUDE_FROM_ALL)
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/brosearch/CMakeLists.txt")
        message(STATUS "brokeys: brosearch from ${CMAKE_SOURCE_DIR}/third_party/brosearch (submodule)")
        add_subdirectory("${CMAKE_SOURCE_DIR}/third_party/brosearch" "${CMAKE_BINARY_DIR}/brosearch"
                         EXCLUDE_FROM_ALL)
    else()
        message(FATAL_ERROR
            "brokeys needs brosearch (its regex engine): not found at ${BROSEARCH_DIR} or "
            "${CMAKE_SOURCE_DIR}/third_party/brosearch. Clone https://github.com/wlejon/brosearch "
            "next to this repo, run `git submodule update --init --recursive`, or pass "
            "-DBROSEARCH_DIR=<path>.")
    endif()
endif()

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
    # bronze, the JavaScript runtime the api binds: a checkout beside this one
    # (../bronze, with ../brass beside it — bronze resolves and
    # add_subdirectory()s its own code generator), the layout of a bro tree and
    # of CI (bronze's .github/actions/checkout-toolchain), or an explicit
    # -DBRONZE_DIR. No vendored fallback: the api is bound across bronze's C++
    # embed boundary, so it has to build against the bronze the surrounding
    # tree builds against. Under bro this block is skipped, bro having added
    # bronze already.
    if(NOT TARGET bronze_runtime_shared)
        set(BRONZE_BUILD_SHARED_RUNTIME ON CACHE BOOL "" FORCE)
        set(BRONZE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        if(DEFINED BRONZE_DIR AND EXISTS "${BRONZE_DIR}/CMakeLists.txt")
            # Explicit BRONZE_DIR supplied
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../bronze/CMakeLists.txt")
            set(BRONZE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../bronze" CACHE PATH "bronze source tree")
        elseif(EXISTS "${CMAKE_SOURCE_DIR}/../bronze/CMakeLists.txt")
            # Beside the top-level project: this repo is a third_party/
            # submodule of the project being built.
            set(BRONZE_DIR "${CMAKE_SOURCE_DIR}/../bronze" CACHE PATH "bronze source tree")
        endif()

        if(NOT BRONZE_DIR OR NOT EXISTS "${BRONZE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR
                "bronze not found: check it out beside this repository (../bronze, with ../brass beside it) "
                "or pass -DBRONZE_DIR=<path>.")
        endif()

        add_subdirectory("${BRONZE_DIR}" "${CMAKE_BINARY_DIR}/bronze-build" EXCLUDE_FROM_ALL)
    endif()

    add_subdirectory(src/api)
endif()

if(BROKEYS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

