# UniffiCpp.cmake: use a Rust library with UniFFI bindings from CMake.
#
# Written by `uniffi-bindgen-cpp cmake <dir>`; include it and call:
#
#   uniffi_cargo_library(<target>
#       MANIFEST <Cargo.toml>     the crate (or workspace) to build
#       NAME <lib name>           the [lib] name, e.g. mqtt_client for libmqtt_client.so
#       [PACKAGE <package>]       for a workspace: the package to build
#       [PROFILE <profile>]       cargo profile, default `release`
#       [FEATURES <features>...]
#   )
#
#     Builds the crate as a cdylib with cargo and makes <target> link it.
#     Sets <target>_LIBRARY (the shared library) in the caller's scope.
#
#   uniffi_cpp_bindings(<target>
#       RUST <target> | LIBRARY <file>
#                                 the library to read the interface from: a uniffi_cargo_library
#                                 target, or a prebuilt library file
#       COMPONENTS <name>...      its UniFFI namespaces; <name>.cpp/.hpp are generated for each
#       [LINK <target|file>...]   what to link, default: the RUST target or LIBRARY
#       [DEPENDS <target>...]     more targets to build first
#       [OUT_DIR <dir>]           default: ${CMAKE_CURRENT_BINARY_DIR}/<target>
#       [CRATE_DIR <dir>]         where to run the generator: library mode finds UDL files
#                                 through `cargo metadata` there. Default: the RUST target's
#                                 crate, else CMAKE_CURRENT_SOURCE_DIR
#       [CONFIG <uniffi.toml>]
#       [BINDGEN <program>]       default: uniffi-bindgen-cpp on the PATH
#       [NO_EXCEPTIONS]           generate with --no-exceptions (Rust errors come back in a
#                                 uniffi::Result) and build the bindings with -fno-exceptions
#                                 (GCC and Clang)
#   )
#
#     Generates the bindings at build time and compiles them into the static library
#     <target>, which carries the include path and links the Rust library.
#
# Typical use:
#
#   include(UniffiCpp.cmake)
#   uniffi_cargo_library(mylib_rust MANIFEST ${CMAKE_SOURCE_DIR}/rust/Cargo.toml NAME mylib)
#   uniffi_cpp_bindings(mylib RUST mylib_rust COMPONENTS mylib)
#   target_link_libraries(app PRIVATE mylib)

cmake_minimum_required(VERSION 3.19)

function(uniffi_cargo_library target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "MANIFEST;NAME;PACKAGE;PROFILE" "FEATURES")
    if(NOT ARG_MANIFEST OR NOT ARG_NAME)
        message(FATAL_ERROR "uniffi_cargo_library(${target}): MANIFEST and NAME are required")
    endif()
    if(NOT ARG_PROFILE)
        set(ARG_PROFILE release)
    endif()
    find_program(UNIFFI_CARGO cargo REQUIRED)

    set(target_dir "${CMAKE_BINARY_DIR}/cargo")
    # Cargo puts the `dev` profile in `debug/`.
    if(ARG_PROFILE STREQUAL "dev")
        set(out_dir "${target_dir}/debug")
    else()
        set(out_dir "${target_dir}/${ARG_PROFILE}")
    endif()
    if(WIN32)
        set(library "${out_dir}/${ARG_NAME}.dll")
        set(link "${out_dir}/${ARG_NAME}.dll.lib")
    elseif(APPLE)
        set(library "${out_dir}/lib${ARG_NAME}.dylib")
        set(link "${library}")
    else()
        set(library "${out_dir}/lib${ARG_NAME}.so")
        set(link "${library}")
    endif()

    set(byproducts "${library}")
    if(NOT link STREQUAL library)
        list(APPEND byproducts "${link}")
    endif()

    set(package_args)
    if(ARG_PACKAGE)
        set(package_args -p "${ARG_PACKAGE}")
    endif()
    set(feature_args)
    if(ARG_FEATURES)
        list(JOIN ARG_FEATURES "," features)
        set(feature_args --features "${features}")
    endif()

    # `cargo rustc --crate-type cdylib` works whatever crate-type the manifest lists.
    add_custom_target(${target}_cargo
        COMMAND "${UNIFFI_CARGO}" rustc --lib --crate-type cdylib --profile "${ARG_PROFILE}"
                --manifest-path "${ARG_MANIFEST}" --target-dir "${target_dir}"
                ${package_args} ${feature_args}
        BYPRODUCTS ${byproducts}
        COMMENT "Building ${ARG_NAME} with cargo"
        USES_TERMINAL
        VERBATIM)

    add_library(${target} INTERFACE)
    add_dependencies(${target} ${target}_cargo)
    target_link_libraries(${target} INTERFACE "${link}")
    get_filename_component(crate_dir "${ARG_MANIFEST}" DIRECTORY)
    set_target_properties(${target} PROPERTIES
        UNIFFI_LIBRARY "${library}"
        UNIFFI_CRATE_DIR "${crate_dir}")
    set(${target}_LIBRARY "${library}" PARENT_SCOPE)
