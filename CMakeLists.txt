cmake_minimum_required(VERSION 3.1)
project(mesytec_data)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(mesytec_data SHARED mesytec_module.cpp mesytec_data.cpp)

install(TARGETS mesytec_data LIBRARY DESTINATION lib)
install(FILES mesytec_module.h mesytec_data.h DESTINATION include/mesytec)
