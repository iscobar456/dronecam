cmake_minimum_required(VERSION 3.14)
project(dronecam)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include("${CMAKE_SOURCE_DIR}/vars.default.cmake")
if(EXISTS "${CMAKE_SOURCE_DIR}/vars.cmake")
    include("${CMAKE_SOURCE_DIR}/vars.cmake")
endif()

message(STATUS "DEBUG=${DEBUG}")
message(STATUS "IS_ON_DEV_MACHINE=${IS_ON_DEV_MACHINE}")

add_subdirectory(src)
add_subdirectory(external/libdatachannel)
