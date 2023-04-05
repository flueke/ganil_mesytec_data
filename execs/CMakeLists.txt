#- look for ZeroMQ to build receiver/transmitter
find_package(ZMQ)
if(ZMQ_FOUND)
    include_directories(${ZMQ_INCLUDE_DIRS})
    # look for boost
    find_package(Boost COMPONENTS program_options)
    if(Boost_PROGRAM_OPTIONS_FOUND)
        include_directories(${Boost_INCLUDE_DIRS})
        add_executable(zmq_receiver zmq_receiver.cpp)
        target_link_libraries(zmq_receiver ${ZMQ_LIBRARIES} ${Boost_PROGRAM_OPTIONS_LIBRARY})
        install(TARGETS zmq_receiver
            EXPORT ${CMAKE_PROJECT_NAME}Exports
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    if(WITH_MESYTEC_MVLC)
        add_executable(mesytec_receiver_mfm_transmitter mesytec_receiver_mfm_transmitter.cpp)
        target_link_libraries(mesytec_receiver_mfm_transmitter mesytec_data ${ZMQ_LIBRARIES} ${Boost_PROGRAM_OPTIONS_LIBRARY})

        install(TARGETS mesytec_receiver_mfm_transmitter
            EXPORT ${CMAKE_PROJECT_NAME}Exports
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif(WITH_MESYTEC_MVLC)
    endif(Boost_PROGRAM_OPTIONS_FOUND)
endif(ZMQ_FOUND)
