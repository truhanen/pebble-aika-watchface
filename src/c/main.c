#include <pebble.h>
#include "clock_area.h"
#include "messaging.h"
#include "settings.h"
#include "weather.h"
#include "sidebar.h"
#include "util.h"
#include "twt_status.h"
#include "midi_status.h"
#include "date_header.h"
#include "electricity.h"
#include "crypto.h"
#include "currency.h"
#include "tuya.h"
#include "tuya_leds.h"
#include "sidebar_widgets.h"
#include "widget_list.h"
#include "battery_days.h"
#include "poll_state_calc.h"
#include "night_window_calc.h"
#include "theme.h"

// windows and layers
static Window* mainWindow;
static Layer* windowLayer;

// current bluetooth state
static bool isPhoneConnected;

// current time service subscription
static bool updatingEverySecond;

static AppTimer *rotationTimer = NULL;   // repaints the sidebar at sub-minute rotation boundaries

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
static bool twtTargetAlerted = false;   // already vibrated for the current daily-target crossing
static bool twtTargetInit = false;      // have we seeded twtTargetAlerted since launch?
static bool twtBudgetAlerted = false;   // already vibrated for the current task's budget crossing
static bool twtBudgetInit = false;      // have we seeded twtBudgetAlerted since launch?
static int32_t twtBudgetLastCtx = 0;    // context key (isTracking?taskId:-1) of the last seed

void update_clock();
void redrawScreen();
void tick_handler(struct tm *tick_time, TimeUnits units_changed);
void bluetoothStateChanged(bool newConnectionState);
static void apply_twt_layout();
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

  ClockArea_update_time(timeInfo);
  Sidebar_updateTime(timeInfo);
  DateHeader_updateTime(timeInfo);
}

