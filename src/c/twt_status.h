// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <pebble.h>

#define TWT_TASK_NAME_LEN 32
#define TWT_STATUS_PERSIST_KEY 300         // unused by TimeStyle (100=settings, 223=weather, 200=migration)
#define TWT_STATUS_VERSION_PERSIST_KEY 301 // guards against reading a stale blob into a changed layout
#define TWT_STATUS_VERSION 5               // bump whenever the TwtStatus struct layout changes

// Height in px reserved at the bottom of the screen for the status line.
// One line (used by the MIDI status strip).
#define TWT_STATUS_HEIGHT 34
// Two lines (work time above the task name) for the TWT status strip.
#define TWT_STATUS_HEIGHT_2LINE 62

typedef struct {
  bool isTracking;
  int32_t taskId;
  char taskName[TWT_TASK_NAME_LEN + 1];
  int32_t workedBeforeMin;       // worked minutes today excluding the running segment
  int32_t taskWorkedBeforeMin;   // worked minutes today on the CURRENT task, excluding the running segment
  int32_t segmentStartEpoch;     // epoch seconds of current clock-in (0 if not tracking)
  int32_t dailyTargetMin;        // configured daily work-time target in minutes (0 = none/unset)
  int32_t taskBudgetMin;         // budget for the CURRENT task in minutes (0 = no budget)
  int32_t taskTotalBeforeMin;    // all-time worked on the CURRENT task, excluding the running segment
  int32_t dayGrossBeforeMin;     // GROSS (no auto-pause) worked minutes today, excluding the running
                                 // segment; denominator for the unbudgeted task percent so it is
                                 // gross/gross like the per-task numerator. 0 = not sent (older phone
                                 // app) -> fall back to the net day total.
} TwtStatus;

extern TwtStatus twt_status;

// true on platforms where we render the status line (rect, non-aplite)
bool TwtStatus_isSupported();

void TwtStatus_load();   // read from persistent storage into twt_status
void TwtStatus_save();   // write twt_status to persistent storage

// Create/destroy the status-line layer as a child of `parent`, occupying the bottom strip.
// The layer is created HIDDEN; main.c shows it only while tracking.
void TwtStatus_initLayer(Layer* parent, GRect frame);
void TwtStatus_deinitLayer();

// Show/hide the status line (used to revert to the original watchface when not tracking).
void TwtStatus_setHidden(bool hidden);

// Reposition the status line (so it can be inset to clear the sidebar while tracking).
void TwtStatus_setFrame(GRect frame);

// Mark the status layer dirty (call on tick + after a message updates twt_status).
void TwtStatus_redraw();

// Worked minutes today INCLUDING the live running segment (workedBeforeMin + running).
// Shared by the status strip and the daily-target-reached vibration in main.c.
int32_t TwtStatus_workedTotalMin(void);

// All-time worked minutes on the CURRENT task INCLUDING the live running segment
// (taskTotalBeforeMin + running). Shared by the budget-reached vibration in main.c.
int32_t TwtStatus_taskTotalMin(void);

// Flash the status strip (settings.twtFlashColor) for ~5s. Called when a
// target-reached vibration fires, so the user sees which alert caused the buzz.
void TwtStatus_startFlash(void);
