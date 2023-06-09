set(SOURCES mesytec_module.cpp mesytec_experimental_setup.cpp mesytec_data.cpp mesytec_buffer_reader.cpp)
set(HEADERS mesytec_module.h mesytec_data.h mesytec_buffer_reader.h mesytec_experimental_setup.h fast_lookup_map.h)

if(WITH_MESYTEC_MVLC)
    set(SOURCES ${SOURCES} mesytec_buffer_reader_mvlc_parser.cpp)
    set(HEADERS ${HEADERS} mesytec_buffer_reader_mvlc_parser.h)
endif(WITH_MESYTEC_MVLC)

add_library(mesytec_data SHARED ${SOURCES})
target_include_directories(mesytec_data PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_PKGINCDIR}>  # <prefix>/include/mesytec_data
)
if(WITH_MESYTEC_MVLC)
    target_link_libraries(mesytec_data mesytec-mvlc::mesytec-mvlc)
endif(WITH_MESYTEC_MVLC)

install(TARGETS mesytec_data
    EXPORT ${CMAKE_PROJECT_NAME}Exports
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_PKGINCDIR})

#---install cmake stuff for find_package(mesytec_data) for use by other projects
set(CMAKE_INSTALL_PKGCONFIGDIR ${CMAKE_INSTALL_DATAROOTDIR}/cmake/mesytec_data)
install(EXPORT ${CMAKE_PROJECT_NAME}Exports
   DESTINATION ${CMAKE_INSTALL_PKGCONFIGDIR}
   FILE ${CMAKE_PROJECT_NAME}-targets.cmake
   )
include(CMakePackageConfigHelpers)
configure_package_config_file(mesytec_dataConfig.cmake.in
                                 ${CMAKE_BINARY_DIR}/mesytec_dataConfig.cmake
                                 INSTALL_DESTINATION ${CMAKE_INSTALL_PKGCONFIGDIR}
                                 PATH_VARS CMAKE_INSTALL_PKGINCDIR
                                 CMAKE_INSTALL_LIBDIR
                                 CMAKE_INSTALL_PKGCONFIGDIR
                                 )
install(FILES ${CMAKE_BINARY_DIR}/mesytec_dataConfig.cmake DESTINATION ${CMAKE_INSTALL_PKGCONFIGDIR})
