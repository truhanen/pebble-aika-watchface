#include <pebble.h>
#include "clock_face.h"
#include "messaging.h"
#include "settings.h"
#include "weather.h"
#include "widget_layer.h"
#include "util.h"
#include "electricity.h"
#include "crypto.h"
#include "currency.h"
#include "tuya.h"
#include "tuya_leds.h"
#include "widgets.h"
#include "widget_list.h"
#include "battery_days.h"
#include "poll_state_calc.h"
#include "night_window_calc.h"
#include "theme.h"
#include "test_hooks.h"

// windows and layers
static Window* mainWindow;
static Layer* windowLayer;

// current bluetooth state
static bool isPhoneConnected;

// current time service subscription
static bool updatingEverySecond;

static AppTimer *rotationTimer = NULL;   // repaints the widgets at sub-minute rotation boundaries

static time_t lastDataRequest = 0;   // epoch of the last watch->phone data request
static time_t lastColdRequest = 0;   // epoch of the last COLD (forced) request

// Last sampled night-window state (-1 = not sampled yet). See the boundary check in
// tick_handler: without it, a night window that suppressed the rotation timer would
// never be undone, because nothing else re-arms it.
static int nightLast = -1;

// Persisted so the poll clock is NOT restarted by a relaunch. A watchface is killed
// whenever any watchapp runs, so re-stamping "now" in init() meant a user who
// returned to the watchface more often than pollIntervalMin never polled at all
// (previously masked by the phone-side fetch on every PKJS 'ready').
#define POLL_STATE_PERSIST_KEY 319
#define COLD_MIN_INTERVAL_S 600      // at most one cold request per 10 min at level 0
#define COLD_MAX_INTERVAL_S 21600    // ...backing off to at most one per 6 h
#define COLD_BACKOFF_MAX_LEVEL 6

// How many cold rounds in a row have failed to resolve the missing source. Persisted
// with the poll clock so a relaunch cannot restart the backoff (a watchface is killed
// whenever any watchapp runs, which is exactly how the flat rate limit got defeated).
static int32_t coldBackoffLevel = 0;

typedef struct {
  int32_t lastDataRequest;
  int32_t lastColdRequest;
  int32_t coldBackoffLevel;   // appended; an older 8-byte blob leaves this at 0
} PollStateBlob;

static void poll_state_load(void) {
  // Zero-init matters: persist_read_data on an older, SHORTER blob fills only what was
  // stored and leaves the tail untouched, so the appended field must already be 0.
  PollStateBlob b = { 0, 0, 0 };
  if (persist_exists(POLL_STATE_PERSIST_KEY)) {
    persist_read_data(POLL_STATE_PERSIST_KEY, &b, sizeof(b));
  }
  int32_t now = (int32_t)time(NULL);
  lastDataRequest = (time_t)poll_stamp_sanitize(b.lastDataRequest, now);
  lastColdRequest = (time_t)poll_stamp_sanitize(b.lastColdRequest, now);
  coldBackoffLevel = b.coldBackoffLevel;
  if (coldBackoffLevel < 0) { coldBackoffLevel = 0; }
  if (coldBackoffLevel > COLD_BACKOFF_MAX_LEVEL) { coldBackoffLevel = COLD_BACKOFF_MAX_LEVEL; }
}

static void poll_state_save(void) {
  PollStateBlob b = { (int32_t)lastDataRequest, (int32_t)lastColdRequest, coldBackoffLevel };
  persist_write_data(POLL_STATE_PERSIST_KEY, &b, sizeof(b));
}

void update_clock();
void redrawScreen();
void tick_handler(struct tm *tick_time, TimeUnits units_changed);
void bluetoothStateChanged(bool newConnectionState);
static void schedule_rotation_timer(void);


