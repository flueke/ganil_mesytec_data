add_library(mesytec_data SHARED mesytec_module.cpp mesytec_experimental_setup.cpp mesytec_data.cpp mesytec_buffer_reader.cpp)

install(TARGETS mesytec_data
    EXPORT ${CMAKE_PROJECT_NAME}Exports
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES mesytec_module.h mesytec_data.h mesytec_buffer_reader.h mesytec_experimental_setup.h
   DESTINATION ${CMAKE_INSTALL_PKGINCDIR})

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
