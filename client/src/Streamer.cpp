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
#include <gst/gstelementfactory.h>
#include <gst/gstmemory.h>
#include <gst/gstsample.h>
#include <gst/gststructure.h>
#include <gst/gstutils.h>
#include <iostream>
#include <rtc/rtc.hpp>
#include <thread>

const int MAX_PIXELS = 921600;

Streamer::Streamer() {
  DEBUG_MSG("enter");
  gst_init(0, nullptr);
  DEBUG_MSG("gst_init done");
  ssrc = static_cast<uint32_t>(std::rand());
  DEBUG_MSG("exit ssrc=" << ssrc);
}

void Streamer::startStream(int track, int w, int h) {
  DEBUG_MSG("enter track=" << track << " w=" << w << " h=" << h);

  while (w * h > MAX_PIXELS) {
    w--;
    h--;
  }
  width = w;
  height = h;
  DEBUG_MSG("dimensions width=" << width << " height=" << height);

  pipeline = gst_pipeline_new("dronecam");
  DEBUG_MSG("pipeline=" << pipeline);
  constructPipeline();
  DEBUG_MSG("constructPipeline done");
  startPipeline();
  DEBUG_MSG("startPipeline done");

  captureThreadRunning = true;
  DEBUG_MSG("spawning capture thread track=" << track);
  captureThread = std::thread(&Streamer::sendPackets, this, track);
  DEBUG_MSG("capture thread spawned");

  /* Wait until error or EOS */
  bus = gst_element_get_bus(pipeline);
  DEBUG_MSG("waiting on bus...");
  msg = gst_bus_timed_pop_filtered(
      bus, GST_CLOCK_TIME_NONE,
      (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
  DEBUG_MSG("bus message received msg=" << msg);

  /* Parse message */
  if (msg != NULL) {
    GError *err;
    gchar *debug_info;

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
      DEBUG_MSG("GST_MESSAGE_ERROR");
      gst_message_parse_error(msg, &err, &debug_info);
      g_printerr("Error received from element %s: %s\n",
                 GST_OBJECT_NAME(msg->src), err->message);
      g_printerr("Debugging information: %s\n",
                 debug_info ? debug_info : "none");
      g_clear_error(&err);
      g_free(debug_info);
      break;
    case GST_MESSAGE_EOS:
      DEBUG_MSG("GST_MESSAGE_EOS");
      g_print("End-Of-Stream reached.\n");
      break;
    default:
      DEBUG_MSG("unexpected message type");
      /* We should not reach here because we only asked for ERRORs and EOS */
      g_printerr("Unexpected message received.\n");
      break;
    }
    gst_message_unref(msg);
  }

  /* Free resources */
  DEBUG_MSG("cleaning up pipeline");
  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  DEBUG_MSG("exit");
};

void Streamer::stopStream() {
  DEBUG_MSG("enter");
  captureThreadRunning = false;
  if (captureThread.joinable()) {
    DEBUG_MSG("joining capture thread");
    captureThread.join();
    DEBUG_MSG("capture thread joined");
  }
  DEBUG_MSG("exit");
}

bool Streamer::constructPipeline() {
  DEBUG_MSG("enter pipeline=" << pipeline);
  if (std::string("RPI") == PLATFORM) {
    DEBUG_MSG("platform RPI");
    createProdElements();
  } else {
    DEBUG_MSG("platform dev");
    createDevElements();
  }

  DEBUG_MSG("creating parser/queue/packetizer/sink");
  parser = gst_element_factory_make("h264parse", "parser");
  queue = gst_element_factory_make("queue", "queue");
  packetizer = gst_element_factory_make("rtph264pay", "packetizer");
  sink = (GstAppSink *)gst_element_factory_make("appsink", "sink");
  DEBUG_MSG("elements parser=" << parser << " queue=" << queue
                               << " packetizer=" << packetizer << " sink=" << sink);

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

  DEBUG_MSG("linking pipeline elements");
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
  DEBUG_MSG("exit");
  return true;
}

void Streamer::createProdElements() {
  DEBUG_MSG("enter width=" << width << " height=" << height);
  source = gst_element_factory_make("libcamerasrc", "source");
  source_cap_filter =
      gst_element_factory_make("capsfilter", "source cap filter");
  encoder = gst_element_factory_make("v4l2h264enc", "encoder");
  encoder_cap_filter =
      gst_element_factory_make("capsfilter", "encoder cap filter");

  if (!source || !source_cap_filter || !encoder || !encoder_cap_filter) {
    DEBUG_MSG("failed to create prod elements");
    g_printerr("Not all Raspberry Pi specific elements could be created.\n");
    return;
  }
  DEBUG_MSG("prod elements created");

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
    DEBUG_MSG("failed to parse extra-controls");
    g_printerr("Failed to parse v4l2h264enc extra-controls string.\n");
    return;
  }

  g_object_set(encoder, "extra-controls", extra_controls, NULL);
  g_object_set(source_cap_filter, "caps", src_caps, NULL);
  g_object_set(encoder_cap_filter, "caps", encoder_caps, NULL);
  DEBUG_MSG("exit");
}

