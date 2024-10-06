cmake_minimum_required(VERSION 3.12)
include(FetchContent)
cmake_policy(SET CMP0135 NEW)

find_package(Boost COMPONENTS locale date_time REQUIRED)

FetchContent_Declare(
    zlib
    URL https://github.com/madler/zlib/archive/refs/tags/v1.3.1.zip
    OVERRIDE_FIND_PACKAGE
    URL_HASH SHA256=50b24b47bf19e1f35d2a21ff36d2a366638cdf958219a66f30ce0861201760e6 
)

FetchContent_MakeAvailable(zlib)

file(GLOB CUSTOM_MINIZIP_SRC CONFIGURE_DEPENDS
    "${zlib_SOURCE_DIR}/contrib/minizip/*.h"
    "${zlib_SOURCE_DIR}/contrib/minizip/unzip.c"
    "${zlib_SOURCE_DIR}/contrib/minizip/zip.c"
    "${zlib_SOURCE_DIR}/contrib/minizip/ioapi.c"
)
add_library(custom_minizip ${CUSTOM_MINIZIP_SRC})
set_target_properties(custom_minizip PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(custom_minizip PUBLIC zlibstatic)

file(GLOB LIBPAKTOOL_SRC CONFIGURE_DEPENDS "*.h" "*.cpp" "impl/*.cpp" "impl/*.h")

add_library(paklib ${LIBPAKTOOL_SRC})
set_target_properties(paklib PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(paklib PUBLIC Boost::locale Boost::date_time Boost::headers)

target_include_directories(paklib PRIVATE "${zlib_SOURCE_DIR}/contrib")
target_link_libraries(paklib PUBLIC custom_minizip)
