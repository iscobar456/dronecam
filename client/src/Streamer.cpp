#include "Streamer.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <glib-object.h>
#include <glib.h>
#include <gst/gstbuffer.h>
#include <gst/gstelementfactory.h>
#include <gst/gstmemory.h>
#include <gst/gstsample.h>
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
  source = gst_element_factory_make("v4l2src", "source");
  const char *v4l2_dev = V4L2_DEV; // Defined as a cmake variable
  g_object_set(source, "device", v4l2_dev, NULL);
  if (!g_file_test(v4l2_dev, G_FILE_TEST_EXISTS)) {
    g_printerr(
        "V4L2 device does not exist: %s\n"
        "Set DRONECAM_V4L2_DEVICE to a node under /dev (e.g. /dev/video1). "
        "List devices: ls -la /dev/video*\n",
        v4l2_dev);
    gst_object_unref(source);
    source = nullptr;
    gst_object_unref(pipeline);
    pipeline = nullptr;
    return false;
  }
  converter = gst_element_factory_make("videoconvert", "converter");
  if (std::string(PLATFORM) == "RPI") {
    encoder = gst_element_factory_make("v4l2h264enc", "encoder");
  } else {
    encoder = gst_element_factory_make("x264enc", "encoder");
  }
  parser = gst_element_factory_make("h264parse", "parser");
  packetizer = gst_element_factory_make("rtph264pay", "packetizer");
  g_object_set(packetizer, "ssrc", ssrc, NULL);
  g_object_set(packetizer, "pt", 96, NULL);
  sink = (GstAppSink *)gst_element_factory_make("appsink", "sink");

  if (!pipeline || !source || !converter || !encoder || !parser ||
      !packetizer || !sink) {
    g_printerr("Not all elements could be created.\n");
    return false;
  }

  /* Build the pipeline */
  gst_bin_add_many(GST_BIN(pipeline), source, converter, encoder, parser,
                   packetizer, sink, NULL);

  if (std::string(PLATFORM) == "RPI") {
    if (gst_element_link(source, converter) != TRUE) {
      g_printerr("Source and converter could not be linked.\n");
      gst_object_unref(pipeline);
      return false;
    }
  } else {
    if (gst_element_link(source, converter) != TRUE) {
      g_printerr("Source and converter could not be linked.\n");
      gst_object_unref(pipeline);
      return false;
    }

    if (gst_element_link(converter, encoder) != TRUE) {
      g_printerr("Converter and encoder could not be linked.\n");
      gst_object_unref(pipeline);
      return false;
    }
  }

  if (gst_element_link(encoder, parser) != TRUE) {
    g_printerr("Encoder and parser could not be linked.\n");
    gst_object_unref(pipeline);
    return false;
  }

  if (gst_element_link(parser, packetizer) != TRUE) {
    g_printerr("Parser and packetizer could not be linked.\n");
    gst_object_unref(pipeline);
    return false;
  }

  if (gst_element_link(packetizer, (GstElement *)sink) != TRUE) {
    g_printerr("Packetizer and sink could not be linked.\n");
    gst_object_unref(pipeline);
    return false;
  }

  return true;
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
