#pragma once

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
  GstElement *pipeline = nullptr, *source = nullptr,
             *source_cap_filter = nullptr, *converter = nullptr,
             *encoder = nullptr, *encoder_cap_filter = nullptr,
             *parser = nullptr, *queue = nullptr, *packetizer = nullptr;
  GstAppSink *sink = nullptr;
  GstBus *bus = nullptr;
  GstMessage *msg = nullptr;
  GstStateChangeReturn ret;

  std::thread captureThread;
  std::atomic<bool> captureThreadRunning{false};

  uint32_t ssrc;
  int width = FOOTAGE_WIDTH;
  int height = FOOTAGE_HEIGHT;

  bool constructPipeline();
  void createProdElements();
  void createDevElements();
  bool startPipeline();
  void sendPackets(int track);
  void destroyPipeline();

public:
  Streamer();
  void startStream(int track, int width, int height);
  void stopStream();
  void setWidth(int width) { width = width; };
  void setHeight(int height) { height = height; };
  uint32_t getSsrc() { return ssrc; }
};
