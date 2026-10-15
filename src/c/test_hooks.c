#define TEST_HOOKS_IMPL
#include "test_hooks.h"

#ifdef TEST_HOOKS

#include "settings.h"
#include "widget_list.h"

static int32_t s_time = -1;         // HHMM
static int32_t s_battery = -1;      // percent, +1000 = charging
static int32_t s_bt = -1;           // 0/1
static int32_t s_steps = -1;
static int32_t s_sleep_min = -1;
static int32_t s_deep_sleep_min = -1;
static int32_t s_heart_rate = -1;
static int32_t s_scene = 0;
static int32_t s_date = -1;         // YYYYMMDD
static int32_t s_font[WIDGET_POS_COUNT] = { -1, -1, -1 };   // by WIDGET_POS_*

static bool read_key(DictionaryIterator *iter, uint32_t key, int32_t *out) {
  Tuple *t = dict_find(iter, key);
  if (!t) { return false; }
  *out = t->value->int32;
  APP_LOG(APP_LOG_LEVEL_INFO, "test hook %lu = %ld", (unsigned long)key, (long)*out);
  return true;
}

bool TestHooks_handleMessage(DictionaryIterator *iter) {
  bool any = false;
  any |= read_key(iter, MESSAGE_KEY_TestTime, &s_time);
  any |= read_key(iter, MESSAGE_KEY_TestBattery, &s_battery);
  any |= read_key(iter, MESSAGE_KEY_TestBt, &s_bt);
  any |= read_key(iter, MESSAGE_KEY_TestSteps, &s_steps);
  any |= read_key(iter, MESSAGE_KEY_TestSleepMin, &s_sleep_min);
  any |= read_key(iter, MESSAGE_KEY_TestDeepSleepMin, &s_deep_sleep_min);
  any |= read_key(iter, MESSAGE_KEY_TestHeartRate, &s_heart_rate);
  any |= read_key(iter, MESSAGE_KEY_TestScene, &s_scene);
  any |= read_key(iter, MESSAGE_KEY_TestDate, &s_date);
  any |= read_key(iter, MESSAGE_KEY_TestFontLeft, &s_font[WIDGET_POS_LEFT]);
  any |= read_key(iter, MESSAGE_KEY_TestFontRight, &s_font[WIDGET_POS_RIGHT]);
  any |= read_key(iter, MESSAGE_KEY_TestFontCenter, &s_font[WIDGET_POS_CENTER]);
  int32_t id;
  if (read_key(iter, MESSAGE_KEY_TestCenterWidget, &id)) {
    const uint8_t slot[2] = { (uint8_t)id, WL_FONT_POS_DEFAULT };
    Settings_setWidgetList(WIDGET_POS_CENTER, slot, id >= 0 ? 2 : 0);
    any = true;
  }
  return any;
}

void TestHooks_applyTime(struct tm *t) {
  if (s_date >= 0) {
    // Fixed calendar date (screenshots must not change with the day they are taken).
    static const int cum[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int y = (int)(s_date / 10000), m = (int)(s_date / 100 % 100), d = (int)(s_date % 100);
    if (m < 1 || m > 12) { m = 1; }
    bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    t->tm_year = y - 1900;
    t->tm_mon = m - 1;
    t->tm_mday = d;
    t->tm_yday = cum[m - 1] + d - 1 + ((leap && m > 2) ? 1 : 0);
    // Sakamoto's weekday formula (0 = Sunday).
    static const int tt[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    int yy = (m < 3) ? y - 1 : y;
    t->tm_wday = (yy + yy / 4 - yy / 100 + yy / 400 + tt[m - 1] + d) % 7;
  }
  if (s_time < 0) { return; }
  t->tm_hour = (s_time / 100) % 24;
  t->tm_min = (s_time % 100) % 60;
  t->tm_sec = 0;
}

int TestHooks_scene(void) { return (int)s_scene; }
int TestHooks_font(int pos) { return (int)s_font[pos]; }

BatteryChargeState TestHooks_batteryPeek(void) {
  BatteryChargeState st = battery_state_service_peek();
  if (s_battery >= 0) {
    st.is_charging = s_battery >= 1000;
    st.is_plugged = st.is_charging;
    st.charge_percent = (uint8_t)(s_battery % 1000);
  }
  return st;
}

bool TestHooks_btPeek(void) {
  return (s_bt >= 0) ? (s_bt != 0) : bluetooth_connection_service_peek();
}

static int32_t health_override(HealthMetric metric) {
  switch (metric) {
    case HealthMetricStepCount:           return s_steps;
    case HealthMetricWalkedDistanceMeters:return (s_steps >= 0) ? s_steps * 7 / 10 : -1;
    case HealthMetricSleepSeconds:        return (s_sleep_min >= 0) ? s_sleep_min * 60 : -1;
    case HealthMetricSleepRestfulSeconds: return (s_deep_sleep_min >= 0) ? s_deep_sleep_min * 60 : -1;
    case HealthMetricHeartRateBPM:        return s_heart_rate;
    default:                              return -1;
  }
}

HealthValue TestHooks_healthSumToday(HealthMetric metric) {
  int32_t v = health_override(metric);
  return (v >= 0) ? v : health_service_sum_today(metric);
}

HealthValue TestHooks_healthPeek(HealthMetric metric) {
  int32_t v = health_override(metric);
  return (v >= 0) ? v : health_service_peek_current_value(metric);
}

HealthServiceAccessibilityMask TestHooks_healthAccessible(HealthMetric metric, time_t start, time_t end) {
  if (health_override(metric) >= 0) { return HealthServiceAccessibilityMaskAvailable; }
  return health_service_metric_accessible(metric, start, end);
}

#else

bool TestHooks_handleMessage(DictionaryIterator *iter) { (void)iter; return false; }
void TestHooks_applyTime(struct tm *t) { (void)t; }
int TestHooks_scene(void) { return 0; }
int TestHooks_font(int pos) { (void)pos; return -1; }

#endif
