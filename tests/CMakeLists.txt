add_executable(test_event_builder test_event_builder.cpp)
target_link_libraries(test_event_builder mesytec_data)
add_executable(debug_mesytec_buffers debug_mesytec_buffers.cpp)
target_link_libraries(debug_mesytec_buffers mesytec_data)
install(TARGETS test_event_builder debug_mesytec_buffers
    EXPORT ${CMAKE_PROJECT_NAME}Exports
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/mesytec_data_tests
)

find_package(ZMQ)
if(ZMQ_FOUND)
    include_directories(${ZMQ_INCLUDE_DIRS})
    add_executable(test_narval_actor test_narval_actor.cpp)
    target_link_libraries(test_narval_actor mesytec_data ${ZMQ_LIBRARIES})
    add_executable(test_buffer_reader test_buffer_reader.cpp)
    target_link_libraries(test_buffer_reader mesytec_data ${ZMQ_LIBRARIES})
    install(TARGETS test_narval_actor test_buffer_reader
        EXPORT ${CMAKE_PROJECT_NAME}Exports
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/mesytec_data_tests
    )
endif(ZMQ_FOUND)