// Apply the TrackWorkTime layout. While tracking, shrink the clock to make room for the
// status line and show it. When not tracking, restore the original full-size clock and hide
// the line, so the watchface looks exactly as it did before the integration.
static void apply_twt_layout() {
  if (!TwtStatus_isSupported()) return;  // rect, non-aplite only

  // The system timeline-peek banner (upcoming calendar event) eats screen
  // space. Rather than cram the bottom status strip into what's left, drop it
  // entirely while obstructed and give the clock + date header the full
  // unobstructed area. Detected by comparing the unobstructed bounds (what we
  // get to draw in) against the full screen bounds.
  Layer *root_layer = window_get_root_layer(mainWindow);
  GRect full = layer_get_bounds(root_layer);
  GRect root = layer_get_unobstructed_bounds(root_layer);
  bool obstructed = root.size.h < full.size.h;

  // Generic "active bottom status" resolution: {visible, height}. MIDI wins over
  // TWT when both active. A future status display adds another case here; the
  // panel/status geometry below is status-type agnostic.
  bool showMidi = !obstructed && midi_status.isRecording;
  bool showTwt  = !obstructed && !showMidi && twt_status.isTracking;
  bool statusVisible = showMidi || showTwt;
  // All status displays reserve the SAME fixed height (the 2-line TWT height), so
  // the clock stays the same size regardless of which status is showing. A
  // single-line status (MIDI) is vertically centered within this area by its own
  // setFrame.
  int statusHeight = statusVisible ? TWT_STATUS_HEIGHT_2LINE : 0;

  int topReserved = settings.showBigDate ? BIG_DATE_HEIGHT : 0;
  int statusTop = root.size.h - statusHeight;   // == root.size.h when no status

  // Column inner height available for widgets. With the strip full-width, columns
  // shorten to the strip top; otherwise (default) they stay full height and the
  // strip is inset between them.
  bool stripFullWidth = settings.statusStripFullWidth;
  int columnFrameHeight = (statusVisible && stripFullWidth) ? statusTop : root.size.h;

  // Two independent lists: left column = widgetList, right column = rightWidgetList.
  // A column is shown iff its list is non-empty (no priority cursor / overflow
  // between columns; each list is drawn in full and clipped if it overflows).
  int primaryCount, secondaryCount;
  Sidebar_distributeWidgets(&primaryCount, &secondaryCount);
  bool showPrimary = primaryCount > 0;
  bool showSecondary = secondaryCount > 0;

  // Left (primary) column, always on the left.
  if (showPrimary) {
    Sidebar_setPrimaryFrame(GRect(0, 0, sidebarWidth, columnFrameHeight));
    Sidebar_setPrimaryHidden(false);
  } else {
    Sidebar_setPrimaryHidden(true);
  }

  // Right (secondary) column, always on the right.
  if (showSecondary) {
    Sidebar_setSecondaryFrame(GRect(root.size.w - sidebarWidth, 0, sidebarWidth, columnFrameHeight));
    Sidebar_setSecondaryHidden(false);
  } else {
    Sidebar_setSecondaryHidden(true);
  }

  // Horizontal insets for the clock/date: inset each side that shows a column.
  int leftInset = showPrimary ? sidebarWidth : 0;
  int rightInset = showSecondary ? sidebarWidth : 0;
  int contentX = leftInset;
  int contentW = root.size.w - leftInset - rightInset;

  // Clock between the panels, between the top strip and the status strip.
  layer_set_frame(clock_area_layer,
      GRect(contentX, topReserved, contentW, statusTop - topReserved));

  // Date header in the top strip, between the panels.
  if (topReserved) {
    DateHeader_setFrame(GRect(contentX, 0, contentW, BIG_DATE_HEIGHT));
    DateHeader_setHidden(false);
    DateHeader_redraw();
  } else {
    DateHeader_setHidden(true);
  }

  // Status strip span: full width when stripFullWidth, otherwise inset between the
  // sidebars (same horizontal band as the clock).
  if (statusVisible) {
    int statusLeft  = stripFullWidth ? 0 : contentX;
    int statusWidth = stripFullWidth ? root.size.w : contentW;
    GRect statusFrame = GRect(statusLeft, statusTop, statusWidth, statusHeight);

    if (showMidi) {
      MidiStatus_setFrame(statusFrame);
      MidiStatus_setHidden(false); MidiStatus_redraw();
      TwtStatus_setHidden(true);
    } else {
      TwtStatus_setFrame(statusFrame);
      TwtStatus_setHidden(false); TwtStatus_redraw();
      MidiStatus_setHidden(true);
    }
  } else {
    TwtStatus_setHidden(true);
    MidiStatus_setHidden(true);
  }

  ClockArea_setStatusVisible(statusVisible);
  layer_mark_dirty(clock_area_layer);
}

// Recompute the layout the instant the timeline-peek banner slides in/out, so
// the bottom status strip never lingers hidden behind it (tick-driven relayout
// alone can lag up to a minute in MINUTE_UNIT mode).
static void unobstructed_did_change(void *context) {
  apply_twt_layout();
}

static void rotation_timer_cb(void *data) {
  (void)data;
  rotationTimer = NULL;
  Sidebar_redraw();              // re-resolves active members from the clock (stable heights)
  schedule_rotation_timer();     // arm the next boundary
}

// (Re)arm the rotation timer for the next sub-minute boundary, or cancel it when no
// sub-minute rotating group exists. >=1min rotations need no timer (the minute tick
// already repaints the sidebar). Skipped while already second-ticking (the tick
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
  int p = WidgetList_minSubMinuteIntervalSec(settings.widgetList, settings.widgetCount);
  int pr = WidgetList_minSubMinuteIntervalSec(settings.rightWidgetList, settings.rightWidgetCount);
  if (pr > 0 && (p == 0 || pr < p)) { p = pr; }
  // During the night window, suppress the sub-minute timer entirely. Rotation does NOT
  // stop: the displayed member is derived from the wall clock on every draw, and the
  // minute tick still repaints the sidebar -- so the group keeps cycling at 1/min while
  // costing zero extra wakeups. (Freezing it outright would need a change in sidebar.c
  // and would hide half a rotating pair all night.)
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
  bool wantEverySecond = dynamicSettings.updateScreenEverySecond
      || (midi_status.isRecording && settings.midiSecondPrecision);
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

  // update the sidebar
  Sidebar_redraw();

  ClockArea_redraw();

  apply_twt_layout();

  schedule_rotation_timer();   // re-evaluate after any settings change / tick-mode change
}

