#pragma once

#include "ConnectionManager.hpp"
#include <cstdint>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/gstmessage.h>
#include <gst/gstsample.h>
#include <gst/gstutils.h>

class Streamer {
private:
  GstElement *pipeline, *source, *parser, *packetizer;
  GstAppSink *sink;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;

  ConnectionManager wrtcManager;

  uint32_t ssrc;

  void constructPipeline();
  bool startPipeline();
  void captureData();

public:
  void startStream();
};
