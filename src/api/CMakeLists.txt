add_library(brokeys_api STATIC
    api.cpp
    host_class.cpp
    native_keys.cpp
)

target_include_directories(brokeys_api PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
)

target_link_libraries(brokeys_api PUBLIC
    brokeys
    bronze_runtime_shared
)

target_compile_features(brokeys_api PUBLIC cxx_std_20)

brokeys_warnings(brokeys_api)
