cmake_minimum_required(VERSION 3.1)
project(mesytec_data)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(mesytec_data SHARED mesytec_module.cpp mesytec_data.cpp mesytec_buffer_reader.cpp)
add_executable(read_listfile read_listfile.cpp)
target_link_libraries(read_listfile mesytec_data)

#- set path to our cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

find_package(ZMQ REQUIRED)
include_directories(${ZMQ_INCLUDE_DIRS})
#add_executable(test_buffer_reader test_buffer_reader.cpp)
#target_link_libraries(test_buffer_reader mesytec_data ${ZMQ_LIBRARIES})

add_library(mesytec_narval_receiver SHARED mesytec_narval_receiver.cpp)
target_link_libraries(mesytec_narval_receiver mesytec_data ${ZMQ_LIBRARIES})
#add_executable(test_narval_actor test_narval_actor.cpp)
#target_link_libraries(test_narval_actor mesytec_narval_receiver)

install(TARGETS mesytec_data read_listfile mesytec_narval_receiver
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES mesytec_module.h mesytec_data.h mesytec_buffer_reader.h mesytec_narval_receiver.h
   DESTINATION include/mesytec)
