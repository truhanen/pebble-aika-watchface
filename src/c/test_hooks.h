#pragma once
#include <pebble.h>

// Emulator test hooks. In a TEST_HOOKS build (`TEST_HOOKS=1 pebble build`) the
// Test* AppMessage keys override the watch's own battery / Bluetooth / health /
// time readings, so every widget state can be driven from scripts/emu.sh without
// a phone or real sensor data. A value of -1 clears an override.
//
// The SDK peek calls are redirected by the macros below, so drawing code keeps
// calling the plain SDK names. In a normal build everything here is a no-op.

// Applies any Test* keys in `iter` (TestCenterWidget sets the centre list
// to one widget id in the position's default font, -1 clears it; 0 is the Empty widget. For
// anything richer send the payload itself: scripts/emu.sh bytes SettingCenterWidgetList=...); returns true if one was present.
bool TestHooks_handleMessage(DictionaryIterator *iter);

// Overwrites hour/minute (TestTime), seconds (TestSeconds) and the calendar date (TestDate, YYYYMMDD) in `t`
// while those overrides are active.
void TestHooks_applyTime(struct tm *t);

// Current TestScene value (0 = normal watchface).
int TestHooks_scene(void);

// TestFont{Left,Right,Center}: the widget font index forced on every widget of a
// position (WIDGET_POS_*), or -1 for the configured fonts.
int TestHooks_font(int pos);

#if defined(TEST_HOOKS) && !defined(TEST_HOOKS_IMPL)
BatteryChargeState TestHooks_batteryPeek(void);
bool TestHooks_btPeek(void);
HealthValue TestHooks_healthSumToday(HealthMetric metric);
HealthValue TestHooks_healthPeek(HealthMetric metric);
HealthServiceAccessibilityMask TestHooks_healthAccessible(HealthMetric metric, time_t start, time_t end);

#define battery_state_service_peek() TestHooks_batteryPeek()
#define bluetooth_connection_service_peek() TestHooks_btPeek()
#define health_service_sum_today(m) TestHooks_healthSumToday(m)
#define health_service_peek_current_value(m) TestHooks_healthPeek(m)
#define health_service_metric_accessible(m, s, e) TestHooks_healthAccessible(m, s, e)
#endif
