cmake_minimum_required(VERSION 2.8)


##################################################################################
project(libgonet)

if (CMAKE_BUILD_TYPE)
else()
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Werror -fPIC -g -pg")
set(CMAKE_CXX_FLAGS_DEBUG "-g -pg ${CMAKE_CXX_FLAGS}")

aux_source_directory(src SRC_LIST)
include_directories("src")
include_directories("third_party/libgo/src")
include_directories("third_party/libgo/src/linux")
set(TARGET "libgonet")

add_library("${TARGET}" STATIC ${SRC_LIST})

set(CMAKE_INSTALL_PREFIX "/usr/local")
install(TARGETS ${TARGET} LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")
install(DIRECTORY src/ DESTINATION "include/libgonet" FILES_MATCHING PATTERN "*.h")

add_subdirectory("third_party/libgo")
