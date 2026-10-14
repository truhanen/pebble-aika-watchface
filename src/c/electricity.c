// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

#include "electricity.h"
#include <string.h>
#include <time.h>

ElectricityInfo Electricity_info;

// Bumped whenever the price table is replaced. Every cache below is keyed on it, so a
// new table invalidates all of them without any explicit teardown.
static uint32_t s_dataVersion = 1;

bool Electricity_hasData(void) { return Electricity_info.count > 0; }

void Electricity_init() {
  Electricity_info.startEpoch = 0;
  Electricity_info.count = 0;
  memset(Electricity_info.prices, 0, sizeof(Electricity_info.prices));

  if (persist_exists(ELEC_PERSIST_KEY_META)) {
    ElectricityMeta meta;
    persist_read_data(ELEC_PERSIST_KEY_META, &meta, sizeof(meta));
    Electricity_info.startEpoch = meta.startEpoch;
    uint16_t count = meta.count;
    if (count > ELEC_MAX_QUARTERS) { count = ELEC_MAX_QUARTERS; }
    Electricity_info.count = count;

    if (persist_exists(ELEC_PERSIST_KEY_DATA0)) {
      persist_read_data(ELEC_PERSIST_KEY_DATA0, Electricity_info.prices,
                        ELEC_CHUNK0_COUNT * sizeof(int16_t));
    }
    if (persist_exists(ELEC_PERSIST_KEY_DATA1)) {
      persist_read_data(ELEC_PERSIST_KEY_DATA1,
                        &Electricity_info.prices[ELEC_CHUNK0_COUNT],
                        (ELEC_MAX_QUARTERS - ELEC_CHUNK0_COUNT) * sizeof(int16_t));
    }
  }
#ifdef SCREENSHOT_FIXTURES
  // Demo electricity for appstore screenshots: 4.5 snt today-average, 6.3 snt now.
  {
    time_t now = time(NULL);
    Electricity_info.startEpoch = (uint32_t)(now - (now % 900)) - 8 * 3600;
    Electricity_info.count = 96;                                  // 24 h of 15-min quarters
    for (int i = 0; i < 96; i++) { Electricity_info.prices[i] = 450; }
    int cur;
    if (elec_current_index(Electricity_info.startEpoch, Electricity_info.count,
                           (int64_t)now, &cur)) {
      Electricity_info.prices[cur] = 630;                         // 6.3 snt right now
    }
  }
#endif
}

void Electricity_saveData() {
  s_dataVersion++;   // the table just changed -> invalidate every memoized result
  ElectricityMeta meta = { .startEpoch = Electricity_info.startEpoch,
                           .count = Electricity_info.count };
  persist_write_data(ELEC_PERSIST_KEY_META, &meta, sizeof(meta));
  persist_write_data(ELEC_PERSIST_KEY_DATA0, Electricity_info.prices,
                     ELEC_CHUNK0_COUNT * sizeof(int16_t));
  persist_write_data(ELEC_PERSIST_KEY_DATA1,
                     &Electricity_info.prices[ELEC_CHUNK0_COUNT],
                     (ELEC_MAX_QUARTERS - ELEC_CHUNK0_COUNT) * sizeof(int16_t));
}

void Electricity_deinit() {
  Electricity_saveData();
}

bool Electricity_getCurrentPrice(int16_t *out) {
  int idx;
  if (!elec_current_index(Electricity_info.startEpoch, Electricity_info.count,
                          (int64_t)time(NULL), &idx)) {
    return false;
  }
  *out = Electricity_info.prices[idx];
  return true;
}

// Index of the quarter containing `now`, or -1 when now is outside the table. Pure
// arithmetic (no localtime), so it is cheap enough to call on every frame -- and it is
// the cache key every memoized result below is keyed on, because none of them can change
// within a quarter unless the table itself is replaced.
static int elec_current_quarter(void) {
  int idx;
  if (!elec_current_index(Electricity_info.startEpoch, Electricity_info.count,
                          (int64_t)time(NULL), &idx)) {
    return -1;
  }
  return idx;
}

bool Electricity_getTodayAverage(int16_t *out) {
  // Memoized: 2 localtime + 2 mktime + a scan of up to 192 quarters, for a value that can
  // only change at a quarter boundary (midnight is one) or when the table is replaced.
  static uint32_t cachedVersion = 0;
  static int cachedIdx = -2;
  static int16_t cachedAvg;
  static bool cachedOk;
  int idx = elec_current_quarter();
  if (cachedVersion == s_dataVersion && cachedIdx == idx) {
    if (cachedOk) { *out = cachedAvg; }
    return cachedOk;
  }

  time_t now = time(NULL);

  struct tm start_tm = *localtime(&now);
  start_tm.tm_hour = 0;
  start_tm.tm_min = 0;
  start_tm.tm_sec = 0;
  time_t dayStart = mktime(&start_tm);

  struct tm end_tm = *localtime(&now);
  end_tm.tm_hour = 0;
  end_tm.tm_min = 0;
  end_tm.tm_sec = 0;
  end_tm.tm_mday += 1;            // mktime normalises month/day rollover
  time_t dayEnd = mktime(&end_tm);

  cachedOk = elec_today_average(Electricity_info.prices, Electricity_info.count,
                                Electricity_info.startEpoch,
                                (int64_t)dayStart, (int64_t)dayEnd, &cachedAvg);
  cachedVersion = s_dataVersion;
  cachedIdx = idx;
  if (cachedOk) { *out = cachedAvg; }
  return cachedOk;
}

#define ELEC_WIN_QUARTERS 4   // 1 hour = four 15-min quarters