void update_clock() {
  time_t rawTime;
  struct tm* timeInfo;

#ifdef USE_FAKE_TIME
  struct tm fakeTime = {
    .tm_hour = 6,
    .tm_min  = 23,
    .tm_sec  = 0,
    .tm_mday = 24,
    .tm_mon  = 3,
    .tm_year = 126,
    .tm_wday = 5,
  };
  timeInfo = &fakeTime;
#else
  time(&rawTime);
  timeInfo = localtime(&rawTime);
#endif
  TestHooks_applyTime(timeInfo);

  ClockFace_updateTime(timeInfo);
}

// Re-parse the widget lists and repaint the face, which draws the widgets.
static void apply_layout(void) {
  WidgetLayer_reload();
  layer_mark_dirty(clock_face_layer);
}

static void rotation_timer_cb(void *data) {
  (void)data;
  rotationTimer = NULL;
  ClockFace_redraw();            // re-resolves active members from the clock (stable heights)
  schedule_rotation_timer();     // arm the next boundary
}

// (Re)arm the rotation timer for the next sub-minute boundary, or cancel it when no
// sub-minute rotating group exists. >=1min rotations need no timer (the minute tick
// already repaints the face). Skipped while already second-ticking (the tick
// handler repaints every second).
// Is the night window in effect right now? Reads the local hour and the watch's
// Quiet Time state and hands both to the pure predicate.
static bool night_window_now(void) {
  if (settings.nightMode == NIGHT_WINDOW_OFF) { return false; }   // cheap path
  time_t now = time(NULL);
  struct tm *lt = localtime(&now);
  return night_window_active(settings.nightMode, lt->tm_hour,
                              settings.nightStartHour, settings.nightEndHour,
                              quiet_time_is_active());
}

// Is the NIGHT PALETTE in effect right now? The window alone is not enough -- the colours
// are their own opt-in, so a user can keep the rotation saving without a dark watchface.
static bool night_colors_now(void) {
  return settings.nightColors && night_window_now();
}

static void schedule_rotation_timer(void) {
  if (rotationTimer) { app_timer_cancel(rotationTimer); rotationTimer = NULL; }
  if (updatingEverySecond) { return; }
  int p = Settings_minSubMinuteIntervalSec();
  // During the night window, suppress the sub-minute timer entirely. Rotation does NOT
  // stop: the displayed member is derived from the wall clock on every draw, and the
  // minute tick still repaints the widgets -- so the group keeps cycling at 1/min while
  // costing zero extra wakeups. (Freezing it outright
  // would hide half a rotating pair all night.)
  // The night window now also drives the palette, so slowing rotation is opt-out
  // (default on = the behaviour the window used to imply on its own).
  p = night_window_rotation_interval(p, night_window_now() && settings.nightSlowRotation);
  if (p <= 0) { return; }
  time_t now = time(NULL);
  struct tm *lt = localtime(&now);
  int sod = lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec;
  int secToNext = p - (sod % p);
  uint32_t delayMs = (uint32_t)secToNext * 1000;
  if (delayMs == 0) { delayMs = (uint32_t)p * 1000; }
  rotationTimer = app_timer_register(delayMs, rotation_timer_cb, NULL);
}

/* forces everything on screen to be redrawn -- perfect for keeping track of settings! */
void redrawScreen() {

  // check if the tick handler frequency should be changed
  bool wantEverySecond = dynamicSettings.updateScreenEverySecond;
  if(wantEverySecond != updatingEverySecond) {
    tick_timer_service_unsubscribe();

    if(wantEverySecond) {
      tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
      updatingEverySecond = true;
    } else {
      tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
      updatingEverySecond = false;
    }
  }

  // Resolve the palette before anything paints with it.
  Theme_update(night_colors_now());

  window_set_background_color(mainWindow, theme.timeBgColor);

  // maybe the language changed!
  update_clock();

  apply_layout();   // re-parses the widget lists and repaints the face

  schedule_rotation_timer();   // re-evaluate after any settings change / tick-mode change
}

