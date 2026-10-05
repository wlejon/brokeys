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
