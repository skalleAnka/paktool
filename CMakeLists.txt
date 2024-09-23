cmake_minimum_required(VERSION 3.12)
project(paktool VERSION 1.0)
set(CMAKE_CXX_STANDARD 20)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-parentheses)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unqualified-std-cast-call -Wno-logical-op-parentheses -Wno-switch)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4)
endif()

add_compile_definitions(PRIVATE $<$<CONFIG:Debug>:_DEBUG=1>)

add_subdirectory(src)
add_subdirectory(cli)
