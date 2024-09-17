cmake_minimum_required(VERSION 3.12)

file(GLOB PAKTOOL_SRC CONFIGURE_DEPENDS "*.h" "*.cpp")

add_executable(paktool ${PAKTOOL_SRC})
target_link_libraries(paktool paklib Boost::program_options)
