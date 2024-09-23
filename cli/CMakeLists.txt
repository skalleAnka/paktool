cmake_minimum_required(VERSION 3.12)

file(GLOB PAKTOOL_SRC CONFIGURE_DEPENDS "*.h" "*.cpp")

find_package(Boost COMPONENTS program_options REQUIRED)

add_executable(paktool ${PAKTOOL_SRC})
target_include_directories(paktool PRIVATE ../src)
target_link_libraries(paktool paklib Boost::program_options)
