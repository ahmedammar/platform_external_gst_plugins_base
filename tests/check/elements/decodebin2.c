/* GStreamer unit tests for decodebin2
 *
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2011 Hewlett-Packard Development Company, L.P.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gst/check/gstcheck.h>
#include <gst/base/gstbaseparse.h>
#include <unistd.h>

static const gchar dummytext[] =
    "Quick Brown Fox Jumps over a Lazy Frog Quick Brown "
    "Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a Lazy Frog Quick "
    "Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a Lazy Frog "
    "Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a Lazy "
    "Frog Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a "
    "Lazy Frog Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps "
    "over a Lazy Frog Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox "
    "jumps over a Lazy Frog Quick Brown Fox Jumps over a Lazy Frog Quick Brown "
    "Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a Lazy Frog Quick "
    "Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a Lazy Frog "
    "Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a Lazy "
    "Frog Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps over a "
    "Lazy Frog Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox Jumps "
    "over a Lazy Frog Quick Brown Fox Jumps over a Lazy Frog Quick Brown Fox ";

static void
src_handoff_cb (GstElement * src, GstBuffer * buf, GstPad * pad, gpointer data)
{
  GST_BUFFER_DATA (buf) = (guint8 *) dummytext;
  GST_BUFFER_SIZE (buf) = sizeof (dummytext);
  GST_BUFFER_OFFSET (buf) = 0;
  GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_READONLY);
}

static void
decodebin_new_decoded_pad_cb (GstElement * decodebin, GstPad * pad,
    gboolean last, gboolean * p_flag)
{
  /* we should not be reached */
  fail_unless (decodebin == NULL, "new-decoded-pad should not be emitted");
}

/* make sure that decodebin errors out instead of creating a new decoded pad
 * if the entire stream is a plain text file */
GST_START_TEST (test_text_plain_streams)
{
  GstElement *pipe, *src, *decodebin;
  GstMessage *msg;

  pipe = gst_pipeline_new (NULL);
  fail_unless (pipe != NULL, "failed to create pipeline");

  src = gst_element_factory_make ("fakesrc", "src");
  fail_unless (src != NULL, "Failed to create fakesrc element");

  g_object_set (src, "signal-handoffs", TRUE, NULL);
  g_object_set (src, "num-buffers", 1, NULL);
  g_object_set (src, "can-activate-pull", FALSE, NULL);
  g_signal_connect (src, "handoff", G_CALLBACK (src_handoff_cb), NULL);

  decodebin = gst_element_factory_make ("decodebin2", "decodebin");
  fail_unless (decodebin != NULL, "Failed to create decodebin element");

  g_signal_connect (decodebin, "new-decoded-pad",
      G_CALLBACK (decodebin_new_decoded_pad_cb), NULL);

  fail_unless (gst_bin_add (GST_BIN (pipe), src));
  fail_unless (gst_bin_add (GST_BIN (pipe), decodebin));
  fail_unless (gst_element_link (src, decodebin), "can't link src<->decodebin");

  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  /* it's push-based, so should be async */
  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);

  /* it should error out at some point */
  msg = gst_bus_poll (GST_ELEMENT_BUS (pipe), GST_MESSAGE_ERROR, -1);
  fail_unless (msg != NULL);
  fail_unless (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR);
  gst_message_unref (msg);

  gst_element_set_state (pipe, GST_STATE_NULL);
  gst_object_unref (pipe);
}

GST_END_TEST;

static void
new_decoded_pad_plug_fakesink_cb (GstElement * decodebin, GstPad * srcpad,
    gboolean last, GstElement * pipeline)
{
  GstElement *sink;
  GstPad *sinkpad;

  GST_LOG ("Linking fakesink");

  sink = gst_element_factory_make ("fakesink", "sink");
  fail_unless (sink != NULL, "Failed to create fakesink element");

  gst_bin_add (GST_BIN (pipeline), sink);

  sinkpad = gst_element_get_static_pad (sink, "sink");
  fail_unless_equals_int (gst_pad_link (srcpad, sinkpad), GST_PAD_LINK_OK);
  gst_object_unref (sinkpad);

  gst_element_set_state (sink, GST_STATE_PLAYING);
}

