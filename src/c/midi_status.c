// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include "midi_status.h"
#include "twt_status.h"   // reuse TwtStatus_isSupported() + TWT_STATUS_HEIGHT
#include "settings.h"
#include "theme.h"

MidiStatus midi_status;

static Layer* s_status_layer;
static char s_status_buffer[MIDI_DEVICE_NAME_LEN + 24];

void MidiStatus_load() {
  bool versionMatches = persist_exists(MIDI_STATUS_VERSION_PERSIST_KEY)
      && persist_read_int(MIDI_STATUS_VERSION_PERSIST_KEY) == MIDI_STATUS_VERSION;
  if (versionMatches && persist_exists(MIDI_STATUS_PERSIST_KEY)) {
    persist_read_data(MIDI_STATUS_PERSIST_KEY, &midi_status, sizeof(MidiStatus));
  } else {
    midi_status = (MidiStatus){0};
    midi_status.deviceName[0] = '\0';
#ifdef SCREENSHOT_FIXTURES
#if SCREENSHOT_SCENE == 3
    // Demo recording state for the MIDI-active appstore scene (no phone needed).
    midi_status.isRecording = true;
    strncpy(midi_status.deviceName, "Roland FP-30", MIDI_DEVICE_NAME_LEN);
    midi_status.deviceName[MIDI_DEVICE_NAME_LEN] = '\0';
    midi_status.recStartEpoch = 0;
#endif
#endif
  }
}

void MidiStatus_save() {
  persist_write_int(MIDI_STATUS_VERSION_PERSIST_KEY, MIDI_STATUS_VERSION);
  persist_write_data(MIDI_STATUS_PERSIST_KEY, &midi_status, sizeof(MidiStatus));
}

// Compose "m:ss  DeviceName" (h:mm:ss past an hour) when the recording timer is
// on (settings.midiSecondPrecision), or just "DeviceName" when off. The recording indicator is a
// drawn red dot (see status_update_proc), not text — the Pebble GOTHIC system fonts
// only cover Latin-1 + limited Latin-Extended, so a U+25CF BLACK CIRCLE (●) would
// render as a missing-glyph box.
static void build_status_text() {
  int32_t elapsed = 0;
  if (midi_status.isRecording && midi_status.recStartEpoch > 0) {
    elapsed = (int32_t)time(NULL) - midi_status.recStartEpoch;
    if (elapsed < 0) elapsed = 0;
  }
  int h = elapsed / 3600;
  int m = (elapsed % 3600) / 60;
  int s = elapsed % 60;
  if (settings.midiSecondPrecision) {
    // seconds precision: m:ss, or h:mm:ss past an hour
    if (h > 0) {
      snprintf(s_status_buffer, sizeof(s_status_buffer), "%d:%02d:%02d  %s",
               h, m, s, midi_status.deviceName);
    } else {
      snprintf(s_status_buffer, sizeof(s_status_buffer), "%d:%02d  %s",
               m, s, midi_status.deviceName);
    }
  } else {
    // timer off: hide the elapsed time entirely — just the REC dot (drawn in
    // status_update_proc) + device name.
    snprintf(s_status_buffer, sizeof(s_status_buffer), "%s", midi_status.deviceName);
  }
}

static void status_update_proc(Layer* layer, GContext* ctx) {
  GRect b = layer_get_bounds(layer);
  build_status_text();

  // Shared "status area" background (same setting as the work-time strip);
  // GColorClear = inherit watchface bg (no fill). Mirrors twt_status.c.
  if (!gcolor_equal(theme.twtStatusBgColor, GColorClear)) {
    graphics_context_set_fill_color(ctx, theme.twtStatusBgColor);
    graphics_fill_rect(ctx, b, 0, GCornerNone);
  }

  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);

  // main.c reserves a fixed (2-line) status height so the clock size is consistent
  // across status types. MIDI is a single line, so center it vertically within that
  // taller area instead of letting it float at the top.
  int lineH = TWT_STATUS_HEIGHT;        // single-line height
  int top = (b.size.h - lineH) / 2;
  if (top < 0) top = 0;

  // Recording indicator: a filled red dot (the classic REC dot), drawn rather than
  // typeset so it always renders (the system font has no ● glyph). Falls back to the
  // theme time color on b&w platforms where GColorRed has no distinct shade.
  int r = 6;
  int leftPad = 8;                      // gap from the strip's left edge to the dot
  int cx = leftPad + r;
  int cy = top + lineH / 2;
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorRed, theme.timeColor));
  graphics_fill_circle(ctx, GPoint(cx, cy), r);

  // Elapsed time + device name, after the dot, truncated with an ellipsis if long.
  int textX = cx + r + 6;
  graphics_context_set_text_color(ctx, theme.timeColor);
  graphics_draw_text(ctx, s_status_buffer, font,
      GRect(textX, top - 2, b.size.w - textX, lineH),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

void MidiStatus_redraw() {
  if (s_status_layer) layer_mark_dirty(s_status_layer);
}

void MidiStatus_setFrame(GRect frame) {
  if (s_status_layer) layer_set_frame(s_status_layer, frame);
}

void MidiStatus_initLayer(Layer* parent, GRect frame) {
  if (!TwtStatus_isSupported()) return;
  s_status_layer = layer_create(frame);
  layer_set_update_proc(s_status_layer, status_update_proc);
  layer_add_child(parent, s_status_layer);
  layer_set_hidden(s_status_layer, true); // shown by main.c only while recording
}

void MidiStatus_setHidden(bool hidden) {
  if (s_status_layer) layer_set_hidden(s_status_layer, hidden);
}

void MidiStatus_deinitLayer() {
  if (s_status_layer) {
    layer_destroy(s_status_layer);
    s_status_layer = NULL;
  }
}
