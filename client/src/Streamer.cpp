#include "Streamer.hpp"
#include "fmt/format.h"
#include "utils.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <glib-object.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/gstbuffer.h>
#include <gst/gstcaps.h>
#include <gst/gstcapsfeatures.h>
#include <gst/gstelement.h>
#include <gst/gstelementfactory.h>
#include <gst/gstmemory.h>
#include <gst/gstsample.h>
#include <gst/gststructure.h>
#include <gst/gstutils.h>
#include <iostream>
#include <ostream>
#include <rtc/rtc.hpp>
#include <thread>

Streamer::Streamer() {
  gst_init(0, nullptr);
  ssrc = static_cast<uint32_t>(std::rand());
}

void Streamer::startStream(int track, int w, int h) {
  std::cout << "starting stream" << std::endl;
  std::cout << "requested dimensions: " << w << "x" << h << std::endl;

  Dimensions d = getValidDimensions(w, h);
  width = d.width;
  height = d.height;

  pipeline = gst_pipeline_new("dronecam");
  constructPipeline();
  startPipeline();

  captureThreadRunning = true;
  captureThread = std::thread(&Streamer::sendPackets, this, track);
};

void Streamer::stopStream() {
  std::cout << "Stopping camera and destroying pipeline... " << std::endl;
  captureThreadRunning = false;
  if (captureThread.joinable()) {
    captureThread.join();
  }
  destroyPipeline();
  std::cout << "done." << std::endl;
}

bool Streamer::constructPipeline() {
  if (std::string("RPI") == PLATFORM) {
    createProdElements();
  } else {
    createDevElements();
  }

  parser = gst_element_factory_make("h264parse", "parser");
  queue = gst_element_factory_make("queue", "queue");
  packetizer = gst_element_factory_make("rtph264pay", "packetizer");
  sink = (GstAppSink *)gst_element_factory_make("appsink", "sink");

  /* config-interval=-1: insert SPS/PPS with each keyframe (default). A 1s
   * fixed interval was tried and looked worse in testing; keep -1. Tune
   * freezes via pacing (ConnectionManager), queue depth below, and GOP. */
  g_object_set(parser, "config-interval", -1, NULL);
  g_object_set(queue, "max-size-buffers", 6, NULL);
  g_object_set(queue, "max-size-time", (guint64)80 * 1000 * 1000, NULL);
  g_object_set(queue, "leaky", 2, NULL);
  g_object_set(packetizer, "ssrc", ssrc, NULL);
  g_object_set(packetizer, "pt", 96, NULL);
  g_object_set(packetizer, "mtu", 1200, NULL);
  g_object_set(sink, "sync", FALSE, NULL);

  std::cout << "Linking pipeline elements... ";
  if (std::string("RPI") == PLATFORM) {
    gst_bin_add_many(GST_BIN(pipeline), source, source_cap_filter, encoder,
                     encoder_cap_filter, parser, queue, packetizer, sink, NULL);
    gst_element_link_many(source, source_cap_filter, encoder,
                          encoder_cap_filter, parser, queue, packetizer, sink,
                          NULL);
  } else {
    gst_bin_add_many(GST_BIN(pipeline), source, source_cap_filter, parser,
                     queue, packetizer, sink, NULL);
    gst_element_link_many(source, source_cap_filter, parser, queue, packetizer,
                          sink, NULL);
  }
  std::cout << "done." << std::endl;
  return true;
}

void Streamer::createProdElements() {
  source = gst_element_factory_make("libcamerasrc", "source");
  source_cap_filter =
      gst_element_factory_make("capsfilter", "source cap filter");
  encoder = gst_element_factory_make("v4l2h264enc", "encoder");
  encoder_cap_filter =
      gst_element_factory_make("capsfilter", "encoder cap filter");

  if (!source || !source_cap_filter || !encoder || !encoder_cap_filter) {
    g_printerr("Not all Raspberry Pi specific elements could be created.\n");
    return;
  }

  std::string src_caps_str = fmt::format(
      "video/x-raw,format=NV12,framerate=30/"
      "1,width={},height={},colorimetry=bt709,interlace-mode=(string)"
      "progressive",
      width, height);
  GstCaps *src_caps = gst_caps_from_string(src_caps_str.c_str());
  GstCaps *encoder_caps = gst_caps_from_string(
      "video/x-h264,profile=constrained-baseline,level=(string)3.1");
  std::string caps_string = fmt::format(
      "controls,video_gop_size={},repeat_sequence_header=1,video_bitrate_mode="
      "1,video_bitrate={},h264_i_frame_period={},h264_profile=1,h264_level=9",
      VIDEO_GOP_FRAMES, VIDEO_BITRATE, VIDEO_GOP_FRAMES);
  GstStructure *extra_controls =
      gst_structure_from_string(caps_string.c_str(), NULL);
  if (!extra_controls) {
    g_printerr("Failed to parse v4l2h264enc extra-controls string.\n");
    return;
  }

  g_object_set(encoder, "extra-controls", extra_controls, NULL);
  g_object_set(source_cap_filter, "caps", src_caps, NULL);
  g_object_set(encoder_cap_filter, "caps", encoder_caps, NULL);
}

void Streamer::createDevElements() {
  std::cout << "Creating dev pipeline elements... ";
  source = gst_element_factory_make("v4l2src", "source");
  source_cap_filter =
      gst_element_factory_make("capsfilter", "source cap filter");
  std::string src_caps_str = fmt::format("video/x-h264,framerate=30/"
                                         "1,width={},height={}",
                                         width, height);
  std::cout << src_caps_str << std::endl;
  GstCaps *src_caps = gst_caps_from_string(src_caps_str.c_str());
  g_object_set(source_cap_filter, "caps", src_caps, NULL);
  g_object_set(source, "device", V4L2_DEV, NULL); // Defined as a cmake var

  if (!source || !source_cap_filter) {
    g_printerr(
        "Not all development specific pipeline elements could be created.\n");
    return;
  }

  std::cout << "done." << std::endl;
}

bool Streamer::startPipeline() {
  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    destroyPipeline();
    return false;
  }
  return true;
}

void Streamer::destroyPipeline() {
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  pipeline = nullptr;
  source = nullptr;
  source_cap_filter = nullptr;
  encoder = nullptr;
  encoder_cap_filter = nullptr;
  parser = nullptr;
  packetizer = nullptr;
  sink = nullptr;
}

void Streamer::sendPackets(int track) {
  GstSample *sample;
  while (captureThreadRunning && !gst_app_sink_is_eos(sink)) {
    sample = gst_app_sink_try_pull_sample(sink, 10000000);

    if (sample == nullptr) {
      continue;
    }

    GstBuffer *sampleBuffer = gst_sample_get_buffer(sample);
    if (sampleBuffer) {
      GstMapInfo map;
      if (gst_buffer_map(sampleBuffer, &map, GST_MAP_READ)) {
        const char *mapData = reinterpret_cast<char *>(map.data);
        if (track != -1 && rtcIsOpen(track)) {
          rtcSendMessage(track, mapData, map.size);
        }
        gst_buffer_unmap(sampleBuffer, &map);
      }
    }

    gst_sample_unref(sample);
  }
}
