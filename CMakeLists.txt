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

aux_source_directory(libgonet SRC_LIST)
include_directories("libgonet")
include_directories("third_party/libgo")
include_directories("third_party/libgo/libgo")
include_directories("third_party/libgo/libgo/linux")
set(TARGET "libgonet")

add_library("${TARGET}" STATIC ${SRC_LIST})

set(CMAKE_INSTALL_PREFIX "/usr/local")
install(TARGETS ${TARGET} LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")
install(DIRECTORY libgonet/ DESTINATION "include/libgonet" FILES_MATCHING PATTERN "*.h")

if (NOT NO_SUB)
    option(ENABLE_BOOST_COROUTINE "use boost coroutine" ON)
    add_subdirectory("third_party/libgo")
endif()
