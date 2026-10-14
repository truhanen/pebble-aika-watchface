// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen
#include "battery_days.h"

static BatteryDaysState s_state;

void BatteryDays_init(void) {
  BatteryDays_reset(&s_state);
  if (persist_exists(BATTERY_DAYS_PERSIST_KEY) &&
      persist_exists(BATTERY_DAYS_VERSION_PERSIST_KEY) &&
      persist_read_int(BATTERY_DAYS_VERSION_PERSIST_KEY) == BATTERY_DAYS_VERSION &&
      persist_get_size(BATTERY_DAYS_PERSIST_KEY) == (int)sizeof(s_state)) {
    persist_read_data(BATTERY_DAYS_PERSIST_KEY, &s_state, sizeof(s_state));
  } else if (persist_exists(BATTERY_DAYS_LEGACY_RATE_KEY)) {
    // One-time migration from v1: seed the EWMA with the old learned rate (capped),
    // so the widget eases from the previous number toward the corrected rate rather
    // than spiking to the 30-day default. It converges within ~20% of discharge.
    uint32_t legacy = (uint32_t)persist_read_int(BATTERY_DAYS_LEGACY_RATE_KEY);
    s_state.learned = BatteryDays_capRate(legacy);
  }
  // Seed the current reading so a discharge segment that happened while the
  // watchface was not the foreground app still gets folded in on launch, and the
  // anchor is current. record() re-anchors on charging / a percent rise, folds on a
  // decrease and is a no-op on an unchanged percent, so this is safe every launch.
  BatteryChargeState st = battery_state_service_peek();
  BatteryDays_record(&s_state, (uint32_t)time(NULL), st.charge_percent, st.is_charging);
}

void BatteryDays_save(void) {
  persist_write_int(BATTERY_DAYS_VERSION_PERSIST_KEY, BATTERY_DAYS_VERSION);
  persist_write_data(BATTERY_DAYS_PERSIST_KEY, &s_state, sizeof(s_state));
}

void BatteryDays_onBattery(BatteryChargeState charge_state) {
  BatteryDays_record(&s_state, (uint32_t)time(NULL),
                     charge_state.charge_percent, charge_state.is_charging);
  BatteryDays_save();
}

int BatteryDays_currentEstimateTenths(void) {
  BatteryChargeState st = battery_state_service_peek();
  // learned is already capped on update; cap again defensively so a stray oversized
  // persisted rate is bounded even on the very first draw. Fall back to the 30-day
  // default until the first plausible discharge segment has been learned.
  uint32_t rate = (s_state.learned > 0) ? BatteryDays_capRate(s_state.learned)
                                        : BATTERY_DAYS_DEFAULT_SEC_PER_PCT;
  return BatteryDays_tenthsFromRate(st.charge_percent, rate);
}
