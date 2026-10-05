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
# standalone repo beside this one (../brosearch), overridable with -DBROSEARCH_DIR=<path>.
if(NOT TARGET brosearch)
    get_filename_component(_brosearch_default "${CMAKE_CURRENT_SOURCE_DIR}/../brosearch" ABSOLUTE)
    set(BROSEARCH_DIR "${_brosearch_default}" CACHE PATH "Path to the brosearch repo")
    if(NOT EXISTS "${BROSEARCH_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "brokeys needs brosearch: clone it beside brokeys (../brosearch) or pass -DBROSEARCH_DIR=<path>")
    endif()
    message(STATUS "brokeys: brosearch from ${BROSEARCH_DIR}")
    set(BROSEARCH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BROSEARCH_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${BROSEARCH_DIR}" "${CMAKE_BINARY_DIR}/brosearch" EXCLUDE_FROM_ALL)
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