endfunction()

function(uniffi_cpp_bindings target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "NO_EXCEPTIONS" "RUST;LIBRARY;OUT_DIR;CRATE_DIR;CONFIG;BINDGEN" "COMPONENTS;LINK;DEPENDS")
    if(ARG_RUST)
        get_target_property(ARG_LIBRARY ${ARG_RUST} UNIFFI_LIBRARY)
        if(NOT ARG_LINK)
            set(ARG_LINK ${ARG_RUST})
        endif()
        if(NOT ARG_CRATE_DIR)
            get_target_property(ARG_CRATE_DIR ${ARG_RUST} UNIFFI_CRATE_DIR)
        endif()
        list(APPEND ARG_DEPENDS ${ARG_RUST}_cargo)
    endif()
    if(NOT ARG_LIBRARY OR NOT ARG_COMPONENTS)
        message(FATAL_ERROR "uniffi_cpp_bindings(${target}): RUST or LIBRARY, and COMPONENTS, are required")
    endif()
    if(NOT ARG_OUT_DIR)
        set(ARG_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${target}")
    endif()
    if(NOT ARG_CRATE_DIR)
        set(ARG_CRATE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    if(NOT ARG_BINDGEN)
        find_program(UNIFFI_BINDGEN_CPP uniffi-bindgen-cpp REQUIRED)
        set(ARG_BINDGEN "${UNIFFI_BINDGEN_CPP}")
    endif()
    if(NOT ARG_LINK)
        set(ARG_LINK "${ARG_LIBRARY}")
    endif()

    set(sources)
    set(headers "${ARG_OUT_DIR}/uniffi_runtime.hpp")
    foreach(component IN LISTS ARG_COMPONENTS)
        list(APPEND sources "${ARG_OUT_DIR}/${component}.cpp")
        list(APPEND headers "${ARG_OUT_DIR}/${component}.hpp")
    endforeach()
    set(config_args)
    if(ARG_CONFIG)
        set(config_args --config "${ARG_CONFIG}")
    endif()
    if(ARG_NO_EXCEPTIONS)
        list(APPEND config_args --no-exceptions)
    endif()

    add_custom_command(
        OUTPUT ${sources} ${headers}
        COMMAND "${ARG_BINDGEN}" generate "${ARG_LIBRARY}" --out-dir "${ARG_OUT_DIR}" --no-format ${config_args}
        DEPENDS "${ARG_LIBRARY}" "${ARG_BINDGEN}" ${ARG_DEPENDS}
        WORKING_DIRECTORY "${ARG_CRATE_DIR}"
        COMMENT "Generating C++ bindings for ${ARG_COMPONENTS}"
        VERBATIM)

    add_library(${target} STATIC ${sources} ${headers})
    target_include_directories(${target} PUBLIC "${ARG_OUT_DIR}")
    target_compile_features(${target} PUBLIC cxx_std_17)
    if(ARG_NO_EXCEPTIONS)
        target_compile_options(${target} PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fno-exceptions>)
    endif()
    find_package(Threads REQUIRED)
    target_link_libraries(${target} PUBLIC ${ARG_LINK} Threads::Threads)
    if(ARG_DEPENDS)
        add_dependencies(${target} ${ARG_DEPENDS})
    endif()
endfunction()
