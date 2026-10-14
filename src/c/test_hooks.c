#define TEST_HOOKS_IMPL
#include "test_hooks.h"

#ifdef TEST_HOOKS

#include "settings.h"

static int32_t s_time = -1;         // HHMM
static int32_t s_battery = -1;      // percent, +1000 = charging
static int32_t s_bt = -1;           // 0/1
static int32_t s_steps = -1;
static int32_t s_sleep_min = -1;
static int32_t s_deep_sleep_min = -1;
static int32_t s_heart_rate = -1;
static int32_t s_scene = 0;
static int32_t s_font[4] = { -1, -1, -1, -1 };   // by WIDGET_POS_*

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
  any |= read_key(iter, MESSAGE_KEY_TestFontLeft, &s_font[WIDGET_POS_LEFT]);
  any |= read_key(iter, MESSAGE_KEY_TestFontRight, &s_font[WIDGET_POS_RIGHT]);
  any |= read_key(iter, MESSAGE_KEY_TestFontTop, &s_font[WIDGET_POS_TOP]);
  any |= read_key(iter, MESSAGE_KEY_TestFontBottom, &s_font[WIDGET_POS_BOTTOM]);
  int32_t id;
  if (read_key(iter, MESSAGE_KEY_TestTopWidget, &id)) {
    settings.topWidgetCount = (id >= 0) ? 1 : 0;
    settings.topWidgetList[0] = (uint8_t)id;
    any = true;
  }
  if (read_key(iter, MESSAGE_KEY_TestBottomWidget, &id)) {
    settings.bottomWidgetCount = (id >= 0) ? 1 : 0;
    settings.bottomWidgetList[0] = (uint8_t)id;
    any = true;
  }
  Settings_sanitizeWidgetLists();
  return any;
}

void TestHooks_applyTime(struct tm *t) {
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
