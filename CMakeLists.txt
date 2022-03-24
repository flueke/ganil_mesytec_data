cmake_minimum_required(VERSION 3.1)
project(mesytec_data)

include(GNUInstallDirs)
set(CMAKE_INSTALL_PKGINCDIR ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(mesytec_data SHARED mesytec_module.cpp mesytec_experimental_setup.cpp mesytec_data.cpp mesytec_buffer_reader.cpp)
#add_executable(read_listfile read_listfile.cpp)
#target_link_libraries(read_listfile mesytec_data)

#- set path to our cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

#- look for ZeroMQ to build Narval receiver for INDRA DAQ
find_package(ZMQ)
if(ZMQ_FOUND)
    include_directories(${ZMQ_INCLUDE_DIRS})
    add_library(mesytec_narval_receiver SHARED mesytec_narval_receiver.cpp)
    target_link_libraries(mesytec_narval_receiver mesytec_data ${ZMQ_LIBRARIES})
    add_executable(test_narval_actor test_narval_actor.cpp)
    target_link_libraries(test_narval_actor mesytec_data ${ZMQ_LIBRARIES})
    add_executable(test_buffer_reader test_buffer_reader.cpp)
    target_link_libraries(test_buffer_reader mesytec_data ${ZMQ_LIBRARIES})
    install(TARGETS mesytec_narval_receiver test_narval_actor test_buffer_reader
        EXPORT ${CMAKE_PROJECT_NAME}Exports
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(FILES mesytec_narval_receiver.h DESTINATION ${CMAKE_INSTALL_PKGINCDIR})
endif(ZMQ_FOUND)

install(TARGETS mesytec_data
# read_listfile
    EXPORT ${CMAKE_PROJECT_NAME}Exports
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
set(CMAKE_INSTALL_PKGINCDIR ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})
install(FILES mesytec_module.h mesytec_data.h mesytec_buffer_reader.h mesytec_experimental_setup.h
   DESTINATION ${CMAKE_INSTALL_PKGINCDIR})

#---install cmake stuff for find_package(mesytec_data) for use by other projects
set(CMAKE_INSTALL_PKGCONFIGDIR ${CMAKE_INSTALL_DATAROOTDIR}/cmake/mesytec_data)
install(EXPORT ${CMAKE_PROJECT_NAME}Exports
   DESTINATION ${CMAKE_INSTALL_PKGCONFIGDIR}
   FILE ${CMAKE_PROJECT_NAME}-targets.cmake
   )
include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_SOURCE_DIR}/mesytec_dataConfig.cmake.in
                                 ${CMAKE_BINARY_DIR}/mesytec_dataConfig.cmake
                                 INSTALL_DESTINATION ${CMAKE_INSTALL_PKGCONFIGDIR}
                                 PATH_VARS CMAKE_INSTALL_PKGINCDIR
                                 CMAKE_INSTALL_LIBDIR
                                 CMAKE_INSTALL_PKGCONFIGDIR
                                 )
install(FILES ${CMAKE_BINARY_DIR}/mesytec_dataConfig.cmake DESTINATION ${CMAKE_INSTALL_PKGCONFIGDIR})
