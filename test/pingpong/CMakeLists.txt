cmake_minimum_required(VERSION 2.8)

##################################################################################
project(pingpong)

if (CMAKE_BUILD_TYPE)
else()
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_FLAGS "-std=c++11 -O3 -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-std=c++11 -g -pg -Wall")

include_directories("${PROJECT_SOURCE_DIR}/../../")
link_directories("${PROJECT_SOURCE_DIR}/../../build")

add_executable(pingpong_client client.cc)
target_link_libraries(pingpong_client libgonet libgo boost_coroutine boost_context boost_regex boost_system boost_thread pthread -static -static-libgcc -static-libstdc++)

add_executable(pingpong_server server.cc)
target_link_libraries(pingpong_server libgonet libgo boost_coroutine boost_context boost_regex boost_system boost_thread pthread -static -static-libgcc -static-libstdc++)