// Eligibility per quarter (true = NOT in quiet hours). Shared by both query functions so
// there is exactly one buffer to keep in sync with the cache key below.
static bool s_eligible[ELEC_MAX_QUARTERS];

// Fills s_eligible[] for all valid quarters.
//
// The eligibility of a quarter depends ONLY on the price table and the quiet-hour window
// -- not on the current time -- so it is recomputed only when one of those changes. This
// matters: the loop does one localtime() per quarter (up to 192), it used to run on EVERY
// frame, and a sub-minute rotating group repaints the whole window several times a minute.
static void elec_build_eligible(int quietStartHour, int quietEndHour) {
  static uint32_t cachedVersion = 0;
  static int cachedQuietStart = -1;
  static int cachedQuietEnd = -1;
  if (cachedVersion != s_dataVersion
      || cachedQuietStart != quietStartHour || cachedQuietEnd != quietEndHour) {
    for (int i = 0; i < (int)Electricity_info.count; i++) {
      time_t qstart = (time_t)(Electricity_info.startEpoch + (uint32_t)i * 900);
      struct tm lt = *localtime(&qstart);
      s_eligible[i] = !elec_hour_in_quiet(lt.tm_hour, quietStartHour, quietEndHour);
    }
    cachedVersion = s_dataVersion;
    cachedQuietStart = quietStartHour;
    cachedQuietEnd = quietEndHour;
  }
}

static void elec_fill_display(const ElecWindow *w, int currentIdx, ElecDisplay *out) {
  time_t qstart = (time_t)(Electricity_info.startEpoch + (uint32_t)w->startIdx * 900);
  struct tm wt = *localtime(&qstart);
  out->startHour = wt.tm_hour;
  out->startMin = wt.tm_min;
  out->avgCenti = w->avgCenti;
  out->now = (w->startIdx == currentIdx);
  time_t now = time(NULL);
  struct tm nt = *localtime(&now);
  out->today = (wt.tm_yday == nt.tm_yday && wt.tm_year == nt.tm_year);
}

bool Electricity_getNextCheap(int quietStartHour, int quietEndHour, int factorPct,
                              int16_t floorCenti, int16_t ceilingCenti,
                              ElecDisplay *out) {
  // Memoized on everything the answer depends on. Within a quarter, and with an unchanged
  // table and unchanged settings, the result is by definition identical -- so the whole
  // scan runs at most 4x/hour instead of once per frame.
  static uint32_t cachedVersion = 0;
  static int cachedIdx = -2, cachedQs = -1, cachedQe = -1, cachedFactor = -1;
  static int16_t cachedFloor = 0, cachedCeiling = 0;
  static ElecDisplay cachedOut;
  static bool cachedOk;

  int currentIdx = elec_current_quarter();
  if (currentIdx < 0) { return false; }
  if (cachedVersion == s_dataVersion && cachedIdx == currentIdx
      && cachedQs == quietStartHour && cachedQe == quietEndHour
      && cachedFactor == factorPct
      && cachedFloor == floorCenti && cachedCeiling == ceilingCenti) {
    if (cachedOk) { *out = cachedOut; }
    return cachedOk;
  }
  cachedVersion = s_dataVersion;
  cachedIdx = currentIdx;
  cachedQs = quietStartHour;
  cachedQe = quietEndHour;
  cachedFactor = factorPct;
  cachedFloor = floorCenti;
  cachedCeiling = ceilingCenti;
  cachedOk = false;

  elec_build_eligible(quietStartHour, quietEndHour);
  int16_t mean;
  if (!elec_eligible_mean(Electricity_info.prices, s_eligible,
                          Electricity_info.count, currentIdx, &mean)) {
    return false;
  }
  int16_t bar = elec_cheap_bar(mean, factorPct, floorCenti, ceilingCenti);
  // Earliest hour below the cheap threshold; if none qualifies, fall back to
  // the cheapest upcoming hour so the widget shows that rather than "--".
  ElecWindow w = elec_find_next_cheap_or_cheapest(Electricity_info.prices, s_eligible,
                                                  Electricity_info.count, currentIdx,
                                                  bar, ELEC_WIN_QUARTERS);
  if (!w.found) { return false; }
  elec_fill_display(&w, currentIdx, &cachedOut);
  cachedOk = true;
  *out = cachedOut;
  return true;
}

bool Electricity_getCheapestHour(int quietStartHour, int quietEndHour,
                                 ElecDisplay *out) {
  // Memoized exactly like Electricity_getNextCheap (own cache: different search, and the
  // two widgets can be placed together).
  static uint32_t cachedVersion = 0;
  static int cachedIdx = -2, cachedQs = -1, cachedQe = -1;
  static ElecDisplay cachedOut;
  static bool cachedOk;

  int currentIdx = elec_current_quarter();
  if (currentIdx < 0) { return false; }
  if (cachedVersion == s_dataVersion && cachedIdx == currentIdx
      && cachedQs == quietStartHour && cachedQe == quietEndHour) {
    if (cachedOk) { *out = cachedOut; }
    return cachedOk;
  }
  cachedVersion = s_dataVersion;
  cachedIdx = currentIdx;
  cachedQs = quietStartHour;
  cachedQe = quietEndHour;
  cachedOk = false;

  elec_build_eligible(quietStartHour, quietEndHour);
  ElecWindow w = elec_find_cheapest(Electricity_info.prices, s_eligible,
                                    Electricity_info.count, currentIdx,
                                    ELEC_WIN_QUARTERS);
  if (!w.found) { return false; }
  elec_fill_display(&w, currentIdx, &cachedOut);
  cachedOk = true;
  *out = cachedOut;
  return true;
}
