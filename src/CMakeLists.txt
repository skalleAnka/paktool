cmake_minimum_required(VERSION 3.12)

file(GLOB LIBPAKTOOL_SRC CONFIGURE_DEPENDS "*.h" "*.cpp" "impl/*.cpp" "impl/*.h")

add_library(paklib ${LIBPAKTOOL_SRC})
