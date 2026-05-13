# set(DEBUG PRIVATE 1)
# set(IS_ON_DEV_MACHINE PRIVATE 1)
set(WEBSOCKET_URL "wss://dcsignaling.isaacspencer.com")
set(PLATFORM "RPI")
set(V4L2_DEV "/dev/video0")
# H.264 target bitrate (bits/s) for v4l2h264enc and RTP send pacing headroom
set(VIDEO_BITRATE 800000)
