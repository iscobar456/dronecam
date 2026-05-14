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
  GstElement *pipeline = nullptr, *source = nullptr, *source_cap_filter = nullptr,
      *converter = nullptr, *encoder = nullptr, *encoder_cap_filter = nullptr,
      *parser = nullptr, *queue = nullptr, *packetizer = nullptr;
  GstAppSink *sink = nullptr;
  GstBus *bus = nullptr;
  GstMessage *msg = nullptr;
  GstStateChangeReturn ret;

  ConnectionManager connMan;
  std::thread captureThread;
  std::atomic<bool> captureThreadRunning{false};

  uint32_t ssrc;

  bool constructPipeline();
  void createProdElements();
  void createDevElements();
  bool startPipeline();
  void captureData();

public:
  Streamer();
  void startStream();
  void stopStream();
};