GST_START_TEST (test_reuse_without_decoders)
{
  GstElement *pipe, *src, *decodebin, *sink;

  pipe = gst_pipeline_new (NULL);
  fail_unless (pipe != NULL, "failed to create pipeline");

  src = gst_element_factory_make ("audiotestsrc", "src");
  fail_unless (src != NULL, "Failed to create audiotestsrc element");

  decodebin = gst_element_factory_make ("decodebin2", "decodebin");
  fail_unless (decodebin != NULL, "Failed to create decodebin element");

  g_signal_connect (decodebin, "new-decoded-pad",
      G_CALLBACK (new_decoded_pad_plug_fakesink_cb), pipe);

  fail_unless (gst_bin_add (GST_BIN (pipe), src));
  fail_unless (gst_bin_add (GST_BIN (pipe), decodebin));
  fail_unless (gst_element_link (src, decodebin), "can't link src<->decodebin");

  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  /* it's push-based, so should be async */
  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);

  /* wait for state change to complete */
  fail_unless_equals_int (gst_element_get_state (pipe, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  /* there shouldn't be any errors */
  fail_if (gst_bus_poll (GST_ELEMENT_BUS (pipe), GST_MESSAGE_ERROR, 0) != NULL);

  /* reset */
  gst_element_set_state (pipe, GST_STATE_READY);

  sink = gst_bin_get_by_name (GST_BIN (pipe), "sink");
  gst_bin_remove (GST_BIN (pipe), sink);
  gst_element_set_state (sink, GST_STATE_NULL);
  gst_object_unref (sink);

  GST_LOG ("second try");

  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_READY),
      GST_STATE_CHANGE_SUCCESS);
  /* it's push-based, so should be async */
  fail_unless_equals_int (gst_element_set_state (pipe, GST_STATE_PAUSED),
      GST_STATE_CHANGE_ASYNC);

  /* wait for state change to complete */
  fail_unless_equals_int (gst_element_get_state (pipe, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  /* there shouldn't be any errors */
  fail_if (gst_bus_poll (GST_ELEMENT_BUS (pipe), GST_MESSAGE_ERROR, 0) != NULL);

  gst_element_set_state (pipe, GST_STATE_NULL);
  gst_object_unref (pipe);
}

GST_END_TEST;

/* Fake mp3 parser for test */
typedef GstBaseParse TestMpegAudioParse;
typedef GstBaseParseClass TestMpegAudioParseClass;

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg, mpegversion=1, layer=[1,3], parsed=(b)true")
    );

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg, mpegversion=1, parsed=(bool) { false, true }")
    );

static GType test_mpeg_audio_parse_get_type (void);
static gboolean test_mpeg_audio_parse_start (GstBaseParse * parse);
static gboolean test_mpeg_audio_parse_stop (GstBaseParse * parse);
static gboolean test_mpeg_audio_parse_check_valid_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, guint * size, gint * skipsize);
static GstFlowReturn test_mpeg_audio_parse_parse_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame);

GST_BOILERPLATE (TestMpegAudioParse, test_mpeg_audio_parse, GstBaseParse,
    GST_TYPE_BASE_PARSE);

static void
test_mpeg_audio_parse_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));

  gst_element_class_set_details_simple (element_class, "MPEG1 Audio Parser",
      "Codec/Parser/Audio", "Pretends to parse mpeg1 audio stream",
      "Foo Bar <foo@bar.com>");
}

static void
test_mpeg_audio_parse_class_init (TestMpegAudioParseClass * klass)
{
  GstBaseParseClass *parse_class = GST_BASE_PARSE_CLASS (klass);

  parse_class->start = test_mpeg_audio_parse_start;
  parse_class->stop = test_mpeg_audio_parse_stop;
  parse_class->check_valid_frame = test_mpeg_audio_parse_check_valid_frame;
  parse_class->parse_frame = test_mpeg_audio_parse_parse_frame;
}

static gint num_parse_instances = 0;

