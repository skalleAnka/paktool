cmake_minimum_required(VERSION 3.12)

file(GLOB LIBPAKTOOL_SRC CONFIGURE_DEPENDS "*.h" "*.cpp")

add_library(libpaktool ${LIBPAKTOOL_SRC})
