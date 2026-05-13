#include "Streamer.hpp"
#include <cstddef>
#include <cstdint>
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
#include <thread>

void Streamer::startStream() {
  gst_init(0, nullptr);

  ssrc = static_cast<uint32_t>(std::rand());

  connMan.init();
  connMan.configureTrack(ssrc);

  if (!constructPipeline()) {
    return;
  }
  if (!startPipeline()) {
    return;
  }

  captureThreadRunning = true;
  captureThread = std::thread(&Streamer::captureData, this);

  /* Wait until error or EOS */
  bus = gst_element_get_bus(pipeline);
  msg = gst_bus_timed_pop_filtered(
      bus, GST_CLOCK_TIME_NONE,
      (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

  /* Parse message */
  if (msg != NULL) {
    GError *err;
    gchar *debug_info;

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error(msg, &err, &debug_info);
      g_printerr("Error received from element %s: %s\n",
                 GST_OBJECT_NAME(msg->src), err->message);
      g_printerr("Debugging information: %s\n",
                 debug_info ? debug_info : "none");
      g_clear_error(&err);
      g_free(debug_info);
      break;
    case GST_MESSAGE_EOS:
      g_print("End-Of-Stream reached.\n");
      break;
    default:
      /* We should not reach here because we only asked for ERRORs and EOS */
      g_printerr("Unexpected message received.\n");
      break;
    }
    gst_message_unref(msg);
  }

  /* Free resources */
  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
};

void Streamer::stopStream() {
  captureThreadRunning = false;
  if (captureThread.joinable()) {
    captureThread.join();
  }
}

bool Streamer::constructPipeline() {
  pipeline = gst_pipeline_new("dronecam");
  if (std::string("RPI") == PLATFORM) {
    createProdElements();
  } else {
    createDevElements();
  }

  parser = gst_element_factory_make("h264parse", "parser");
  queue = gst_element_factory_make("queue", "queue");
  packetizer = gst_element_factory_make("rtph264pay", "packetizer");
  sink = (GstAppSink *)gst_element_factory_make("appsink", "sink");

  g_object_set(parser, "config-interval", -1, NULL);
  g_object_set(queue, "max-size-buffers", 2, NULL);
  g_object_set(queue, "leaky", 2, NULL);
  g_object_set(packetizer, "ssrc", ssrc, NULL);
  g_object_set(packetizer, "pt", 96, NULL);
  g_object_set(sink, "sync", FALSE, NULL);

  if (std::string("RPI") == PLATFORM) {
    gst_bin_add_many(GST_BIN(pipeline), source, source_cap_filter, encoder,
                     encoder_cap_filter, parser, queue, packetizer, sink, NULL);
    gst_element_link_many(source, source_cap_filter, encoder,
                          encoder_cap_filter, parser, queue, packetizer, sink,
                          NULL);
  } else {
    gst_bin_add_many(GST_BIN(pipeline), source, converter, encoder, parser,
                     queue, packetizer, sink, NULL);
    gst_element_link_many(source, converter, encoder, parser, queue, packetizer,
                          sink, NULL);
  }
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

  GstCaps *src_caps = gst_caps_from_string(
      "video/x-raw,format=NV12,framerate=10/"
      "1,width=1280,height=720,colorimetry=bt709,interlace-mode=(string)"
      "progressive,level=(string)4.1,profile=constained-baseline");
  GstCaps *encoder_caps = gst_caps_from_string(
      "video/x-h264,profile=constrained-baseline,level=(string)4.1");
  GstStructure *extra_controls =
      gst_structure_from_string("controls,video_gop_size=10,"
                                "repeat_sequence_header=1,"
                                "video_bitrate_mode=1,"
                                "video_bitrate=1500000,"
                                "h264_i_frame_period=5,"
                                "h264_profile=1,"
                                "h264_level=12",
                                NULL);

  g_object_set(source, "do-timestamp", TRUE, NULL);
  g_object_set(encoder, "extra-controls", extra_controls, NULL);
  g_object_set(source_cap_filter, "caps", src_caps, NULL);
  g_object_set(encoder_cap_filter, "caps", encoder_caps, NULL);
}

void Streamer::createDevElements() {
  source = gst_element_factory_make("v4l2src", "source");
  converter = gst_element_factory_make("videoconvert", "converter");
  encoder = gst_element_factory_make("x264enc", "encoder");

  if (!source || !converter || !encoder) {
    g_printerr(
        "Not all development specific pipeline elements could be created.\n");
    return;
  }

  g_object_set(source, "device", V4L2_DEV, NULL); // Defined as a cmake var
}

bool Streamer::startPipeline() {
  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    GstBus *diag_bus = gst_element_get_bus(pipeline);
    if (diag_bus) {
      GstMessage *m =
          gst_bus_timed_pop_filtered(diag_bus, GST_SECOND, GST_MESSAGE_ERROR);
      if (m) {
        GError *err = nullptr;
        gchar *dbg = nullptr;
        gst_message_parse_error(m, &err, &dbg);
        g_printerr("Pipeline error from element %s: %s\n",
                   m->src ? GST_OBJECT_NAME(m->src) : "?",
                   err ? err->message : "?");
        if (dbg) {
          g_printerr("Debugging information: %s\n", dbg);
        }
        g_clear_error(&err);
        g_free(dbg);
        gst_message_unref(m);
      }
      gst_object_unref(diag_bus);
    }
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(pipeline);
    pipeline = nullptr;
    source = nullptr;
    parser = nullptr;
    packetizer = nullptr;
    sink = nullptr;
    return false;
  }
  return true;
}

void Streamer::captureData() {
  GstSample *sample;
  while (captureThreadRunning && !gst_app_sink_is_eos((GstAppSink *)sink)) {
    sample = gst_app_sink_try_pull_sample((GstAppSink *)sink, 10000000);

    if (sample == nullptr) {
      continue;
    }

    GstBuffer *sampleBuffer = gst_sample_get_buffer(sample);
    if (sampleBuffer) {
      GstMapInfo map;
      if (gst_buffer_map(sampleBuffer, &map, GST_MAP_READ)) {
        const char *mapData = reinterpret_cast<char *>(map.data);
        connMan.sendPacket(mapData, map.size);

        gst_buffer_unmap(sampleBuffer, &map);
      }
    }

    gst_sample_unref(sample);
  }
}