static void main_window_load(Window *window) {
  window_set_background_color(window, theme.timeBgColor);

  ClockFace_init(window);
  WidgetLayer_init();

  redrawScreen();
}

static void main_window_unload(Window *window) {
  ClockFace_deinit();
  WidgetLayer_deinit();
}


// Widgets whose data comes from the phone JS on the shared poll. Crypto covers
// every coin wid (legacy 15/16/17 and the configurable 200+ range).
static bool isPhoneDataWidget(WidgetType w) {
  return w == ELECTRICITY || w == NEXT_CHEAP_ELEC || w == CHEAPEST_ELEC_HOUR
      || w == TUYA_LEDS
      || Crypto_isWid((uint8_t)w) || Currency_isWid((uint8_t)w) || Tuya_isWid((uint8_t)w);
}

static void phone_data_scan_cb(uint8_t w, void *ctx) {
  bool *needs = (bool *)ctx;
  if (isPhoneDataWidget((WidgetType)w)) { *needs = true; }
}

// True when ANY phone-fetched source is in use: weather is gated by the setting,
// every other source by a placed widget id in either rendered column (not the
// legacy 3-slot widgets[] mirror). Shared by the tick poll and the BT-reconnect
// refresh so both agree on "is a phone round-trip worth anything at all".
static bool needs_phone_data(void) {
  bool needs = !dynamicSettings.disableWeather;
  Settings_forEachWidgetId(phone_data_scan_cb, &needs);
  return needs;
}

// The effective poll interval in seconds, floored at 5 min.
static int poll_interval_sec(void) {
  int s = (int)settings.pollIntervalMin * 60;
  return (s < 300) ? 300 : s;
}

// A placed widget whose data the watch does NOT have. Per-wid for the phone-data
// slots, so a newly configured coin/sensor also counts as missing.
static bool widget_source_missing(uint8_t w) {
  switch (w) {
    case ELECTRICITY:
    case NEXT_CHEAP_ELEC:
    case CHEAPEST_ELEC_HOUR:
      return !Electricity_hasData();
    case WEATHER_CURRENT:
    case WEATHER_FORECAST_TODAY:
    case WEATHER_UV_INDEX:
      return !Weather_hasData();
    case TUYA_LEDS:
      return TuyaLeds_count == 0;
    default:
      if (Crypto_isWid(w))   { return Crypto_find(w) == NULL; }
      if (Currency_isWid(w)) { return Currency_find(w) == NULL; }
      if (Tuya_isWid(w))     { return Tuya_find(w) == NULL; }
      return false;
  }
}

static void cold_scan_cb(uint8_t w, void *ctx) {
  bool *missing = (bool *)ctx;
  if (widget_source_missing(w)) { *missing = true; }
}

