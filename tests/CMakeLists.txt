add_executable(test_event_builder test_event_builder.cpp)
target_link_libraries(test_event_builder mesytec_data)
add_executable(example_analysis example_analysis.cpp)
target_link_libraries(example_analysis mesytec_data)
install(TARGETS example_analysis test_event_builder
    EXPORT ${CMAKE_PROJECT_NAME}Exports
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/mesytec_data_tests
)
