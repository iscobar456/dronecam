#include "Streamer.hpp"
#include <cstddef>
#include <gst/gstbuffer.h>
#include <gst/gstmemory.h>
#include <gst/gstsample.h>

void Streamer::startStream() {
  gst_init(0, nullptr);

  constructPipeline();
  startPipeline();

  wrtcManager = WebRTCManager();
  wrtcManager.init();
  captureData();

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

void Streamer::constructPipeline() {
  pipeline = gst_pipeline_new("dronecam");
  source = gst_element_factory_make("v4l2src", "source");
  parser = gst_element_factory_make("h264parse", "parser");
  packetizer = gst_element_factory_make("rtph264pay", "packetizer");
  sink = (GstAppSink *)gst_element_factory_make("appsink", "sink");

  if (!pipeline || !source || !parser || !packetizer || !sink) {
    g_printerr("Not all elements could be created.\n");
    return;
  }

  /* Build the pipeline */
  gst_bin_add_many(GST_BIN(pipeline), source, parser, packetizer, sink, NULL);

  if (gst_element_link(source, parser) != TRUE) {
    g_printerr("Source and parser could not be linked.\n");
    gst_object_unref(pipeline);
    return;
  }

  if (gst_element_link(parser, packetizer) != TRUE) {
    g_printerr("Parser and packetizer could not be linked.\n");
    gst_object_unref(pipeline);
    return;
  }

  if (gst_element_link(packetizer, (GstElement *)sink) != TRUE) {
    g_printerr("Packetizer and sink could not be linked.\n");
    gst_object_unref(pipeline);
    return;
  }
}

bool Streamer::startPipeline() {
  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(pipeline);
    return false;
  }
  return true;
}

void Streamer::captureData() {
  GstSample *sample;
  while (!gst_app_sink_is_eos((GstAppSink *)sink)) {
    sample = gst_app_sink_try_pull_sample((GstAppSink *)sink, 10000);
    GstBuffer *sampleBuffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (gst_buffer_map(sampleBuffer, &map, GST_MAP_READ)) {
      wrtcManager.sendRTPPacket(map.data, map.size);

      gst_buffer_unmap(sampleBuffer, &map);
    }
  }
}
