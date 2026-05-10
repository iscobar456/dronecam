find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0)

add_executable(sender main.cpp)
target_sources(sender PRIVATE Streamer.cpp)
target_sources(sender PRIVATE WebRTCManager.cpp)

target_link_libraries(sender PUBLIC datachannel-static)

target_link_libraries(sender PUBLIC PkgConfig::GST)

target_compile_definitions(sender PRIVATE WEBSOCKET_URL="${WEBSOCKET_URL}")
