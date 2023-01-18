#- look for ZeroMQ to build Narval actor
find_package(ZMQ)
if(ZMQ_FOUND)
    add_library(mesytec_narval_receiver SHARED mesytec_narval_receiver.cpp)
    target_include_directories(mesytec_narval_receiver INTERFACE ${ZMQ_INCLUDE_DIRS})
    target_link_libraries(mesytec_narval_receiver mesytec_data ${ZMQ_LIBRARIES})
    install(TARGETS mesytec_narval_receiver
        EXPORT ${CMAKE_PROJECT_NAME}Exports
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(FILES mesytec_narval_receiver.h DESTINATION ${CMAKE_INSTALL_PKGINCDIR})
endif(ZMQ_FOUND)
