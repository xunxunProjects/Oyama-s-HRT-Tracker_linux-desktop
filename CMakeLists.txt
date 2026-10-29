cmake_minimum_required(VERSION 3.21)

project(oyama_hrt_linux_desktop
    VERSION 0.1.0
    DESCRIPTION "Oyama's HRT Tracker - Linux/BSD desktop"
    LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(oyama_pk STATIC
    core/src/pk.c)
target_include_directories(oyama_pk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/core/include)
if(NOT WIN32)
    target_link_libraries(oyama_pk PUBLIC m)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(oyama_pk PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(pk_smoke_test core/tests/pk_smoke_test.c)
    target_link_libraries(pk_smoke_test PRIVATE oyama_pk)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(pk_smoke_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME pk_smoke_test COMMAND pk_smoke_test)
endif()

option(OYAMA_BUILD_COMPARE_TOOLS "Build the upstream comparison dumper" ON)

if(OYAMA_BUILD_COMPARE_TOOLS)
    add_executable(oyama_pk_dump tools/compare/dump.c)
    target_link_libraries(oyama_pk_dump PRIVATE oyama_pk)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(oyama_pk_dump PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
