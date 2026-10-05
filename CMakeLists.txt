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

if(MSVC AND _brokeys_top_level)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

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

if(BROKEYS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
