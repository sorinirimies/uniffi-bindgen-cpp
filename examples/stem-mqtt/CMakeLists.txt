# stem-mqtt (https://github.com/sorinirimies/stem-mqtt) from C++: an MQTT broker and
# client written in Rust, each its own UniFFI library, used together from one program.
#
#   git clone https://github.com/sorinirimies/stem-mqtt
#   cmake -S examples/stem-mqtt -B build/mqtt -DSTEM_MQTT_DIR=<checkout> \
#         -DUNIFFI_BINDGEN_CPP=<path to uniffi-bindgen-cpp>
#   cmake --build build/mqtt && ctest --test-dir build/mqtt
cmake_minimum_required(VERSION 3.19)
project(stem_mqtt_cpp CXX)

set(STEM_MQTT_DIR "" CACHE PATH "A checkout of stem-mqtt")
if(NOT STEM_MQTT_DIR)
    message(FATAL_ERROR "Set STEM_MQTT_DIR to a checkout of https://github.com/sorinirimies/stem-mqtt")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../../crates/uniffi-bindgen-cpp/cmake/UniffiCpp.cmake)

uniffi_cargo_library(mqtt_client_rust
    MANIFEST ${STEM_MQTT_DIR}/Cargo.toml PACKAGE stem-mqtt-client NAME mqtt_client)
uniffi_cargo_library(mqtt_broker_rust
    MANIFEST ${STEM_MQTT_DIR}/Cargo.toml PACKAGE stem-mqtt-broker NAME mqtt_broker)
uniffi_cpp_bindings(mqtt_client RUST mqtt_client_rust COMPONENTS mqtt_client
    CRATE_DIR ${STEM_MQTT_DIR}/crates/mqtt-client)
uniffi_cpp_bindings(mqtt_broker RUST mqtt_broker_rust COMPONENTS mqtt_broker
    CRATE_DIR ${STEM_MQTT_DIR}/crates/mqtt-broker)

add_executable(mqtt_demo main.cpp)
target_link_libraries(mqtt_demo PRIVATE mqtt_client mqtt_broker)
if(WIN32)
    add_custom_command(TARGET mqtt_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${mqtt_client_rust_LIBRARY} ${mqtt_broker_rust_LIBRARY} $<TARGET_FILE_DIR:mqtt_demo>)
endif()

enable_testing()
add_test(NAME mqtt_demo COMMAND mqtt_demo)
