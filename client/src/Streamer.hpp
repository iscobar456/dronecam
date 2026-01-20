#pragma once

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/gstmessage.h>
#include <gst/gstsample.h>
#include <gst/gstutils.h>
#include "WebRTCManager.hpp"

class Streamer {
private:
  GstElement *pipeline, *source, *parser, *packetizer;
  GstAppSink *sink;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;

  WebRTCManager wrtcManager;

  void constructPipeline();
  bool startPipeline();
  void captureData();

public:
  void startStream();
};
