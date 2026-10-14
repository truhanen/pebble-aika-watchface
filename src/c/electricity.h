// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#pragma once
#include <pebble.h>
#include "electricity_calc.h"

// Persist keys (free range; in use elsewhere: 100, 4, 200, 223, 300-303)
#define ELEC_PERSIST_KEY_META  310
#define ELEC_PERSIST_KEY_DATA0 311
#define ELEC_PERSIST_KEY_DATA1 312
// First chunk holds 128 quarters (256 bytes = persist max); rest go to DATA1.
#define ELEC_CHUNK0_COUNT 128

typedef struct {
  uint32_t startEpoch;
  uint16_t count;
} ElectricityMeta;

typedef struct {
  uint32_t startEpoch;                 // UTC epoch (s) of prices[0]
  uint16_t count;                      // valid entries (<= ELEC_MAX_QUARTERS)
  int16_t  prices[ELEC_MAX_QUARTERS];  // 0.01 snt/kWh
} ElectricityInfo;

extern ElectricityInfo Electricity_info;

void Electricity_init();

// True once a price table has been received (restored from persist or pushed).
bool Electricity_hasData(void);
void Electricity_deinit();
void Electricity_saveData();
// out in 0.01 snt; false if now is outside the table / no data.
bool Electricity_getCurrentPrice(int16_t *out);
// out in 0.01 snt; false if the current local day has no entries.
bool Electricity_getTodayAverage(int16_t *out);

// What the cheap-electricity widgets need to render one result.
typedef struct {
  int     startHour;   // local hour (0-23) the window starts
  int     startMin;    // local minute (0/15/30/45) the window starts
  bool    today;       // window starts on the current local day
  bool    now;         // window starts at the current quarter (cheap right now)
  int16_t avgCenti;    // window average price (0.01 snt/kWh)
} ElecDisplay;

// "Next cheap" widget: earliest >=1 h run below the adaptive cheap threshold,
// excluding quiet hours [quietStartHour, quietEndHour). false if none / no data.
bool Electricity_getNextCheap(int quietStartHour, int quietEndHour, int factorPct,
                              int16_t floorCenti, int16_t ceilingCenti,
                              ElecDisplay *out);

// "Cheapest hour" widget: globally cheapest 1 h window (excluding quiet hours).
// false if no eligible 1 h window / no data.
bool Electricity_getCheapestHour(int quietStartHour, int quietEndHour,
                                 ElecDisplay *out);
