#pragma once

#include "ConnectionManager.hpp"
#include <atomic>
#include <cstdint>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/gstmessage.h>
#include <gst/gstsample.h>
#include <gst/gstutils.h>
#include <thread>

class Streamer {
private:
  GstElement *pipeline, *source, *parser, *packetizer;
  GstAppSink *sink;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;

  ConnectionManager connMan;
  std::thread captureThread;
  std::atomic<bool> captureThreadRunning{false};

  uint32_t ssrc;

  bool constructPipeline();
  bool startPipeline();
  void captureData();

public:
  void startStream();
  void stopStream();
};
