find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0)
pkg_check_modules(UUID REQUIRED IMPORTED_TARGET uuid)

add_executable(dronecam main.cpp)
target_sources(dronecam PRIVATE Streamer.cpp)
target_sources(dronecam PRIVATE ConnectionManager.cpp)
target_sources(dronecam PRIVATE utils.cpp)

target_link_libraries(dronecam PUBLIC datachannel-static)
target_link_libraries(dronecam PUBLIC PkgConfig::GST)
target_link_libraries(dronecam PRIVATE PkgConfig::UUID)

target_compile_definitions(dronecam PRIVATE WEBSOCKET_URL="${WEBSOCKET_URL}")
target_compile_definitions(dronecam PRIVATE V4L2_DEV="${V4L2_DEV}")
target_compile_definitions(dronecam PRIVATE PLATFORM="${PLATFORM}")
target_compile_definitions(dronecam PRIVATE VIDEO_BITRATE=${VIDEO_BITRATE})
target_compile_definitions(dronecam PRIVATE RTP_PACING_BITRATE_MULT=${RTP_PACING_BITRATE_MULT})
target_compile_definitions(dronecam PRIVATE RTP_PACING_INTERVAL_MS=${RTP_PACING_INTERVAL_MS})
target_compile_definitions(dronecam PRIVATE VIDEO_GOP_FRAMES=${VIDEO_GOP_FRAMES})

target_include_directories(dronecam PRIVATE
    ../external/libdatachannel/deps/json/single_include
)

