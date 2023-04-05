cmake_minimum_required(VERSION 3.1)
project(mesytec_data)

include(GNUInstallDirs)
set(CMAKE_INSTALL_PKGINCDIR ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# RPATH handling - executables can be used without setting LD_LIBRARY_PATH
# https://gitlab.kitware.com/cmake/community/-/wikis/doc/cmake/RPATH-handling

# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH FALSE)

# Enable $ORIGIN in the rpath if supported by the target platform.
SET(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)

# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# look for mesytec-mvlc package on system
# if available, we build the mesytec_receiver_mfm_transmitter using Florian Lueke's parsing code
find_package(mesytec-mvlc)
if(mesytec-mvlc_DIR)
    message(STATUS "mesyec-mvlc software found: mesytec-mvlc_DIR=${mesytec-mvlc_DIR}")
    set(WITH_MESYTEC_MVLC true)
endif(mesytec-mvlc_DIR)

add_subdirectory(lib)

#- set path to our cmake modules (FindZMQ)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

add_subdirectory(narval)

add_subdirectory(execs)

option(BUILD_TESTS "Build executables for testing/debugging" OFF)
if(BUILD_TESTS)
    message(STATUS "Will build executables for testing/debugging")
    add_subdirectory(tests)
endif(BUILD_TESTS)
