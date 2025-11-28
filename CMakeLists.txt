cmake_minimum_required(VERSION 3.16)

project(psi_gc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(PSI_WITH_BLAKE3_HASH "Use BLAKE3 as the hash function for PSI elements" ON)

if(PSI_WITH_BLAKE3_HASH)
    message(STATUS "PSI_WITH_BLAKE3_HASH=ON: building and linking BLAKE3")

    set(BLAKE3_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/BLAKE3/c")

    # always use the portable core so we can compile for web and embedded
    set(BLAKE3_SOURCES
        "${BLAKE3_SRC_DIR}/blake3.c"
        "${BLAKE3_SRC_DIR}/blake3_dispatch.c"
        "${BLAKE3_SRC_DIR}/blake3_portable.c"
    )

    # only add SIMD-specific sources and flags on non-Emscripten builds
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        list(APPEND BLAKE3_SOURCES
            "${BLAKE3_SRC_DIR}/blake3_avx2.c"
            "${BLAKE3_SRC_DIR}/blake3_avx512.c"
            "${BLAKE3_SRC_DIR}/blake3_sse2.c"
            "${BLAKE3_SRC_DIR}/blake3_sse41.c"
        )
    endif()

    add_library(blake3 STATIC ${BLAKE3_SOURCES})

    target_include_directories(blake3
        PUBLIC
            "${BLAKE3_SRC_DIR}"
    )

    # Enable x86 SIMD instruction sets only for native builds
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
            target_compile_options(blake3 PRIVATE
                -msse2
                -msse4.1
                -mavx2
                -mavx512f
                -mavx512vl
                -mavx512bw
                -mavx512dq
            )
        endif()
    endif()
endif()

# this is the core psi library
add_library(psi_gc STATIC
    src/psi_gc.c
    src/psi_hash_blake3.c
    src/gc_core.c
)

target_include_directories(psi_gc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(PSI_WITH_BLAKE3_HASH)
    target_link_libraries(psi_gc PRIVATE blake3)
endif()

# tests
enable_testing()

add_executable(test_psi_gc
    tests/test_main.c
)

target_link_libraries(test_psi_gc
    PRIVATE psi_gc
)

add_test(NAME psi_gc_tests COMMAND test_psi_gc)

add_executable(test_gc_core
    tests/test_gc_core.c
)

target_link_libraries(test_gc_core
    PRIVATE psi_gc
)

add_test(NAME gc_core_tests COMMAND test_gc_core)

add_executable(test_gc_garbled
    tests/test_gc_garbled.c
)

target_link_libraries(test_gc_garbled
    PRIVATE psi_gc
)

add_test(NAME gc_garbled_tests COMMAND test_gc_garbled)

add_executable(test_gc_proto_psi
    tests/test_gc_proto_psi.c
)

target_link_libraries(test_gc_proto_psi
    PRIVATE psi_gc
)

add_test(NAME gc_proto_psi_tests COMMAND test_gc_proto_psi)

add_executable(test_psi_bench
    tests/test_psi_bench.c
)

target_link_libraries(test_psi_bench
    PRIVATE psi_gc
)


# This target is only enabled when configuring with emcmake (Emscripten's CMake
# wrapper), which sets CMAKE_SYSTEM_NAME to "Emscripten". It compiles the
# psi_gc + gc_core code into a single JS/WASM module for use in web/
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(STATUS "Configuring psi_gc_wasm target for Emscripten")

    add_executable(psi_gc_wasm
        src/psi_gc.c
        src/psi_hash_blake3.c
        src/gc_core.c
    )

    target_include_directories(psi_gc_wasm
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    # name the output "psi_gc" so Emscripten emits psi_gc.js / psi_gc.wasm
    set_target_properties(psi_gc_wasm PROPERTIES
        OUTPUT_NAME "psi_gc"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web"
    )

    # Link against the BLAKE3 library so gc_core.c and psi_hash_blake3.c
    # can use blake3_hasher_* and include "blake3.h"
    if(PSI_WITH_BLAKE3_HASH)
        target_link_libraries(psi_gc_wasm PRIVATE blake3)
    endif()
endif()
