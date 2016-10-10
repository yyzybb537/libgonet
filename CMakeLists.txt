cmake_minimum_required(VERSION 2.8)


##################################################################################
project(libgonet)

if (CMAKE_BUILD_TYPE)
else()
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

option(ENABLE_SSL "enable openssl" OFF)
if (ENABLE_SSL)
    set(ENABLE_SSL 1)
else()
    set(ENABLE_SSL 0)
endif()
configure_file(${PROJECT_SOURCE_DIR}/libgonet/cmake_config.h.in ${PROJECT_SOURCE_DIR}/libgonet/cmake_config.h)

message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("ENABLE_SSL: ${ENABLE_SSL}")

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Werror -fPIC -g -pg")
set(CMAKE_CXX_FLAGS_DEBUG "-g -pg ${CMAKE_CXX_FLAGS}")

aux_source_directory(libgonet SRC_LIST)
if (ENABLE_SYSTEM_LIBGO)
else()
	include_directories("${PROJECT_SOURCE_DIR}/libgonet")
	include_directories("${PROJECT_SOURCE_DIR}/third_party/libgo")
	include_directories("${PROJECT_SOURCE_DIR}/third_party/libgo/libgo")
	include_directories("${PROJECT_SOURCE_DIR}/third_party/libgo/libgo/linux")
endif()
set(TARGET "libgonet")

add_library("${TARGET}" STATIC ${SRC_LIST})

#set(CMAKE_INSTALL_PREFIX "/usr/local")
install(TARGETS ${TARGET} LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")
install(DIRECTORY libgonet/ DESTINATION "include/libgonet" FILES_MATCHING PATTERN "*.h")

if (NOT NO_SUB)
    option(ENABLE_BOOST_CONTEXT "use boost coroutine" ON)
	if (ENABLE_SYSTEM_LIBGO)
	else()
		add_subdirectory("third_party/libgo")
	endif()
endif()
