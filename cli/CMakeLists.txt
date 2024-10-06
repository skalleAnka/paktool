cmake_minimum_required(VERSION 3.12)

file(GLOB PAKTOOL_SRC CONFIGURE_DEPENDS "*.h" "*.cpp")

find_package(Boost COMPONENTS program_options endian crc REQUIRED)

if (UNIX)
    find_program(PANDOC_EXE pandoc)
    if (PANDOC_EXE)
        add_custom_command(
                OUTPUT ${CMAKE_BINARY_DIR}/paktool.1
                COMMAND ${PANDOC_EXE} -s -t man ${CMAKE_CURRENT_LIST_DIR}/readme.md -o ${CMAKE_BINARY_DIR}/paktool.1
                DEPENDS ${CMAKE_CURRENT_LIST_DIR}/readme.md
                COMMENT "Generating man page"
                VERBATIM)
        add_custom_target(paktool_manpage ALL DEPENDS ${CMAKE_BINARY_DIR}/paktool.1)
        install(FILES ${CMAKE_BINARY_DIR}/paktool.1 DESTINATION share/man/man1)
    endif()
endif()

add_executable(paktool ${PAKTOOL_SRC})
target_include_directories(paktool PRIVATE ../src)
target_link_libraries(paktool paklib Boost::program_options Boost::endian Boost::crc)

install(TARGETS paktool DESTINATION bin)

if (WIN32)
    install(FILES $<TARGET_RUNTIME_DLLS:paktool> DESTINATION bin)
endif()
