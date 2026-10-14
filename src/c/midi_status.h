// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <pebble.h>

#define MIDI_DEVICE_NAME_LEN 32
#define MIDI_STATUS_PERSIST_KEY 302         // 300/301 = TWT status; pick the next free pair
#define MIDI_STATUS_VERSION_PERSIST_KEY 303
#define MIDI_STATUS_VERSION 1               // bump whenever the MidiStatus struct layout changes

typedef struct {
  bool isRecording;
  char deviceName[MIDI_DEVICE_NAME_LEN + 1];
  int32_t recStartEpoch; // epoch seconds when recording began (0 if not recording)
} MidiStatus;

extern MidiStatus midi_status;

void MidiStatus_load();   // read from persistent storage into midi_status
void MidiStatus_save();   // write midi_status to persistent storage

// Create/destroy the status-line layer as a child of `parent`. Created HIDDEN; main.c shows it.
void MidiStatus_initLayer(Layer* parent, GRect frame);
void MidiStatus_deinitLayer();
void MidiStatus_setHidden(bool hidden);
void MidiStatus_setFrame(GRect frame);
void MidiStatus_redraw(); // recompute "REC m:ss  Device" and mark dirty
