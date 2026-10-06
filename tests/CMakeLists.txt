cmake_minimum_required(VERSION 3.24)

set(TEST_TARGETS
    test_chords
    test_when_expr
    test_layout
    test_conflicts
    test_json
    test_engine
    test_trie_oracle
)

foreach(test_name IN LISTS TEST_TARGETS)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE brokeys::brokeys)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    brokeys_warnings(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

if(BROKEYS_ENABLE_API)
    add_executable(brokeys_test_api
        test_keys_api.cpp
    )
    target_include_directories(brokeys_test_api PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/api
        ${PROJECT_SOURCE_DIR}/src
    )
    target_link_libraries(brokeys_test_api PRIVATE
        brokeys_api
        brokeys
        bronze_runtime_shared
    )
    brokeys_warnings(brokeys_test_api)
    if(WIN32)
        target_compile_definitions(brokeys_test_api PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE _WIN32_WINNT=0x0A00)
        add_custom_command(TARGET brokeys_test_api POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:bronze_runtime_shared>
                $<TARGET_FILE_DIR:brokeys_test_api>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_LINKER_FILE:bronze_runtime_shared>
                $<TARGET_FILE_DIR:brokeys_test_api>
        )
    endif()

    if(NOT BRASS_HOST_BACKEND)
        message(STATUS "brokeys: brokeys_test_api not registered (brass has no code generator for this target)")
    else()
        add_test(NAME brokeys_test_api COMMAND brokeys_test_api)
        set_tests_properties(brokeys_test_api PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 77)

        add_test(NAME brokeys_test_api_gc_stress COMMAND brokeys_test_api)
        set_tests_properties(brokeys_test_api_gc_stress PROPERTIES
            TIMEOUT 60
            SKIP_RETURN_CODE 77
            ENVIRONMENT "BRONZE_GC_STRESS=1;BRONZE_GC_POISON=1"
        )
        if(WIN32)
            set_tests_properties(brokeys_test_api PROPERTIES
                ENVIRONMENT "PATH=$<TARGET_FILE_DIR:bronze_runtime_shared>;$<TARGET_FILE_DIR:brokeys_test_api>;$ENV{PATH}")
            set_property(TEST brokeys_test_api_gc_stress APPEND PROPERTY
                ENVIRONMENT "PATH=$<TARGET_FILE_DIR:bronze_runtime_shared>;$<TARGET_FILE_DIR:brokeys_test_api>;$ENV{PATH}")
        endif()
    endif()
endif()

# Every test executable links no_dialogs.cpp (see the file).
if(WIN32)
    get_property(_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_t IN LISTS _test_targets)
        get_target_property(_type ${_t} TYPE)
        if(_type STREQUAL "EXECUTABLE")
            target_sources(${_t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/no_dialogs.cpp)
        endif()
    endforeach()
endif()