static void
test_mpeg_audio_parse_init (TestMpegAudioParse * mp3parse,
    TestMpegAudioParseClass * klass)
{
  /* catch decodebin plugging parsers in a loop early */
  fail_unless (++num_parse_instances < 10);
}

static gboolean
test_mpeg_audio_parse_start (GstBaseParse * parse)
{
  gst_base_parse_set_min_frame_size (parse, 6);
  return TRUE;
}

static gboolean
test_mpeg_audio_parse_stop (GstBaseParse * parse)
{
  return TRUE;
}

static gboolean
test_mpeg_audio_parse_check_valid_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, guint * framesize, gint * skipsize)
{
  const guint8 *data = GST_BUFFER_DATA (frame->buffer);

  if ((GST_READ_UINT16_BE (data) & 0xffe0) == 0xffe0) {
    /* this framesize is hard-coded for ../test.mp3 */
    *framesize = 1045;
    return TRUE;
  } else {
    *skipsize = 1;
    return FALSE;
  }
}

static GstFlowReturn
test_mpeg_audio_parse_parse_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame)
{
  if (GST_BUFFER_OFFSET (frame->buffer) == 0) {
    GstCaps *caps;

    caps = gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 1,
        "mpegaudioversion", G_TYPE_INT, 1, "layer", G_TYPE_INT, 3,
        "rate", G_TYPE_INT, 44100, "channels", G_TYPE_INT, 2, NULL);
    gst_buffer_set_caps (frame->buffer, caps);
    gst_pad_set_caps (GST_BASE_PARSE_SRC_PAD (parse), caps);
    gst_caps_unref (caps);
  }
  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "testmpegaudioparse", GST_RANK_NONE,
      test_mpeg_audio_parse_get_type ());
}

GST_START_TEST (test_mp3_parser_loop)
{
  GstStateChangeReturn sret;
  GstPluginFeature *feature;
  GstMessage *msg;
  GstElement *pipe, *src, *dec;
  gchar *path;

  num_parse_instances = 0;

  gst_plugin_register_static (GST_VERSION_MAJOR, GST_VERSION_MINOR,
      "fakemp3parse", "fakemp3parse", plugin_init, VERSION, "LGPL",
      "gst-plugins-base", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);

  feature = gst_default_registry_find_feature ("testmpegaudioparse",
      GST_TYPE_ELEMENT_FACTORY);

  gst_plugin_feature_set_rank (feature, GST_RANK_PRIMARY + 100);

  pipe = gst_pipeline_new (NULL);

  src = gst_element_factory_make ("filesrc", NULL);
  fail_unless (src != NULL);

  path = g_build_filename (GST_TEST_FILES_PATH, "test.mp3", NULL);
  g_object_set (src, "location", path, NULL);
  g_free (path);

  dec = gst_element_factory_make ("decodebin2", NULL);
  fail_unless (dec != NULL);

  gst_bin_add_many (GST_BIN (pipe), src, dec, NULL);
  gst_element_link_many (src, dec, NULL);

  sret = gst_element_set_state (pipe, GST_STATE_PLAYING);
  fail_unless_equals_int (sret, GST_STATE_CHANGE_ASYNC);

  /* wait for unlinked error */
  msg = gst_bus_timed_pop_filtered (GST_ELEMENT_BUS (pipe),
      GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR);
  gst_message_unref (msg);

  gst_element_set_state (pipe, GST_STATE_NULL);
  gst_object_unref (pipe);

  /* make sure out parser got plugged at all though */
  fail_unless_equals_int (num_parse_instances, 1);

  /* don't want to interfere with any other of the other tests */
  gst_plugin_feature_set_rank (feature, GST_RANK_NONE);
  gst_object_unref (feature);
}

GST_END_TEST;

static Suite *
decodebin2_suite (void)
{
  Suite *s = suite_create ("decodebin2");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_text_plain_streams);
  tcase_add_test (tc_chain, test_reuse_without_decoders);
  tcase_add_test (tc_chain, test_mp3_parser_loop);

  return s;
}

GST_CHECK_MAIN (decodebin2);