void Streamer::createDevElements() {
  DEBUG_MSG("enter width=" << width << " height=" << height);
  std::cout << "Creating dev pipeline elements... ";
  source = gst_element_factory_make("v4l2src", "source");
  source_cap_filter =
      gst_element_factory_make("capsfilter", "source cap filter");
  std::string src_caps_str = fmt::format("video/x-h264,framerate=30/"
                                         "1,width={},height={}",
                                         width, height);
  GstCaps *src_caps = gst_caps_from_string(src_caps_str.c_str());
  g_object_set(source_cap_filter, "caps", src_caps, NULL);
  g_object_set(source, "device", V4L2_DEV, NULL); // Defined as a cmake var

  if (!source || !source_cap_filter) {
    DEBUG_MSG("failed to create dev elements");
    g_printerr(
        "Not all development specific pipeline elements could be created.\n");
    return;
  }

  std::cout << "done." << std::endl;
  DEBUG_MSG("exit source=" << source << " source_cap_filter=" << source_cap_filter);
}

bool Streamer::startPipeline() {
  DEBUG_MSG("enter pipeline=" << pipeline);
  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  DEBUG_MSG("set_state ret=" << ret);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    DEBUG_MSG("failed to reach PLAYING");
    g_printerr("Unable to set the pipeline to the playing state.\n");
    destroyPipeline();
    return false;
  }
  DEBUG_MSG("exit");
  return true;
}

void Streamer::destroyPipeline() {
  DEBUG_MSG("enter pipeline=" << pipeline);
  gst_object_unref(pipeline);
  pipeline = nullptr;
  source = nullptr;
  source_cap_filter = nullptr;
  encoder = nullptr;
  encoder_cap_filter = nullptr;
  parser = nullptr;
  packetizer = nullptr;
  sink = nullptr;
  DEBUG_MSG("exit");
}

void Streamer::sendPackets(int track) {
  DEBUG_MSG("enter track=" << track << " sink=" << sink);
  GstSample *sample;
  int packetCount = 0;
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
        if (track == -1 || !rtcIsOpen(track)) {
          DEBUG_MSG("track not ready, exiting loop track=" << track
                     << " rtcIsOpen=" << (track != -1 ? rtcIsOpen(track) : -1));
          return;
        }
        if (packetCount == 0) {
          DEBUG_MSG("sending first packet size=" << map.size);
        }
        rtcSendMessage(track, mapData, map.size);
        packetCount++;
        if (packetCount % 300 == 0) {
          DEBUG_MSG("sent " << packetCount << " packets");
        }

        gst_buffer_unmap(sampleBuffer, &map);
      } else {
        DEBUG_MSG("gst_buffer_map failed");
      }
    } else {
      DEBUG_MSG("sampleBuffer is null");
    }

    gst_sample_unref(sample);
  }
  DEBUG_MSG("exit packetCount=" << packetCount
                                << " captureThreadRunning=" << captureThreadRunning);
}
