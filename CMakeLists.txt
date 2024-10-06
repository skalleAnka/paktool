cmake_minimum_required(VERSION 3.25)
cmake_policy(SET CMP0135 NEW)

include(version.cmake)
project(paktool VERSION ${PAKTOOL_VERSION})
set(CMAKE_CXX_STANDARD 20)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-parentheses)
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13.0)
        message(FATAL_ERROR "GCC version must be at least 13.0")
    endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unqualified-std-cast-call -Wno-logical-op-parentheses -Wno-switch)
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16.0)
        message(FATAL_ERROR "Clang version must be at least 16.0")
    endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    #add_compile_options(/W4)
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16.11)
        message(FATAL_ERROR "MSVC version must be at least 16.11")
    endif()
endif()

if (MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++ -static-libgfortran")
endif()

include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries")
set(BOOST_COMPONENTS program_options locale date_time headers)

find_package(Boost COMPONENTS ${BOOST_COMPONENTS} QUIET)

if (Boost_locale_FOUND AND NOT Boost_HEADERS_FOUND OR NOT Boost_locale_FOUND)
    #At some point they split the headers package into separate packages
    set(BOOST_COMPONENTS program_options locale date_time crc endian)
    find_package(Boost COMPONENTS ${BOOST_COMPONENTS} QUIET)
endif()

foreach (component IN LISTS BOOST_COMPONENTS)
    if (NOT Boost_${component}_FOUND)
        list(APPEND MISSING_BOOST_COMPONENTS ${component})
    endif()
endforeach()

if (MISSING_BOOST_COMPONENTS)
    message(STATUS "Getting missing Boost components: ${MISSING_BOOST_COMPONENTS} (this will take time, be patient)")
        
    set(BOOST_ENABLE_CMAKE ON)
    set(BOOST_INCLUDE_LIBRARIES ${MISSING_BOOST_COMPONENTS})
    FetchContent_Declare(
        Boost
        URL https://github.com/boostorg/boost/releases/download/boost-1.86.0/boost-1.86.0-cmake.7z 
        OVERRIDE_FIND_PACKAGE
        URL_HASH SHA256=ee6e0793b5ec7d13e7181ec05d3b1aaa23615947295080e4b9930324488e078f
    )
    FetchContent_MakeAvailable(Boost)

    find_package(Boost REQUIRED COMPONENTS ${MISSING_BOOST_COMPONENTS})
endif()

add_compile_definitions(PRIVATE $<$<CONFIG:Debug>:_DEBUG=1>)

add_subdirectory(src)
add_subdirectory(cli)

if (EXISTS "/etc/debian_version")
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "skalleAnka")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libboost-program-options1.74.0, libboost-locale1.74.0, libboost-atomic1.74.0, libboost-chrono1.74.0, libboost-system1.74.0, libboost-thread1.74.0, libboost-date-time1.74.0, libicu72")
elseif (UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "TGZ")
else()
    set(CPACK_GENERATOR "ZIP")
endif()

set(CPACK_PACKAGE_NAME "paktool")
set(CPACK_PACKAGE_VERSION ${PAKTOOL_VERSION})
set(CPACK_PACKAGE_DESCRIPTION "Tool for converting and extracting .pak and .pk3 used by Quake engines.")
set(CPACK_PACKAGE_CONTACT "thisemailis@fa.ke")

include(CPack)
