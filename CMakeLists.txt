cmake_minimum_required(VERSION 3.1)
project(mesytec_data)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(mesytec_data SHARED mesytec_module.cpp mesytec_data.cpp mesytec_buffer_reader.cpp)
add_executable(read_listfile read_listfile.cpp)
target_link_libraries(read_listfile mesytec_data)

install(TARGETS mesytec_data read_listfile
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES mesytec_module.h mesytec_data.h mesytec_buffer_reader.h DESTINATION include/mesytec)
