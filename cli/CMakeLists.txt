cmake_minimum_required(VERSION 3.12)

file(GLOB PAKTOOL_SRC CONFIGURE_DEPENDS "*.h" "*.cpp")
set(BOOST_COMPONENTS_CLI program_options headers)
set(BOOST_LINK_TARGETS_CLI Boost::program_options Boost::headers)

find_package(Boost COMPONENTS BOOST_COMPONENTS_CLI QUIET)
if (NOT Boost_HEADERS_FOUND)
    #At some point they split the headers package into separate packages
    set(BOOST_COMPONENTS_CLI program_options crc)
    set(BOOST_LINK_TARGETS_CLI Boost::program_options Boost::crc)
endif()

find_package(Boost COMPONENTS ${BOOST_COMPONENTS_CLI} REQUIRED)

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
target_link_libraries(paktool paklib ${BOOST_LINK_TARGETS_CLI})

install(TARGETS paktool DESTINATION bin)

if (WIN32)
    install(FILES $<TARGET_RUNTIME_DLLS:paktool> DESTINATION bin)
endif()
