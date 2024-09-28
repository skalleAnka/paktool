cmake_minimum_required(VERSION 3.12)

find_package(Boost COMPONENTS locale date_time REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_MINIZIP IMPORTED_TARGET minizip)

file(GLOB LIBPAKTOOL_SRC CONFIGURE_DEPENDS "*.h" "*.cpp" "impl/*.cpp" "impl/*.h")

add_library(paklib ${LIBPAKTOOL_SRC})
target_link_libraries(paklib PUBLIC ${PC_MINIZIP_LIBRARIES} Boost::locale Boost::date_time)