static void main_window_load(Window *window) {
  window_set_background_color(window, theme.timeBgColor);

  // create the sidebar
  Sidebar_init(window);

  ClockArea_init(window);

  if (TwtStatus_isSupported()) {
    GRect root = layer_get_bounds(window_get_root_layer(window));
    // Initial frames; apply_twt_layout() resets them per-mode before unhiding.
    GRect twtFrame = GRect(0, root.size.h - TWT_STATUS_HEIGHT_2LINE, root.size.w, TWT_STATUS_HEIGHT_2LINE);
    GRect midiFrame = GRect(0, root.size.h - TWT_STATUS_HEIGHT, root.size.w, TWT_STATUS_HEIGHT);
    TwtStatus_initLayer(window_get_root_layer(window), twtFrame);   // created hidden
    MidiStatus_initLayer(window_get_root_layer(window), midiFrame); // created hidden
    // Date header lives in the top strip; created hidden, apply_twt_layout() shows it per setting.
    GRect dateFrame = GRect(0, 0, root.size.w, BIG_DATE_HEIGHT);
    DateHeader_initLayer(window_get_root_layer(window), dateFrame);
  }

  redrawScreen(); // calls apply_twt_layout() -> sets clock size + line visibility per tracking
}

static void main_window_unload(Window *window) {
  ClockArea_deinit();
  TwtStatus_deinitLayer();
  MidiStatus_deinitLayer();
  DateHeader_deinitLayer();
  Sidebar_deinit();
}


// Widgets whose data comes from the phone JS on the shared poll. Crypto covers
// every coin wid (legacy 15/16/17 and the configurable 200+ range).
static bool isPhoneDataWidget(SidebarWidgetType w) {
  return w == ELECTRICITY || w == NEXT_CHEAP_ELEC || w == CHEAPEST_ELEC_HOUR
      || w == TUYA_LEDS
      || Crypto_isWid((uint8_t)w) || Currency_isWid((uint8_t)w) || Tuya_isWid((uint8_t)w);
}

static void phone_data_scan_cb(uint8_t w, void *ctx) {
  bool *needs = (bool *)ctx;
  if (isPhoneDataWidget((SidebarWidgetType)w)) { *needs = true; }
}

