cmake_minimum_required(VERSION 2.8)

add_subdirectory("${PROJECT_SOURCE_DIR}/third_party/libgo")

##################################################################################
project(libgonet)
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_LIST)
include_directories("${PROJECT_SOURCE_DIR}/third_party/libgo/src")
include_directories("${PROJECT_SOURCE_DIR}/third_party/libgo/src/linux")
set(TARGET "libgonet")
add_library("${TARGET}" STATIC ${SRC_LIST})

set(CMAKE_INSTALL_PREFIX "/usr/local")
install(TARGETS ${TARGET} LIBRARY DESTINATION "lib" ARCHIVE DESTINATION "lib")
install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/ DESTINATION "include/libgonet" FILES_MATCHING PATTERN "*.h")

