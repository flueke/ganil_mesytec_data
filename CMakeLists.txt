cmake_minimum_required(VERSION 3.1)
project(mesytec_data)

include(GNUInstallDirs)
set(CMAKE_INSTALL_PKGINCDIR ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(lib)

#- give access to library headers
include_directories(lib)

#- set path to our cmake modules (FindZMQ)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

add_subdirectory(narval)

add_subdirectory(execs)

option(BUILD_TESTS "Build executables for testing/debugging" OFF)
if(BUILD_TESTS)
    message(STATUS "Will build executables for testing/debugging")
    add_subdirectory(tests)
endif(BUILD_TESTS)
