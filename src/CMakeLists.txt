cmake_minimum_required(VERSION 3.12)

find_package(Boost COMPONENTS locale date_time REQUIRED)

if (NOT VCPKG_MANIFEST_MODE)
	pkg_check_modules(minizip REQUIRED)
else()
	find_package(minizip-ng REQUIRED)
endif()

file(GLOB LIBPAKTOOL_SRC CONFIGURE_DEPENDS "*.h" "*.cpp" "impl/*.cpp" "impl/*.h")

add_library(paklib ${LIBPAKTOOL_SRC})
target_link_libraries(paklib PUBLIC Boost::locale Boost::date_time)
if (NOT VCPKG_MANIFEST_MODE)
	target_link_libraries(paklib PUBLIC ${PC_MINIZIP_LIBRARIES})
else()
	target_link_libraries(paklib PUBLIC MINIZIP::minizip-ng)
endif()