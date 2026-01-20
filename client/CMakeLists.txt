cmake_minimum_required(VERSION 3.14)
project(dronecam)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(EXISTS "${CMAKE_SOURCE_DIR}/vars.cmake")
    include("${CMAKE_SOURCE_DIR}/vars.cmake")
endif()

message(STATUS "DEBUG=${DEBUG}")
message(STATUS "IS_ON_DEV_MACHINE=${IS_ON_DEV_MACHINE}")

add_subdirectory(src)
add_subdirectory(external/libdatachannel)