void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // Night boundary, sampled once a minute. Two things depend on it and NEITHER has an
  // event of its own: schedule_rotation_timer() is otherwise reachable only from
  // redrawScreen() and the rotation timer's own callback (so once the window suppresses
  // the timer, nothing would re-arm it in the morning), and Quiet Time -- one of the
  // window modes -- emits no start/end event at all. The window's boundaries are
  // hour-granular, so a per-minute check is exact enough and rides a tick the watchface
  // performs anyway; the flip test keeps it from repainting 1440 times a day.
  if (tick_time->tm_sec == 0) {
    int nightNow = night_window_now() ? 1 : 0;
    if (nightNow != nightLast) {
      nightLast = nightNow;
      schedule_rotation_timer();
    }
    if (night_colors_now() != Theme_nightActive()) {
      redrawScreen();   // recomputes the theme and repaints everything
    }
  }

  // One watch-driven request per configured interval serves ALL phone-fetched
  // data (weather, electricity, crypto). This is the documented Pebble pattern:
  // the tick service is always running and the AppMessage wakes the phone JS out
  // of power-save. The JS side throttles per source (electricity ~2/day; crypto
  // sends only on change), so a short interval does not over-fetch slow sources.
  //
  // tm_sec first: in seconds mode this runs every tick, and the widget-list scan
  // inside needs_phone_data() is only worth doing on the minute boundary.
  if (tick_time->tm_sec == 0 && needs_phone_data()) {
    int intervalSec = poll_interval_sec();
    time_t now = time(NULL);

    // The watch is the only party that knows its persist was wiped (reinstall /
    // firmware wipe): the phone's *_last_sent stamps survive and would suppress a
    // re-send as "unchanged". Ask for a FORCED round in that case, rate limited so a
    // permanently failing source (bad Tuya keys) can't fetch on every relaunch.
    //
    // The rate limit BACKS OFF, because a source can be permanently missing (wrong Tuya
    // credentials, a coin the phone cannot price, denied location). At a flat 10 min that
    // meant 144 forced rounds a day instead of 48 -- and a cold request bypasses every
    // phone-side throttle, including electricity's 11 h one and its ~400 B message, the
    // largest in the system. So: double the spacing after each cold round that fails to
    // resolve the gap (10 min -> ... -> 6 h), and reset the moment nothing is missing.
    bool missing = false;
    Settings_forEachWidgetId(cold_scan_cb, &missing);
    bool cold = false;
    if (!missing) {
      // everything resolved -> the next genuine gap gets the fast 10-minute response
      if (coldBackoffLevel != 0) { coldBackoffLevel = 0; poll_state_save(); }
    } else if (poll_cold_allowed((int32_t)lastColdRequest, (int32_t)now,
                                 poll_cold_interval((int)coldBackoffLevel,
                                                    COLD_MIN_INTERVAL_S,
                                                    COLD_MAX_INTERVAL_S))) {
      cold = true;
    }

    if (cold || poll_due((int32_t)lastDataRequest, (int32_t)now, (int32_t)intervalSec)) {
      // Kept deliberately: this is the one line that makes the relaunch-silence
      // behaviour observable in `pebble logs` on a real watch. Fires at most once
      // per poll interval (>= 5 min), so it is not log spam.
      APP_LOG(APP_LOG_LEVEL_INFO, "poll: requesting data (cold=%d backoff=%d)",
              cold ? 1 : 0, (int)coldBackoffLevel);
      messaging_requestNewWeatherData(cold);
      lastDataRequest = now;
      if (cold) {
        lastColdRequest = now;
        // Assume it will fail again; the !missing branch above resets this to 0 as soon
        // as the data actually arrives, so a successful cold round costs one extra level
        // that is immediately cleared on the next tick.
        if (coldBackoffLevel < COLD_BACKOFF_MAX_LEVEL) { coldBackoffLevel++; }
      }
      poll_state_save();
    }
  }

  // every hour, if requested, vibrate
  if(!quiet_time_is_active() && tick_time->tm_sec == 0) {
    if(settings.hourlyVibe == VIBE_EVERY_HOUR) { // hourly vibes only
      if(tick_time->tm_min == 0) {
        vibes_double_pulse();
      }
    } else if(settings.hourlyVibe == VIBE_EVERY_HALF_HOUR) {  // hourly and half-hourly
      if(tick_time->tm_min == 0) {
        vibes_double_pulse();
      } else if(tick_time->tm_min == 30) {
        vibes_short_pulse();
      }
    }
  }

  update_clock();

  // redraw all screen
  ClockFace_redraw();
}

