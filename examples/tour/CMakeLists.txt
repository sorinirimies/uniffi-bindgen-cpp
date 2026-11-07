# Builds the tour fixture crate with cargo, generates its C++ bindings and runs
# the C++ test against them: the CMake way to use uniffi-bindgen-cpp.
#
#   cmake -S examples/tour -B build/tour -DUNIFFI_BINDGEN_CPP=<path to uniffi-bindgen-cpp>
#   cmake --build build/tour && ctest --test-dir build/tour
cmake_minimum_required(VERSION 3.19)
project(uniffi_cpp_tour CXX)

set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/../..)
include(${REPO}/crates/uniffi-bindgen-cpp/cmake/UniffiCpp.cmake)

uniffi_cargo_library(tour_rust MANIFEST ${REPO}/fixtures/tour/Cargo.toml NAME tour PROFILE dev)
uniffi_cpp_bindings(tour RUST tour_rust COMPONENTS tour)

add_executable(test_tour ${REPO}/fixtures/tour/cpp/test_tour.cpp)
target_link_libraries(test_tour PRIVATE tour)
if(WIN32)
    # Windows finds DLLs next to the executable.
    add_custom_command(TARGET test_tour POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${tour_rust_LIBRARY} $<TARGET_FILE_DIR:test_tour>)
endif()

enable_testing()
add_test(NAME tour COMMAND test_tour)