// True when ANY phone-fetched source is in use: weather is gated by the setting,
// every other source by a placed widget id in either rendered column (not the
// legacy 3-slot widgets[] mirror). Shared by the tick poll and the BT-reconnect
// refresh so both agree on "is a phone round-trip worth anything at all".
static bool needs_phone_data(void) {
  bool needs = !dynamicSettings.disableWeather;
  WidgetList_forEachId(settings.widgetList, settings.widgetCount, phone_data_scan_cb, &needs);
  WidgetList_forEachId(settings.rightWidgetList, settings.rightWidgetCount, phone_data_scan_cb, &needs);
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
    WidgetList_forEachId(settings.widgetList, settings.widgetCount, cold_scan_cb, &missing);
    WidgetList_forEachId(settings.rightWidgetList, settings.rightWidgetCount, cold_scan_cb, &missing);
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

  // Daily work-time target reached: vibrate once, on the rising edge, the minute
  // the worked total first hits the configured target. running is whole-minutes,
  // so the crossing is minute-granular and the MINUTE_UNIT tick lands on it.
  bool twtReached = twt_status.isTracking && twt_status.dailyTargetMin > 0
      && TwtStatus_workedTotalMin() >= twt_status.dailyTargetMin;
  if (!twtTargetInit) {
    twtTargetAlerted = twtReached;   // seed at launch: already-over-target -> no spurious vibe
    twtTargetInit = true;
  } else if (!twtReached) {
    twtTargetAlerted = false;        // reset for next day / target raised / tracking stopped
  } else if (!twtTargetAlerted) {
    twtTargetAlerted = true;
    if (settings.twtTargetVibe && !quiet_time_is_active()) {
      // distinct ascending pattern: short - short - long ("day complete")
      static const uint32_t segs[] = { 100, 80, 100, 80, 300 };
      VibePattern pat = { .durations = segs, .num_segments = ARRAY_LENGTH(segs) };
      vibes_enqueue_custom_pattern(pat);
      TwtStatus_startFlash();
      light_enable_interaction();   // briefly light the backlight, per the user's watch light settings
    }
  }

  // Task budget reached: vibrate once on the rising edge while tracking THAT task.
  // Context key isTracking?taskId:-1 re-seeds the alerted flag on task-switch and
  // stop/restart, so switching to an already-over task does NOT re-vibrate (only a
  // genuine crossing while tracking does). Budget measure = task all-time total
  // (matches the strip + Android list).
  int32_t budgetCtx = twt_status.isTracking ? twt_status.taskId : -1;
  bool budgetReached = twt_status.isTracking && twt_status.taskBudgetMin > 0
      && TwtStatus_taskTotalMin() >= twt_status.taskBudgetMin;
  if (!twtBudgetInit || budgetCtx != twtBudgetLastCtx) {
    twtBudgetAlerted = budgetReached;   // seed: already-over (or no budget) -> no spurious vibe
    twtBudgetLastCtx = budgetCtx;
    twtBudgetInit = true;
  } else if (!budgetReached) {
    twtBudgetAlerted = false;
  } else if (!twtBudgetAlerted) {
    twtBudgetAlerted = true;
    if (settings.twtBudgetVibe && !quiet_time_is_active()) {
      static const uint32_t segs[] = { 100, 80, 100, 80, 300 };  // same as daily target
      VibePattern pat = { .durations = segs, .num_segments = ARRAY_LENGTH(segs) };
      vibes_enqueue_custom_pattern(pat);
      TwtStatus_startFlash();
      light_enable_interaction();   // briefly light the backlight, per the user's watch light settings
    }
  }

  update_clock();

  // redraw all screen
  Sidebar_redraw();
  ClockArea_redraw();
  TwtStatus_redraw();
  MidiStatus_redraw();
  DateHeader_redraw();
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

  Sidebar_redraw();
  if (TwtStatus_isSupported()) { apply_twt_layout(); }
}

// force the sidebar to redraw any time the battery state changes
void batteryStateChanged(BatteryChargeState charge_state) {
  BatteryDays_onBattery(charge_state);
  Sidebar_redraw();
  if (TwtStatus_isSupported()) { apply_twt_layout(); }
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

  TwtStatus_load();
  MidiStatus_load();
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
  // (a watchface is killed whenever any watchapp runs). The redraw/layout that call
  // also did is replicated here; the disconnect vibe could never fire at launch anyway
  // (it requires the previous state to be connected).
  isPhoneConnected = bluetooth_connection_service_peek();
  Sidebar_redraw();
  if (TwtStatus_isSupported()) { apply_twt_layout(); }
  bluetooth_connection_service_subscribe(bluetoothStateChanged);

  // register with battery service
  battery_state_service_subscribe(batteryStateChanged);

  // set up focus change handlers
  app_focus_service_subscribe_handlers((AppFocusHandlers){
    .did_focus = app_focus_changed,
    .will_focus = app_focus_changing
  });

  // relayout the bottom status strip when the timeline-peek banner appears
  if (TwtStatus_isSupported()) {
    unobstructed_area_service_subscribe((UnobstructedAreaHandlers){
      .did_change = unobstructed_did_change
    }, NULL);
  }
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
  if (TwtStatus_isSupported()) {
    unobstructed_area_service_unsubscribe();
  }
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