void bluetoothStateChanged(bool newConnectionState) {
  // if the phone was connected but isn't anymore and the user has opted in,
  // trigger a vibration
  if(!quiet_time_is_active() && isPhoneConnected && !newConnectionState && settings.btVibe) {
    static uint32_t const segments[] = { 200, 100, 100, 100, 500 };
    VibePattern pat = {
      .durations = segments,
      .num_segments = ARRAY_LENGTH(segments),
      };
    vibes_enqueue_custom_pattern(pat);
    light_enable_interaction();   // briefly light the backlight, per the user's watch light settings
  }

  // A genuine disconnect->connect transition can have left the data stale, so refresh
  // it -- but gated exactly like the tick poll (a phone source is actually in use AND
  // the interval has elapsed), sharing the same persisted clock. Without the gate a
  // flapping BT link turned every reconnect into a phone round-trip, and a watchface
  // with no phone-data widget asked anyway.
  //
  // NOTE this handler is NEVER called synthetically at launch anymore (see init()):
  // isPhoneConnected is zero-initialised on every launch, so a launch-time call looked
  // like a reconnect and sent a request on EVERY relaunch -- the exact phone traffic
  // the persisted poll clock exists to avoid.
  if(!isPhoneConnected && newConnectionState && needs_phone_data()) {
    time_t now = time(NULL);
    if (poll_due((int32_t)lastDataRequest, (int32_t)now, (int32_t)poll_interval_sec())) {
      APP_LOG(APP_LOG_LEVEL_INFO, "poll: requesting data (bt reconnect)");
      messaging_requestNewWeatherData(false);
      lastDataRequest = now;
      poll_state_save();
    }
  }

  isPhoneConnected = newConnectionState;

  ClockFace_redraw();
}

// force the face (battery widgets) to redraw any time the battery state changes
void batteryStateChanged(BatteryChargeState charge_state) {
  BatteryDays_onBattery(charge_state);
  ClockFace_redraw();
}

// fixes for disappearing elements after notifications
// (from http://codecorner.galanter.net/2016/01/08/solved-issue-with-pebble-framebuffer-after-notification-is-dismissed/)
static void app_focus_changing(bool focusing) {
  if (focusing) {
     layer_set_hidden(windowLayer, true);
  }
}

static void app_focus_changed(bool focused) {
  if (focused) {
     layer_set_hidden(windowLayer, false);
     layer_mark_dirty(windowLayer);
  }
}

static void init() {
  setlocale(LC_ALL, "");

  srand(time(NULL));

  // init settings
  Settings_init();

  // Resolve the palette so the very first paint (before redrawScreen ever runs) is
  // already correct.
  Theme_update(night_colors_now());

  // Restore the poll clock (see poll_state_load): a relaunch must NOT restart it.
  poll_state_load();

  BatteryDays_init();

  // init weather system
  Weather_init();
  Electricity_init();
  Crypto_init();
  Currency_init();
  Tuya_init();
  TuyaLeds_init();

  // init the messaging thing
  messaging_init(redrawScreen);

  // Create main Window element and assign to pointer
  mainWindow = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(mainWindow, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(mainWindow, true);

  windowLayer = window_get_root_layer(mainWindow);

  // Register with TickTimerService
  if(dynamicSettings.updateScreenEverySecond) {
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
    updatingEverySecond = true;
  } else {
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    updatingEverySecond = false;
  }

  // SEED the connection state directly instead of calling bluetoothStateChanged():
  // isPhoneConnected is zero-initialised at every launch, so the synthetic first call
  // read as a disconnect->connect transition and sent a data request on EVERY relaunch
  // (a watchface is killed whenever any watchapp runs). The disconnect vibe could never fire at launch anyway
  // (it requires the previous state to be connected).
  isPhoneConnected = bluetooth_connection_service_peek();
  bluetooth_connection_service_subscribe(bluetoothStateChanged);

  // register with battery service
  battery_state_service_subscribe(batteryStateChanged);

  // set up focus change handlers
  app_focus_service_subscribe_handlers((AppFocusHandlers){
    .did_focus = app_focus_changed,
    .will_focus = app_focus_changing
  });
}

static void deinit() {
  // Destroy Window
  window_destroy(mainWindow);

  // unload weather stuff
  Weather_deinit();
  Electricity_deinit();
  Settings_deinit();
  BatteryDays_save();

  tick_timer_service_unsubscribe();
  if (rotationTimer) { app_timer_cancel(rotationTimer); rotationTimer = NULL; }
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
