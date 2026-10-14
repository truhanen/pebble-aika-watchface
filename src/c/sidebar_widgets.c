#include "sidebar_widgets.h"
#include "languages.h"
#include "settings.h"
#include "theme.h"
#include "sidebar.h"
#include "util.h"
#include "weather.h"
#include <math.h>
#include <pebble.h>
#include <string.h>
#include "electricity.h"
#include "crypto.h"
#include "currency.h"
#include "tuya.h"
#include "tuya_leds.h"
#include "sidebar.h"   // sidebarWidth, for fitting the LED row
#include "battery_days.h"
#include "sleep_calc.h"

int SidebarWidgets_xOffset;
uint8_t SidebarWidgets_currentWidgetType = 0;
bool SidebarWidgets_hideIdentifier = false;

// sidebar icons
GDrawCommandImage *dateImage;
GDrawCommandImage *disconnectImage;
GDrawCommandImage *batteryImage;
GDrawCommandImage *batteryChargeImage;
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
GDrawCommandImage *dateLgImage;
#endif

// fonts
GFont smSidebarFont;
GFont mdSidebarFont;
GFont lgSidebarFont;
GFont currentSidebarFont;
GFont batteryFont;

// lightning bolt icon for the electricity widget (vector, no resource needed)
static const GPathInfo ELEC_BOLT_PATH_INFO = {
  .num_points = 7,
  .points = (GPoint[]) {{6, 0}, {0, 11}, {4, 11}, {2, 20}, {12, 7}, {7, 7}, {9, 0}}
};
static GPath *electricityBoltPath = NULL;

typedef struct {
  // battery meter
  int batteryGraphicOnlyHeight;
  int batteryWithPctHeight;
  int batteryTextY;
  // date
  int dateHeight, dateHeightCompact;
  int dateTopCorrection, dateDayNumY, dateMonthY;
  int dateBgX, dateBgY;
  int dateBgRectX, dateBgRectY, dateBgRectWidth, dateBgRectHeight;
  // current weather
  int weatherHeight;
  int weatherTempY;
  int weatherStationY;   // baseline of the small station-name line (FMI)
  // common text rect x offset and width for main data display
  int textRectX;
  int textRectWidth;
  // weather forecast
  int weatherForecastHeight;
  int forecastHighY, forecastDividerY, forecastLowY;
  int forecastDividerX, forecastDividerWidth;
  // basic widgets (week number, alt time, UV index, beats)
  int basicWidgetHeight;
  int basicWidgetLabelY;
  int basicWidgetY;
  // bluetooth disconnect
  int btDisconnectHeight;
  // heart rate
  int heartRateHeight;
  int heartRateValueY;
  int heartRateAgeY;
  // steps
  int stepCounterHeight;
  int stepsTextY;
  // sleep (single decimal line; Deep Sleep reuses these)
  int sleepTimerHeight;
  int sleepTextY;
  // Sleep + Deep Sleep: share bar in the identifier row, then the total, then the
  // restful figure in smSidebarFont. No icon, so it has its own first-line y.
  int sleepComboHeight, sleepComboBarY, sleepComboTotalY, sleepComboDeepY;
  int sleepBarWidgetHeight, sleepBarY, sleepBarThickness;
  // seconds
  int secondsHeight, secondsY;
  // tuya LED row
  int tuyaLedDiameter, tuyaLedGap;
} SidebarWidgetLayout;

static SidebarWidgetLayout layout;

// the date, time and weather strings
char currentDayName[8];
char currentDayNum[5];
char currentMonth[8];
char currentWeekNum[5];
char currentSecondsNum[5];
char altClock[8];
char currentBeats[5];

// the widgets
SidebarWidget batteryMeterWidget;
int BatteryMeter_getHeight();
void BatteryMeter_draw(GContext *ctx, int yPosition);

SidebarWidget batteryDaysWidget;
int BatteryDays_getHeight();
void BatteryDays_draw(GContext *ctx, int yPosition);

SidebarWidget emptyWidget;
int EmptyWidget_getHeight();
void EmptyWidget_draw(GContext *ctx, int yPosition);

SidebarWidget dateWidget;
int DateWidget_getHeight();
void DateWidget_draw(GContext *ctx, int yPosition);

SidebarWidget currentWeatherWidget;
int CurrentWeather_getHeight();
void CurrentWeather_draw(GContext *ctx, int yPosition);

SidebarWidget weatherForecastWidget;
int WeatherForecast_getHeight();
void WeatherForecast_draw(GContext *ctx, int yPosition);

SidebarWidget btDisconnectWidget;
int BTDisconnect_getHeight();
void BTDisconnect_draw(GContext *ctx, int yPosition);

SidebarWidget weekNumberWidget;
int WeekNumber_getHeight();
void WeekNumber_draw(GContext *ctx, int yPosition);

SidebarWidget secondsWidget;
int Seconds_getHeight();
void Seconds_draw(GContext *ctx, int yPosition);

SidebarWidget altTimeWidget;
int AltTime_getHeight();
void AltTime_draw(GContext *ctx, int yPosition);

SidebarWidget beatsWidget;
int Beats_getHeight();
void Beats_draw(GContext *ctx, int yPosition);

SidebarWidget electricityWidget;
int Electricity_getHeight();
void Electricity_draw(GContext *ctx, int yPosition);

SidebarWidget nextCheapWidget;
int NextCheap_getHeight();
void NextCheap_draw(GContext *ctx, int yPosition);

SidebarWidget cheapestHourWidget;
int CheapestHour_getHeight();
void CheapestHour_draw(GContext *ctx, int yPosition);

SidebarWidget cryptoWidget;
int  CryptoSlot_getHeight();
void CryptoSlot_draw(GContext *ctx, int yPosition);

SidebarWidget tuyaLedsWidget;
int TuyaLeds_getHeight();
void TuyaLeds_drawWidget(GContext *ctx, int yPosition);

SidebarWidget uvIndexWidget;
int UVIndex_getHeight();
void UVIndex_draw(GContext *ctx, int yPosition);

#ifdef PBL_HEALTH
GDrawCommandImage *sleepImage;
GDrawCommandImage *stepsImage;
GDrawCommandImage *heartImage;
GDrawCommandImage *deepSleepImage;

SidebarWidget stepCounterWidget;
int StepCounter_getHeight();
void StepCounter_draw(GContext *ctx, int yPosition);

SidebarWidget distanceWidget;
int Distance_getHeight();
void Distance_draw(GContext *ctx, int yPosition);

SidebarWidget sleepTimerWidget;
int SleepTimer_getHeight();
void SleepTimer_draw(GContext *ctx, int yPosition);

SidebarWidget deepSleepWidget;
int DeepSleep_getHeight();
void DeepSleep_draw(GContext *ctx, int yPosition);

SidebarWidget sleepCombinedWidget;
int SleepCombined_getHeight();
void SleepCombined_draw(GContext *ctx, int yPosition);

SidebarWidget sleepDeepBarWidget;
int SleepDeepBar_getHeight();
void SleepDeepBar_draw(GContext *ctx, int yPosition);

SidebarWidget heartRateWidget;
int HeartRate_getHeight();
void HeartRate_draw(GContext *ctx, int yPosition);

// UTC time of the most recent heart-rate reading; 0 = unknown.
static time_t s_last_hr_update_time = 0;
// BPM of that most recent reading; 0 = unknown. Used as a fallback when the
// live peek has no value yet (e.g. right after a cold start / install).
static int s_last_hr_bpm = 0;

// Seed s_last_hr_update_time/s_last_hr_bpm from the last hour of minute history
// so the age (and a fallback BPM) is correct immediately at launch, not only
// after the first in-session update.
static void hr_seed_from_history() {
  time_t now = time(NULL);
  time_t start = now - 3600;
  time_t end = now;
  HealthMinuteData minute_data[60];
  uint32_t num = health_service_get_minute_history(minute_data, 60, &start, &end);
  // Records are oldest-first; `start` is updated to the first record's time.
  for (int i = (int)num - 1; i >= 0; i--) {
    if (!minute_data[i].is_invalid && minute_data[i].heart_rate_bpm > 0) {
      s_last_hr_update_time = start + (time_t)i * 60;
      s_last_hr_bpm = minute_data[i].heart_rate_bpm;
      break;
    }
  }
}

static void hr_health_handler(HealthEventType event, void *context) {
  if (event == HealthEventHeartRateUpdate) {
    int bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
    // HealthEventHeartRateUpdate also fires when HealthMetricHeartRateRawBPM
    // changes, which happens a cycle *before* the filtered HealthMetricHeartRateBPM
    // we display recomputes. Bumping the age timestamp on every event would reset
    // the age line to "0m" while peek() still returns the previous BPM, so the old
    // number lingers for ~a minute next to a 0m age. Only treat it as a new reading
    // (resetting the age) when the displayed filtered value actually changes.
    if (bpm > 0 && bpm != s_last_hr_bpm) {
      s_last_hr_update_time = time(NULL);
      s_last_hr_bpm = bpm;
    }
    Sidebar_redraw();
  }
}
#endif

void SidebarWidgets_init() {
// load fonts
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  smSidebarFont = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  mdSidebarFont = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  lgSidebarFont = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
#else
  smSidebarFont = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  mdSidebarFont = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  lgSidebarFont = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
#endif

  // load the sidebar graphics
  dateImage = gdraw_command_image_create_with_resource(RESOURCE_ID_DATE_BG);
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  dateLgImage =
      gdraw_command_image_create_with_resource(RESOURCE_ID_DATE_BG_LG);
#endif
  disconnectImage =
      gdraw_command_image_create_with_resource(RESOURCE_ID_DISCONNECTED);
  batteryImage =
      gdraw_command_image_create_with_resource(RESOURCE_ID_BATTERY_BG);
  batteryChargeImage =
      gdraw_command_image_create_with_resource(RESOURCE_ID_BATTERY_CHARGE);

#ifdef PBL_HEALTH
  sleepImage =
      gdraw_command_image_create_with_resource(RESOURCE_ID_HEALTH_SLEEP);
  stepsImage =
      gdraw_command_image_create_with_resource(RESOURCE_ID_HEALTH_STEPS);
  heartImage =
      gdraw_command_image_create_with_resource(RESOURCE_ID_HEALTH_HEART);
  deepSleepImage =
      gdraw_command_image_create_with_resource(RESOURCE_ID_HEALTH_DEEP_SLEEP);
#endif

  // set up widgets' function pointers correctly
  batteryMeterWidget.getHeight = BatteryMeter_getHeight;
  batteryMeterWidget.draw = BatteryMeter_draw;

  batteryDaysWidget.getHeight = BatteryDays_getHeight;
  batteryDaysWidget.draw = BatteryDays_draw;

  emptyWidget.getHeight = EmptyWidget_getHeight;
  emptyWidget.draw = EmptyWidget_draw;

  dateWidget.getHeight = DateWidget_getHeight;
  dateWidget.draw = DateWidget_draw;

  currentWeatherWidget.getHeight = CurrentWeather_getHeight;
  currentWeatherWidget.draw = CurrentWeather_draw;

  weatherForecastWidget.getHeight = WeatherForecast_getHeight;
  weatherForecastWidget.draw = WeatherForecast_draw;

  btDisconnectWidget.getHeight = BTDisconnect_getHeight;
  btDisconnectWidget.draw = BTDisconnect_draw;

  weekNumberWidget.getHeight = WeekNumber_getHeight;
  weekNumberWidget.draw = WeekNumber_draw;

  secondsWidget.getHeight = Seconds_getHeight;
  secondsWidget.draw = Seconds_draw;

  altTimeWidget.getHeight = AltTime_getHeight;
  altTimeWidget.draw = AltTime_draw;

  uvIndexWidget.getHeight = UVIndex_getHeight;
  uvIndexWidget.draw = UVIndex_draw;

#ifdef PBL_HEALTH
  stepCounterWidget.getHeight = StepCounter_getHeight;
  stepCounterWidget.draw = StepCounter_draw;

  distanceWidget.getHeight = Distance_getHeight;
  distanceWidget.draw = Distance_draw;

  sleepTimerWidget.getHeight = SleepTimer_getHeight;
  sleepTimerWidget.draw = SleepTimer_draw;

  deepSleepWidget.getHeight = DeepSleep_getHeight;
  deepSleepWidget.draw = DeepSleep_draw;

  sleepCombinedWidget.getHeight = SleepCombined_getHeight;
  sleepCombinedWidget.draw = SleepCombined_draw;

  sleepDeepBarWidget.getHeight = SleepDeepBar_getHeight;
  sleepDeepBarWidget.draw = SleepDeepBar_draw;

  heartRateWidget.getHeight = HeartRate_getHeight;
  heartRateWidget.draw = HeartRate_draw;

  health_service_events_subscribe(hr_health_handler, NULL);
  hr_seed_from_history();
#endif

  beatsWidget.getHeight = Beats_getHeight;
  beatsWidget.draw = Beats_draw;

  electricityWidget.getHeight = Electricity_getHeight;
  electricityWidget.draw = Electricity_draw;

  nextCheapWidget.getHeight = NextCheap_getHeight;
  nextCheapWidget.draw = NextCheap_draw;

  cheapestHourWidget.getHeight = CheapestHour_getHeight;
  cheapestHourWidget.draw = CheapestHour_draw;

  cryptoWidget.getHeight = CryptoSlot_getHeight;
  cryptoWidget.draw      = CryptoSlot_draw;

  tuyaLedsWidget.getHeight = TuyaLeds_getHeight;
  tuyaLedsWidget.draw      = TuyaLeds_drawWidget;

  electricityBoltPath = gpath_create(&ELEC_BOLT_PATH_INFO);
}

void SidebarWidgets_deinit() {
  gdraw_command_image_destroy(dateImage);
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  gdraw_command_image_destroy(dateLgImage);
#endif
  gdraw_command_image_destroy(disconnectImage);
  gdraw_command_image_destroy(batteryImage);
  gdraw_command_image_destroy(batteryChargeImage);

#ifdef PBL_HEALTH
  gdraw_command_image_destroy(stepsImage);
  gdraw_command_image_destroy(sleepImage);
  gdraw_command_image_destroy(heartImage);
  gdraw_command_image_destroy(deepSleepImage);

  health_service_events_unsubscribe();
#endif
  gpath_destroy(electricityBoltPath);
}

void SidebarWidgets_updateFonts() {
  if (settings.useLargeFonts) {
    currentSidebarFont = lgSidebarFont;
    batteryFont = lgSidebarFont;
  } else {
    currentSidebarFont = mdSidebarFont;
    batteryFont = smSidebarFont;
  }

  if (settings.useLargeFonts) {
    layout = (SidebarWidgetLayout){
        .batteryGraphicOnlyHeight = 14,
        .batteryWithPctHeight = 33,
        .batteryTextY = 14,
        .dateHeight = 62,
        .dateHeightCompact = 42,
        .dateTopCorrection = 10,
        .dateDayNumY = 24,
        .dateMonthY = 48,
        .dateBgX = 3,
        .dateBgY = 23,
        .dateBgRectX = 2,
        .dateBgRectY = 30,
        .dateBgRectWidth = 26,
        .dateBgRectHeight = 22,
        .weatherHeight = 44,
        .weatherTempY = 20,
        .weatherStationY = 42,
        .textRectX = -5,
        .textRectWidth = 40,
        .weatherForecastHeight = 63,
        .forecastHighY = 20,
        .forecastDividerY = 38,
        .forecastLowY = 39,
        .forecastDividerX = 3,
        .forecastDividerWidth = 24,
        .basicWidgetHeight = 29,
        .basicWidgetLabelY = -4,
        .basicWidgetY = 6,
        .btDisconnectHeight = 22,
        .heartRateHeight = 54,
        .heartRateValueY = 17,
        .heartRateAgeY = 40,
        .stepCounterHeight = 32,
        .stepsTextY = 13,
        .sleepTimerHeight = 32,
        .sleepTextY = 13,
        .sleepComboHeight = 34,
        .sleepComboBarY = 1,
        .sleepComboTotalY = 4,
        .sleepComboDeepY = 21,
        .sleepBarWidgetHeight = 42,
        .sleepBarY = 33,
        .sleepBarThickness = 5,
        .secondsHeight = 14,
        .secondsY = -10,
        .tuyaLedDiameter = 9,
        .tuyaLedGap = 2,
    };
  } else {
    layout = (SidebarWidgetLayout){
        .batteryGraphicOnlyHeight = 14,
        .batteryWithPctHeight = 27,
        .batteryTextY = 18,
        .dateHeight = 58,
        .dateHeightCompact = 41,
        .dateTopCorrection = 7,
        .dateDayNumY = 26,
        .dateMonthY = 47,
        .dateBgX = 3,
        .dateBgY = 23,
        .dateBgRectX = 2,
        .dateBgRectY = 30,
        .dateBgRectWidth = 26,
        .dateBgRectHeight = 22,
        .weatherHeight = 42,
        .weatherTempY = 24,
        .weatherStationY = 46,
        .textRectX = -5,
        .textRectWidth = 40,
        .weatherForecastHeight = 60,
        .forecastHighY = 24,
        .forecastDividerY = 37,
        .forecastLowY = 42,
        .forecastDividerX = 3,
        .forecastDividerWidth = 24,
        .basicWidgetHeight = 26,
        .basicWidgetLabelY = -4,
        .basicWidgetY = 9,
        .btDisconnectHeight = 22,
        .heartRateHeight = 58,
        .heartRateValueY = 21,
        .heartRateAgeY = 44,
        .stepCounterHeight = 36,
        .stepsTextY = 13,
        .sleepTimerHeight = 36,
        .sleepTextY = 13,
        .sleepComboHeight = 34,
        .sleepComboBarY = 1,
        .sleepComboTotalY = 4,
        .sleepComboDeepY = 21,
        .sleepBarWidgetHeight = 46,
        .sleepBarY = 33,
        .sleepBarThickness = 5,
        .secondsHeight = 14,
        .secondsY = -10,
        .tuyaLedDiameter = 9,
        .tuyaLedGap = 2,
    };
  }

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  // Larger fonts on these platforms — override layout values.
  // These start from the standard large-font values as a baseline; tune as
  // needed.
  if (settings.useLargeFonts) {
    layout.batteryWithPctHeight = 38;
    layout.batteryTextY = 15;
    layout.dateHeight = 74;
    layout.dateHeightCompact = 50;
    layout.dateTopCorrection = 9;
    layout.dateDayNumY = 28;
    layout.dateMonthY = 56;
    layout.dateBgX = 1;
    layout.dateBgY = 29;
    layout.dateBgRectX = 0;
    layout.dateBgRectY = 34;
    layout.dateBgRectWidth = 30;
    layout.dateBgRectHeight = 26;
    layout.weatherHeight = 49;
    layout.weatherTempY = 21;
    layout.weatherStationY = 46;
    layout.textRectX = -10;
    layout.textRectWidth = 50;
    layout.weatherForecastHeight = 76;
    layout.forecastHighY = 21;
    layout.forecastDividerY = 45;
    layout.forecastLowY = 48;
    layout.forecastDividerX = 1;
    layout.forecastDividerWidth = 28;
    layout.basicWidgetHeight = 35;
    layout.basicWidgetLabelY = -6;
    layout.basicWidgetY = 8;
    layout.btDisconnectHeight = 22;
    layout.heartRateHeight = 62;
    layout.heartRateValueY = 20;
    layout.heartRateAgeY = 45;
    layout.stepCounterHeight = 35;
    layout.stepsTextY = 10;
    layout.sleepTimerHeight = 35;
    layout.sleepTextY = 10;
    layout.sleepComboHeight = 42;
    layout.sleepComboBarY = 1;
    layout.sleepComboTotalY = 3;
    layout.sleepComboDeepY = 25;
    layout.sleepBarWidgetHeight = 47;
    layout.sleepBarY = 37;
    layout.sleepBarThickness = 7;
    layout.secondsHeight = 18;
    layout.secondsY = -10;
    layout.tuyaLedDiameter = 11;
    layout.tuyaLedGap = 3;
  } else {
    layout.batteryWithPctHeight = 30;
    layout.batteryTextY = 17;
    layout.dateHeight = 67;
    layout.dateHeightCompact = 47;
    layout.dateTopCorrection = 10;
    layout.dateDayNumY = 29;
    layout.dateMonthY = 53;
    layout.dateBgX = 1;
    layout.dateBgY = 29;
    layout.dateBgRectX = 0;
    layout.dateBgRectY = 35;
    layout.dateBgRectWidth = 30;
    layout.dateBgRectHeight = 26;
    layout.weatherHeight = 46;
    layout.weatherTempY = 22;
    layout.weatherStationY = 47;
    layout.textRectX = -9;
    layout.textRectWidth = 48;
    layout.weatherForecastHeight = 69;
    layout.forecastHighY = 22;
    layout.forecastDividerY = 42;
    layout.forecastLowY = 45;
    layout.forecastDividerX = 1;
    layout.forecastDividerWidth = 28;
    layout.basicWidgetHeight = 32;
    layout.basicWidgetLabelY = -4;
    layout.basicWidgetY = 12;
    layout.btDisconnectHeight = 22;
    layout.heartRateHeight = 58;
    layout.heartRateValueY = 20;
    layout.heartRateAgeY = 43;
    layout.stepCounterHeight = 32;
    layout.stepsTextY = 11;
    layout.sleepTimerHeight = 32;
    layout.sleepTextY = 11;
    layout.sleepComboHeight = 42;
    layout.sleepComboBarY = 1;
    layout.sleepComboTotalY = 3;
    layout.sleepComboDeepY = 25;
    layout.sleepBarWidgetHeight = 44;
    layout.sleepBarY = 38;
    layout.sleepBarThickness = 7;
    layout.secondsHeight = 18;
    layout.secondsY = -10;
    layout.tuyaLedDiameter = 11;
    layout.tuyaLedGap = 3;
  }
#endif
}

// c can't do true modulus on negative numbers, apparently
// from
// http://stackoverflow.com/questions/11720656/modulo-operation-with-negative-numbers
int mod(int a, int b) {
  int r = a % b;
  return r < 0 ? r + b : r;
}

void SidebarWidgets_updateTime(struct tm *timeInfo) {
  // printf("Current RAM: %d", heap_bytes_free());

  // set all the date strings
  strftime(currentDayNum, 3, "%e", timeInfo);
  // remove padding on date num, if needed
  if (currentDayNum[0] == ' ') {
    currentDayNum[0] = currentDayNum[1];
    currentDayNum[1] = '\0';
  }

  strftime(currentWeekNum, 3, "%V", timeInfo);

  strncpy(currentDayName, dayNames[settings.languageId][timeInfo->tm_wday],
          sizeof(currentDayName));
  strncpy(currentMonth, monthNames[settings.languageId][timeInfo->tm_mon],
          sizeof(currentMonth));

  // set the seconds string
  strftime(currentSecondsNum, 4, ":%S", timeInfo);

  if (dynamicSettings.enableAltTimeZone) {
    // set the alternate time zone string
    int hour =
        timeInfo->tm_hour - timeInfo->tm_gmtoff / 60 / 60 - timeInfo->tm_isdst;

    // apply the configured offset value
    hour += settings.altclockOffset;

    char am_pm = (mod(hour, 24) < 12) ? 'a' : 'p';

    // format it
    if (clock_is_24h_style()) {
      hour = mod(hour, 24);
      am_pm = (char)0;
    } else {
      hour = mod(hour, 12);
      if (hour == 0) {
        hour = 12;
      }
    }

    if (settings.showLeadingZero && hour < 10) {
      snprintf(altClock, sizeof(altClock), "0%i%c", hour, am_pm);
    } else {
      snprintf(altClock, sizeof(altClock), "%i%c", hour, am_pm);
    }
  }

  if (dynamicSettings.enableBeats) {
    int beats = 0;

    // set the swatch internet time beats
    beats = time_get_beats(timeInfo);

    snprintf(currentBeats, sizeof(currentBeats), "%i", beats);
  }
}

/* Sidebar Widget Selection */
SidebarWidget getSidebarWidgetByType(SidebarWidgetType type) {
  if (Crypto_isWid((uint8_t)type) || Currency_isWid((uint8_t)type) || Tuya_isWid((uint8_t)type)) { return cryptoWidget; }
  switch (type) {
  case BATTERY_METER:
    return batteryMeterWidget;
    break;
  case BATTERY_DAYS:
    return batteryDaysWidget;
    break;
  case BLUETOOTH_DISCONNECT:
    return btDisconnectWidget;
    break;
  case DATE:
    return dateWidget;
    break;
  case ALT_TIME_ZONE:
    return altTimeWidget;
    break;
  case SECONDS:
    return secondsWidget;
    break;
  case WEATHER_CURRENT:
    return currentWeatherWidget;
    break;
  case WEATHER_FORECAST_TODAY:
    return weatherForecastWidget;
    break;
  case WEEK_NUMBER:
    return weekNumberWidget;
  case WEATHER_UV_INDEX:
    return uvIndexWidget;
  case ELECTRICITY:
    return electricityWidget;
  case NEXT_CHEAP_ELEC:
    return nextCheapWidget;
  case CHEAPEST_ELEC_HOUR:
    return cheapestHourWidget;
  case TUYA_LEDS:
    return tuyaLedsWidget;
#ifdef PBL_HEALTH
  case STEP_COUNTER:
    return stepCounterWidget;
  case DISTANCE:
    return distanceWidget;
  case SLEEP_TIMER:
    return sleepTimerWidget;
  case DEEP_SLEEP_TIMER:
    return deepSleepWidget;
  case SLEEP_COMBINED:
    return sleepCombinedWidget;
  case SLEEP_DEEP_BAR:
    return sleepDeepBarWidget;
  case HEARTRATE:
    return heartRateWidget;
#endif
  case BEATS:
    return beatsWidget;
  default:
    return emptyWidget;
    break;
  }
}

/********** functions for the empty widget **********/
int EmptyWidget_getHeight() { return 0; }

void EmptyWidget_draw(GContext *ctx, int yPosition) { return; }

/********** functions for the battery meter widget **********/

int BatteryMeter_getHeight() {
  // Fixed per the showBatteryPct setting (not the live charging state): a widget
  // must reserve a constant height so neighbours don't reflow when charging
  // starts/stops. The draw code still hides the % while charging.
  if (!settings.showBatteryPct) { return layout.batteryGraphicOnlyHeight; }  // icon only: no-op
  return SidebarWidgets_hideIdentifier
      ? (layout.batteryWithPctHeight - layout.batteryTextY)
      : layout.batteryWithPctHeight;
}

void BatteryMeter_draw(GContext *ctx, int yPosition) {

  BatteryChargeState chargeState = battery_state_service_peek();
  uint8_t battery_percent =
      (chargeState.charge_percent > 0) ? chargeState.charge_percent : 5;

  graphics_context_set_text_color(ctx, theme.sidebarTextColor);

  char batteryString[6];
  int batteryPositionY =
      yPosition - 5; // correct for vertical empty space on battery icon

  if (!SidebarWidgets_hideIdentifier) {
    if (batteryImage) {
      gdraw_command_image_recolor(batteryImage, theme.iconFillColor,
                                  theme.iconStrokeColor);
      gdraw_command_image_draw(
          ctx, batteryImage,
          GPoint(3 + SidebarWidgets_xOffset, batteryPositionY));
    }

    if (chargeState.is_charging) {
      if (batteryChargeImage) {
        // the charge "bolt" icon uses inverted colors
        gdraw_command_image_recolor(batteryChargeImage,
                                    theme.iconStrokeColor,
                                    theme.iconFillColor);
        gdraw_command_image_draw(
            ctx, batteryChargeImage,
            GPoint(3 + SidebarWidgets_xOffset, batteryPositionY));
      }
    } else {

      int width = roundf(18 * battery_percent / 100.0f);

      graphics_context_set_fill_color(ctx, theme.iconStrokeColor);

#ifdef PBL_COLOR
      if (battery_percent <= 20) {
        graphics_context_set_fill_color(ctx, GColorRed);
      }
#endif

      graphics_fill_rect(
          ctx, GRect(6 + SidebarWidgets_xOffset, 8 + batteryPositionY, width, 8),
          0, GCornerNone);
    }
  }

  // never show battery % while charging, because of this issue:
  // https://github.com/freakified/TimeStylePebble/issues/11
  if (settings.showBatteryPct && !chargeState.is_charging) {
    if (!settings.useLargeFonts) {
      // put the percent sign on the opposite side if turkish
      snprintf(batteryString, sizeof(batteryString),
               (settings.languageId == LANGUAGE_TR) ? "%%%d" : "%d%%",
               battery_percent);
    } else {
      snprintf(batteryString, sizeof(batteryString), "%d", battery_percent);
    }
    // When the identifier (icon) is hidden, base the value on yPosition (the cell
    // top), NOT batteryPositionY: the -5 in batteryPositionY corrects for the empty
    // space atop the battery ICON, which is irrelevant with no icon, and would push
    // the value 5px high in the short value-only cell. This matches the clean
    // icon+value widgets (steps/sleep), whose hidden value lands at yPosition.
    int valueBaseY = SidebarWidgets_hideIdentifier ? yPosition : batteryPositionY;
    int hs = SidebarWidgets_hideIdentifier ? layout.batteryTextY : 0;
    graphics_draw_text(ctx, batteryString, batteryFont,
                       GRect(layout.textRectX + SidebarWidgets_xOffset,
                             layout.batteryTextY + valueBaseY - hs,
                             layout.textRectWidth, 20),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

/********** functions for the battery days estimate widget **********/

int BatteryDays_getHeight() {
  // Always shows a value (the days number), so reserve the with-value height.
  return SidebarWidgets_hideIdentifier
      ? (layout.batteryWithPctHeight - layout.batteryTextY)
      : layout.batteryWithPctHeight;
}

void BatteryDays_draw(GContext *ctx, int yPosition) {
  BatteryChargeState chargeState = battery_state_service_peek();
  uint8_t battery_percent =
      (chargeState.charge_percent > 0) ? chargeState.charge_percent : 5;

  graphics_context_set_text_color(ctx, theme.sidebarTextColor);

  char daysString[8];
  int batteryPositionY = yPosition - 5; // correct for vertical empty space on battery icon

  if (!SidebarWidgets_hideIdentifier) {
    if (batteryImage) {
      gdraw_command_image_recolor(batteryImage, theme.iconFillColor,
                                  theme.iconStrokeColor);
      gdraw_command_image_draw(
          ctx, batteryImage,
          GPoint(3 + SidebarWidgets_xOffset, batteryPositionY));
    }

    if (chargeState.is_charging) {
      if (batteryChargeImage) {
        gdraw_command_image_recolor(batteryChargeImage,
                                    theme.iconStrokeColor,
                                    theme.iconFillColor);
        gdraw_command_image_draw(
            ctx, batteryChargeImage,
            GPoint(3 + SidebarWidgets_xOffset, batteryPositionY));
      }
    } else {
      int width = roundf(18 * battery_percent / 100.0f);
      graphics_context_set_fill_color(ctx, theme.iconStrokeColor);
#ifdef PBL_COLOR
      if (battery_percent <= 20) {
        graphics_context_set_fill_color(ctx, GColorRed);
      }
#endif
      graphics_fill_rect(
          ctx, GRect(6 + SidebarWidgets_xOffset, 8 + batteryPositionY, width, 8),
          0, GCornerNone);
    }
  }

  // The discharge estimate is meaningless while charging -> show only the bolt/icon.
  if (!chargeState.is_charging) {
    int tenths = BatteryDays_currentEstimateTenths();
    if (tenths == BATTERY_DAYS_NONE) {
      snprintf(daysString, sizeof(daysString), "--");      // warm-up / not enough data
    } else {
      snprintf(daysString, sizeof(daysString), "%d%c%d",
               tenths / 10, settings.decimalSeparator, tenths % 10);
    }
    int valueBaseY = SidebarWidgets_hideIdentifier ? yPosition : batteryPositionY;
    int hs = SidebarWidgets_hideIdentifier ? layout.batteryTextY : 0;
    graphics_draw_text(ctx, daysString, batteryFont,
                       GRect(layout.textRectX + SidebarWidgets_xOffset,
                             layout.batteryTextY + valueBaseY - hs,
                             layout.textRectWidth, 20),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

/********** current date widget **********/

int DateWidget_getHeight() {
  return layout.dateHeight;
}

void DateWidget_draw(GContext *ctx, int yPosition) {
  graphics_context_set_text_color(ctx, theme.sidebarTextColor);

  // compensate for extra space that appears on the top of the date widget
  yPosition -= layout.dateTopCorrection;

  // first draw the day name
  graphics_draw_text(ctx, currentDayName, currentSidebarFont,
                     GRect(layout.textRectX + SidebarWidgets_xOffset, yPosition,
                           layout.textRectWidth, 20),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  // next, draw the date background
  // (an image in normal mode, a rectangle in large font mode)
  if (!settings.useLargeFonts) {
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    GDrawCommandImage *activeDateImage = dateLgImage;
#else
    GDrawCommandImage *activeDateImage = dateImage;
#endif
    if (activeDateImage) {
      gdraw_command_image_recolor(activeDateImage,
                                  theme.iconFillColor,
                                  theme.iconStrokeColor);
      gdraw_command_image_draw(ctx, activeDateImage,
                               GPoint(layout.dateBgX + SidebarWidgets_xOffset,
                                      yPosition + layout.dateBgY));
    }
  } else {
    graphics_context_set_fill_color(ctx, theme.iconStrokeColor);
    graphics_fill_rect(ctx,
                       GRect(layout.dateBgRectX + SidebarWidgets_xOffset,
                             yPosition + layout.dateBgRectY,
                             layout.dateBgRectWidth, layout.dateBgRectHeight),
                       2, GCornersAll);

    graphics_context_set_fill_color(ctx, theme.iconFillColor);
    graphics_fill_rect(ctx,
                       GRect(layout.dateBgRectX + 2 + SidebarWidgets_xOffset,
                             yPosition + layout.dateBgRectY + 2,
                             layout.dateBgRectWidth - 4,
                             layout.dateBgRectHeight - 4),
                       0, GCornersAll);
  }

  // next, draw the date number
  graphics_context_set_text_color(ctx, theme.iconStrokeColor);

  int yOffset = layout.dateDayNumY;

  graphics_draw_text(ctx, currentDayNum, currentSidebarFont,
                     GRect(layout.textRectX + SidebarWidgets_xOffset,
                           yPosition + yOffset, layout.textRectWidth, 20),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  // switch back to normal color for the rest
  graphics_context_set_text_color(ctx, theme.sidebarTextColor);

  // draw the month
  yOffset = layout.dateMonthY;

  graphics_draw_text(ctx, currentMonth, currentSidebarFont,
                     GRect(layout.textRectX + SidebarWidgets_xOffset,
                           yPosition + yOffset, layout.textRectWidth, 20),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

/********** current weather widget **********/

int CurrentWeather_getHeight() {
  // When the identifier is hidden the icon is skipped and every text line shifts
  // up by weatherTempY (SHIFT), so the reserved height shrinks by the same amount.
  int hs = SidebarWidgets_hideIdentifier ? layout.weatherTempY : 0;
  // reserve room for the location-name line(s) only when there is a name (so the
  // Open-Meteo / no-name case is laid out exactly as before). Names longer than
  // 4 chars wrap to a second stacked line.
  if (Weather_weatherInfo.stationName[0] != '\0') {
    // wrap to a second line only when it would hold >= 2 chars (a lone 1-char
    // tail looks worse than just truncating to the first line)
    int extra = strlen(Weather_weatherInfo.stationName) >= 6 ? 27 : 16;
    return layout.weatherStationY + extra - hs;
  }
  return layout.weatherHeight - hs;
}

void CurrentWeather_draw(GContext *ctx, int yPosition) {
  graphics_context_set_text_color(ctx, theme.sidebarTextColor);

  int hs = SidebarWidgets_hideIdentifier ? layout.weatherTempY : 0;

  if (!SidebarWidgets_hideIdentifier) {
    if (Weather_currentWeatherIcon) {
      gdraw_command_image_recolor(Weather_currentWeatherIcon,
                                  theme.iconFillColor,
                                  theme.iconStrokeColor);
      gdraw_command_image_draw(ctx, Weather_currentWeatherIcon,
                               GPoint(3 + SidebarWidgets_xOffset, yPosition));
    }
  }

  // draw weather data only if it has been set
  if (Weather_weatherInfo.currentTemp != INT32_MIN) {

    int currentTemp = Weather_weatherInfo.currentTemp;

    if (!settings.useMetric) {
      currentTemp = roundf(currentTemp * 1.8f + 32);
    }

    char tempString[8];

    // in large font mode, omit the degree symbol and move the text
    if (!settings.useLargeFonts) {
      snprintf(tempString, sizeof(tempString), "%d°", currentTemp);
    } else {
      snprintf(tempString, sizeof(tempString), "%d", currentTemp);
    }
    graphics_draw_text(ctx, tempString, currentSidebarFont,
                       GRect(layout.textRectX + SidebarWidgets_xOffset,
                             yPosition + layout.weatherTempY - hs,
                             layout.textRectWidth, 20),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);

    // location name (FMI only), small bold gothic, left-aligned and indented to
    // the widget's left edge (same 3px inset as the icon). JS sends up to 8
    // chars; we stack them as two 4-char lines below the temperature.
    if (Weather_weatherInfo.stationName[0] != '\0') {
      GFont nameFont = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
      int nameX = 1 + SidebarWidgets_xOffset;
      char line1[5] = {0}, line2[5] = {0};
      size_t nlen = strlen(Weather_weatherInfo.stationName);
      size_t n1 = nlen < 4 ? nlen : 4;
      memcpy(line1, Weather_weatherInfo.stationName, n1);
      graphics_draw_text(ctx, line1, nameFont,
                         GRect(nameX, yPosition + layout.weatherStationY - hs,
                               layout.textRectWidth, 16),
                         GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      if (nlen >= 6) {   // only show line 2 if it holds >= 2 chars
        size_t n2 = (nlen - 4) < 4 ? (nlen - 4) : 4;
        memcpy(line2, Weather_weatherInfo.stationName + 4, n2);
        graphics_draw_text(ctx, line2, nameFont,
                           GRect(nameX, yPosition + layout.weatherStationY + 13 - hs,
                                 layout.textRectWidth, 16),
                           GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      }
    }
  } else {
    // if the weather data isn't set, draw a loading indication
    graphics_draw_text(ctx, "...", currentSidebarFont,
                       GRect(layout.textRectX + SidebarWidgets_xOffset,
                             yPosition, layout.textRectWidth, 20),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

/***** Bluetooth Disconnection Widget *****/

int BTDisconnect_getHeight() { return layout.btDisconnectHeight; }

void BTDisconnect_draw(GContext *ctx, int yPosition) {
  if (disconnectImage) {
    gdraw_command_image_recolor(disconnectImage, theme.iconFillColor,
                                theme.iconStrokeColor);

    gdraw_command_image_draw(ctx, disconnectImage,
                             GPoint(3 + SidebarWidgets_xOffset, yPosition));
  }
}

static void draw_basic_widget(GContext *ctx, int yPosition, const char *label,
                              const char *value, int valueYOffset) {
  int hs = SidebarWidgets_hideIdentifier ? layout.basicWidgetY : 0;
  if (!SidebarWidgets_hideIdentifier) {
    graphics_draw_text(ctx, label, smSidebarFont,
                       GRect(layout.textRectX + SidebarWidgets_xOffset,
                             yPosition + layout.basicWidgetLabelY,
                             layout.textRectWidth, 20),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
  graphics_draw_text(ctx, value, currentSidebarFont,
                     GRect(layout.textRectX + SidebarWidgets_xOffset,
                           yPosition + valueYOffset - hs, layout.textRectWidth, 20),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static int basic_widget_height(void) {
  return SidebarWidgets_hideIdentifier
      ? (layout.basicWidgetHeight - layout.basicWidgetY)
      : layout.basicWidgetHeight;
}

/***** Week Number Widget *****/

int WeekNumber_getHeight() { return basic_widget_height(); }

void WeekNumber_draw(GContext *ctx, int yPosition) {
  graphics_context_set_text_color(ctx, theme.sidebarTextColor);
  draw_basic_widget(ctx, yPosition, wordForWeek[settings.languageId],
                    currentWeekNum, layout.basicWidgetY);
}

/***** Seconds Widget *****/

int Seconds_getHeight() { return layout.secondsHeight; }

void Seconds_draw(GContext *ctx, int yPosition) {
  graphics_context_set_text_color(ctx, theme.sidebarTextColor);

  graphics_draw_text(ctx, currentSecondsNum, lgSidebarFont,
                     GRect(layout.textRectX + SidebarWidgets_xOffset,
                           yPosition + layout.secondsY, layout.textRectWidth,
                           20),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

/***** Weather Forecast Widget *****/

int WeatherForecast_getHeight() {
  return SidebarWidgets_hideIdentifier
      ? (layout.weatherForecastHeight - layout.forecastHighY)
      : layout.weatherForecastHeight;
}

void WeatherForecast_draw(GContext *ctx, int yPosition) {
  graphics_context_set_text_color(ctx, theme.sidebarTextColor);

  int hs = SidebarWidgets_hideIdentifier ? layout.forecastHighY : 0;

  if (!SidebarWidgets_hideIdentifier && Weather_forecastWeatherIcon) {
    gdraw_command_image_recolor(Weather_forecastWeatherIcon,
                                theme.iconFillColor,
                                theme.iconStrokeColor);

    gdraw_command_image_draw(ctx, Weather_forecastWeatherIcon,
                             GPoint(3 + SidebarWidgets_xOffset, yPosition));
  }

  // draw weather data only if it has been set
  if (Weather_weatherInfo.todaysHighTemp != INT32_MIN) {

    int todaysHighTemp = Weather_weatherInfo.todaysHighTemp;
    int todaysLowTemp = Weather_weatherInfo.todaysLowTemp;

    if (!settings.useMetric) {
      todaysHighTemp = roundf(todaysHighTemp * 1.8f + 32);
      todaysLowTemp = roundf(todaysLowTemp * 1.8f + 32);
    }

    char tempString[8];

    graphics_context_set_fill_color(ctx, theme.sidebarTextColor);

    // in large font mode, omit the degree symbol and move the text
    if (!settings.useLargeFonts) {
      snprintf(tempString, sizeof(tempString), "%d°", todaysHighTemp);
    } else {
      snprintf(tempString, sizeof(tempString), "%d", todaysHighTemp);
    }
    graphics_draw_text(ctx, tempString, currentSidebarFont,
                       GRect(layout.textRectX + SidebarWidgets_xOffset,
                             yPosition + layout.forecastHighY - hs,
                             layout.textRectWidth, 20),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);

    graphics_fill_rect(ctx,
                       GRect(layout.forecastDividerX + SidebarWidgets_xOffset,
                             8 + yPosition + layout.forecastDividerY - hs,
                             layout.forecastDividerWidth, 1),
                       0, GCornerNone);

    if (!settings.useLargeFonts) {
      snprintf(tempString, sizeof(tempString), "%d°", todaysLowTemp);
    } else {
      snprintf(tempString, sizeof(tempString), "%d", todaysLowTemp);
    }
    graphics_draw_text(ctx, tempString, currentSidebarFont,
                       GRect(layout.textRectX + SidebarWidgets_xOffset,
                             yPosition + layout.forecastLowY - hs,
                             layout.textRectWidth, 20),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  } else {
    // if the weather data isn't set, draw a loading indication
    graphics_draw_text(ctx, "...", currentSidebarFont,
                       GRect(layout.textRectX + SidebarWidgets_xOffset,
                             yPosition - hs, layout.textRectWidth, 20),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

/***** Alternate Time Zone Widget *****/

int AltTime_getHeight() { return basic_widget_height(); }

void AltTime_draw(GContext *ctx, int yPosition) {
  graphics_context_set_text_color(ctx, theme.sidebarTextColor);
  draw_basic_widget(ctx, yPosition, settings.altclockName, altClock,
                    layout.basicWidgetY - 1);
}

/********** UV Index Widget **********/

int UVIndex_getHeight() { return basic_widget_height(); }

void UVIndex_draw(GContext *ctx, int yPosition) {
  graphics_context_set_text_color(ctx, theme.sidebarTextColor);
  char uvString[5];
  // A real UV index is always >= 0. A negative value (INT32_MIN before any data,
  // or -1 sent by the provider when no reading could be obtained -- e.g. FMI at
  // night or its sparse UV station network returned only NaN) means "no data":
  // show a placeholder rather than a misleading 0.
  if (Weather_weatherInfo.currentUVIndex >= 0) {
    snprintf(uvString, sizeof(uvString), "%d",
             (int)Weather_weatherInfo.currentUVIndex);
  } else {
    snprintf(uvString, sizeof(uvString), "--");
  }
  draw_basic_widget(ctx, yPosition, "UV", uvString, layout.basicWidgetY);
}

#ifdef PBL_HEALTH

/***** Step Counter / Distance Widgets *****/

int StepCounter_getHeight() {
  return SidebarWidgets_hideIdentifier
      ? (layout.stepCounterHeight - layout.stepsTextY)
      : layout.stepCounterHeight;
}

// Shared renderer for the Steps / Distance widgets: recolored steps icon on top +
// one centered value line. They differ only in whether the value is today's step
// count or today's walked distance.
static void draw_steps_metric(GContext *ctx, int yPosition, bool use_distance) {
  int hs = SidebarWidgets_hideIdentifier ? layout.stepsTextY : 0;
  if (!SidebarWidgets_hideIdentifier) {
    if (stepsImage) {
      gdraw_command_image_recolor(stepsImage, theme.iconFillColor,
                                  theme.iconStrokeColor);
      gdraw_command_image_draw(ctx, stepsImage,
                               GPoint(3 + SidebarWidgets_xOffset, yPosition - 7));
    }
  }

  char steps_text[8];

  if (use_distance) {
    int distance = 0;

    if (is_health_metric_accessible(HealthMetricWalkedDistanceMeters)) {
      distance =
          (int)health_service_sum_today(HealthMetricWalkedDistanceMeters);
    }

    MeasurementSystem unit_system =
        health_service_get_measurement_system_for_display(
            HealthMetricWalkedDistanceMeters);

    // Format distance as a bare number (no km/mi unit — it wastes sidebar space).
    // Always expressed in the display unit's own scale: one decimal below 10, a
    // whole number at 10 and above (e.g. 0,0 / 0,5 / 5,0 / 9,9 / 12).
    if (unit_system == MeasurementSystemMetric) {
      int tenths = distance / 100; // tenths of a km
      if (tenths < 100) {
        snprintf(steps_text, sizeof(steps_text), "%i%c%i", tenths / 10,
                 settings.decimalSeparator, tenths % 10);
      } else {
        snprintf(steps_text, sizeof(steps_text), "%i", tenths / 10);
      }
    } else {
      int tenths = distance * 10 / 1609; // tenths of a mile
      if (tenths < 100) {
        snprintf(steps_text, sizeof(steps_text), "%i%c%i", tenths / 10,
                 settings.decimalSeparator, tenths % 10);
      } else {
        snprintf(steps_text, sizeof(steps_text), "%i", tenths / 10);
      }
    }
  } else {
    // One syscall, not two: the unconditional call here used to be overwritten by the
    // guarded one on the very next line, so it was pure waste on every frame. When the
    // metric is inaccessible health_service_sum_today returns 0, which is what 0 seeds.
    int steps = 0;
    if (is_health_metric_accessible(HealthMetricStepCount)) {
      steps = (int)health_service_sum_today(HealthMetricStepCount);
    }
#ifdef SCREENSHOT_FIXTURES
    steps = 1454;   // demo: renders "1.4k"
#endif

    // format step string
    if (steps < 1000) {
      snprintf(steps_text, sizeof(steps_text), "%i", steps);
    } else {
      int steps_thousands = steps / 1000;
      int steps_hundreds = steps / 100 % 10;

      if (steps < 10000) {
        snprintf(steps_text, sizeof(steps_text), "%i%c%ik", steps_thousands,
                 settings.decimalSeparator, steps_hundreds);
      } else {
        snprintf(steps_text, sizeof(steps_text), "%ik", steps_thousands);
      }
    }
  }

  graphics_context_set_text_color(ctx, theme.sidebarTextColor);

  graphics_draw_text(
      ctx, steps_text, mdSidebarFont,
      GRect(layout.textRectX + SidebarWidgets_xOffset,
            yPosition + layout.stepsTextY - hs, layout.textRectWidth, 20),
      GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

void StepCounter_draw(GContext *ctx, int yPosition) {
  draw_steps_metric(ctx, yPosition, false);
}

/***** Distance Widget *****/

int Distance_getHeight() {
  return SidebarWidgets_hideIdentifier
      ? (layout.stepCounterHeight - layout.stepsTextY)
      : layout.stepCounterHeight;
}

void Distance_draw(GContext *ctx, int yPosition) {
  draw_steps_metric(ctx, yPosition, true);
}

/***** Sleep Time Widgets *****/

// What the sleep renderer adds around the total-sleep number.
typedef enum {
  SLEEP_EXTRA_NONE = 0,   // Sleep / Deep Sleep: icon + one number
  SLEEP_EXTRA_COMBO,      // Sleep + Deep Sleep: bar in the identifier row, then the
                          // total, then the restful figure in the small font
  SLEEP_EXTRA_BAR         // Sleep + Deep Bar: icon, the total, then the bar
} SleepExtra;

// The y of a variant's FIRST text line. It doubles as the identifier row's height:
// hiding the identifier drops that row (icon, or the combo's bar) and reclaims
// exactly this much vertical space.
static int sleep_top_y(SleepExtra extra) {
  return (extra == SLEEP_EXTRA_COMBO) ? layout.sleepComboTotalY : layout.sleepTextY;
}

static int sleep_widget_height(int fullHeight, SleepExtra extra) {
  return SidebarWidgets_hideIdentifier ? (fullHeight - sleep_top_y(extra)) : fullHeight;
}

static int read_sleep_seconds(HealthMetric metric) {
  if (!is_health_metric_accessible(metric)) { return 0; }
  return (int)health_service_sum_today(metric);
}

// Outlined bar filled to the restful share of the night. The outline is drawn even
// at 0 % so a dataless widget reads as an empty bar rather than a missing one.
static void draw_sleep_bar(GContext *ctx, int barY, int total_s, int deep_s) {
  GRect frame = GRect(layout.forecastDividerX + SidebarWidgets_xOffset, barY,
                      layout.forecastDividerWidth, layout.sleepBarThickness);
  graphics_context_set_stroke_color(ctx, theme.sidebarTextColor);
  graphics_draw_rect(ctx, frame);

  int fill = sleep_bar_fill_px(total_s, deep_s, frame.size.w - 2);
  if (fill > 0) {
    graphics_context_set_fill_color(ctx, theme.sidebarTextColor);
    graphics_fill_rect(ctx,
                       GRect(frame.origin.x + 1, frame.origin.y + 1,
                             fill, layout.sleepBarThickness - 2),
                       0, GCornerNone);
  }
}

// Shared renderer for all sleep widgets: an identifier row (icon, or the combo's
// bar), the total-sleep decimal line, and the restful figure as a small second line
// or a fill bar underneath.
static void draw_sleep_widget(GContext *ctx, int yPosition, GDrawCommandImage *img,
                              HealthMetric metric, SleepExtra extra) {
  int topY = sleep_top_y(extra);
  int hs = SidebarWidgets_hideIdentifier ? topY : 0;

  int sleep_seconds = read_sleep_seconds(metric);
  int deep_seconds = (extra == SLEEP_EXTRA_NONE)
      ? 0 : read_sleep_seconds(HealthMetricSleepRestfulSeconds);
#ifdef SCREENSHOT_FIXTURES
  // demo: 6.5 h slept, 1.75 h of it restful (~27 % of the night)
  sleep_seconds = (metric == HealthMetricSleepRestfulSeconds) ? 6300 : 23400;
  deep_seconds = 6300;
#endif

  if (!SidebarWidgets_hideIdentifier) {
    if (extra == SLEEP_EXTRA_COMBO) {
      // The combo widget spends its identifier row on the bar instead of the icon:
      // two numbers plus the share, in less height than the icon variant.
      draw_sleep_bar(ctx, yPosition + layout.sleepComboBarY, sleep_seconds, deep_seconds);
    } else if (img) {
      gdraw_command_image_recolor(img, theme.iconFillColor,
                                  theme.iconStrokeColor);
      gdraw_command_image_draw(ctx, img,
                               GPoint(3 + SidebarWidgets_xOffset, yPosition - 7));
    }
  }

  char sleep_text[12];
  sleep_format_decimal(sleep_seconds, settings.decimalSeparator,
                       sleep_text, sizeof(sleep_text));

  graphics_context_set_text_color(ctx, theme.sidebarTextColor);
  graphics_draw_text(ctx, sleep_text, mdSidebarFont,
                     GRect(layout.textRectX + SidebarWidgets_xOffset,
                           yPosition + topY - hs, layout.textRectWidth, 20),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  if (extra == SLEEP_EXTRA_COMBO) {
    sleep_format_decimal(deep_seconds, settings.decimalSeparator,
                         sleep_text, sizeof(sleep_text));
    graphics_draw_text(ctx, sleep_text, smSidebarFont,
                       GRect(layout.textRectX + SidebarWidgets_xOffset,
                             yPosition + layout.sleepComboDeepY - hs,
                             layout.textRectWidth, 20),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  } else if (extra == SLEEP_EXTRA_BAR) {
    draw_sleep_bar(ctx, yPosition + layout.sleepBarY - hs, sleep_seconds, deep_seconds);
  }
}

int SleepTimer_getHeight() {
  return sleep_widget_height(layout.sleepTimerHeight, SLEEP_EXTRA_NONE);
}

void SleepTimer_draw(GContext *ctx, int yPosition) {
  draw_sleep_widget(ctx, yPosition, sleepImage, HealthMetricSleepSeconds,
                    SLEEP_EXTRA_NONE);
}

/***** Deep (Restful) Sleep Widget *****/

int DeepSleep_getHeight() {
  return sleep_widget_height(layout.sleepTimerHeight, SLEEP_EXTRA_NONE);
}

void DeepSleep_draw(GContext *ctx, int yPosition) {
  draw_sleep_widget(ctx, yPosition, deepSleepImage, HealthMetricSleepRestfulSeconds,
                    SLEEP_EXTRA_NONE);
}

/***** Sleep + Deep Sleep Widget (share bar, total, restful in the small font) *****/

int SleepCombined_getHeight() {
  return sleep_widget_height(layout.sleepComboHeight, SLEEP_EXTRA_COMBO);
}

void SleepCombined_draw(GContext *ctx, int yPosition) {
  draw_sleep_widget(ctx, yPosition, NULL, HealthMetricSleepSeconds,
                    SLEEP_EXTRA_COMBO);
}

/***** Sleep + Deep Bar Widget (total number + restful-share bar) *****/

int SleepDeepBar_getHeight() {
  return sleep_widget_height(layout.sleepBarWidgetHeight, SLEEP_EXTRA_BAR);
}

void SleepDeepBar_draw(GContext *ctx, int yPosition) {
  draw_sleep_widget(ctx, yPosition, sleepImage, HealthMetricSleepSeconds,
                    SLEEP_EXTRA_BAR);
}

int HeartRate_getHeight() {
  return SidebarWidgets_hideIdentifier
      ? (layout.heartRateHeight - layout.heartRateValueY) : layout.heartRateHeight;
}

void HeartRate_draw(GContext *ctx, int yPosition) {
  int hs = SidebarWidgets_hideIdentifier ? layout.heartRateValueY : 0;

  if (!SidebarWidgets_hideIdentifier && heartImage) {
    gdraw_command_image_recolor(heartImage, theme.iconFillColor,
                                theme.iconStrokeColor);
    gdraw_command_image_draw(ctx, heartImage,
                             GPoint(3 + SidebarWidgets_xOffset, yPosition));
  }

  int yOffset = layout.heartRateValueY;

  // TODO: accessibility check?
  int heart_rate = health_service_peek_current_value(HealthMetricHeartRateBPM);
  // The live peek has no value right after a cold start (e.g. install) until the
  // next automatic sample; fall back to the last reading from minute history so
  // the BPM and the age line below it stay consistent instead of showing "0".
  if (heart_rate <= 0) {
    heart_rate = s_last_hr_bpm;
  }
  char heart_rate_text[8];

  snprintf(heart_rate_text, sizeof(heart_rate_text), "%i", heart_rate);

  graphics_context_set_text_color(ctx, theme.sidebarTextColor);
  graphics_draw_text(ctx, heart_rate_text, currentSidebarFont,
                     GRect(layout.textRectX + SidebarWidgets_xOffset,
                           yPosition + yOffset - hs, layout.textRectWidth, 20),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  // minutes since the last reading, drawn under the BPM value
  int age_min = (s_last_hr_update_time == 0)
                    ? -1
                    : (int)((time(NULL) - s_last_hr_update_time) / 60);
  char age_text[5];
  if (age_min < 0) {
    strcpy(age_text, "--");
  } else if (age_min > 59) {
    strcpy(age_text, "60+m");
  } else {
    snprintf(age_text, sizeof(age_text), "%dm", age_min);
  }

  graphics_draw_text(ctx, age_text, smSidebarFont,
                     GRect(layout.textRectX + SidebarWidgets_xOffset,
                           yPosition + layout.heartRateAgeY - hs,
                           layout.textRectWidth, 20),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

#endif

/***** Beats (Swatch Internet Time) widget *****/

int Beats_getHeight() { return basic_widget_height(); }

void Beats_draw(GContext *ctx, int yPosition) {
  graphics_context_set_text_color(ctx, theme.sidebarTextColor);
  draw_basic_widget(ctx, yPosition, "@", currentBeats, layout.basicWidgetY - 1);
}

/***** Electricity widgets (current price / next cheap / cheapest hour) *****/

// Shared renderer: lightning bolt + a large line and a small line below it.
// The bolt is shorter than the heart-rate icon this layout borrows from;
// pull the number block up a few px.
#define ELEC_Y_NUDGE (-6)

static void elec_draw_bolt(GContext *ctx, int yPosition) {
  if (!SidebarWidgets_hideIdentifier && electricityBoltPath) {
    gpath_move_to(electricityBoltPath,
                  GPoint(9 + SidebarWidgets_xOffset, yPosition));
    graphics_context_set_fill_color(ctx, theme.iconStrokeColor);
    gpath_draw_filled(ctx, electricityBoltPath);
  }
  graphics_context_set_text_color(ctx, theme.sidebarTextColor);
}

static void elec_draw_small_line(GContext *ctx, int yPosition, const char *small) {
  int hs = SidebarWidgets_hideIdentifier ? (layout.heartRateValueY + ELEC_Y_NUDGE) : 0;
  graphics_draw_text(ctx, small, smSidebarFont,
                     GRect(layout.textRectX + SidebarWidgets_xOffset,
                           yPosition + layout.heartRateAgeY + ELEC_Y_NUDGE - hs,
                           layout.textRectWidth, 20),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void elec_draw_two_line(GContext *ctx, int yPosition,
                               const char *big, const char *small,
                               GFont bigFont) {
  int hs = SidebarWidgets_hideIdentifier ? (layout.heartRateValueY + ELEC_Y_NUDGE) : 0;
  elec_draw_bolt(ctx, yPosition);
  graphics_draw_text(ctx, big, bigFont,
                     GRect(layout.textRectX + SidebarWidgets_xOffset,
                           yPosition + layout.heartRateValueY + ELEC_Y_NUDGE - hs,
                           layout.textRectWidth, 20),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  elec_draw_small_line(ctx, yPosition, small);
}

// Cheap-widget big line: the hour in the full font + minutes ("00".."45") one
// font size down, the pair measured and centered as a block. A later-day window
// gets a small "+" drawn as a superscript ABOVE the minutes (not appended), so
// the block width stays hour+minutes and the worst case ("23" + "45") still
// fits the narrow sidebar. Keeps the hour big/legible.
static void elec_draw_split_time(GContext *ctx, int yPosition,
                                 const ElecDisplay *d, const char *small) {
  int hs = SidebarWidgets_hideIdentifier ? (layout.heartRateValueY + ELEC_Y_NUDGE) : 0;
  elec_draw_bolt(ctx, yPosition);
  char hourStr[4], minStr[3];
  snprintf(hourStr, sizeof(hourStr), "%d", d->startHour);
  snprintf(minStr, sizeof(minStr), "%02d", d->startMin);

  GFont hourFont = currentSidebarFont;
  GFont minFont = smSidebarFont;
  GRect probe = GRect(0, 0, 80, 30);
  GSize hsz = graphics_text_layout_get_content_size(
      hourStr, hourFont, probe, GTextOverflowModeFill, GTextAlignmentLeft);
  GSize msz = graphics_text_layout_get_content_size(
      minStr, minFont, probe, GTextOverflowModeFill, GTextAlignmentLeft);

  int rectX = layout.textRectX + SidebarWidgets_xOffset;
  int startX = rectX + (layout.textRectWidth - (hsz.w + msz.w)) / 2;
  int yBig = yPosition + layout.heartRateValueY + ELEC_Y_NUDGE - hs;
  int minX = startX + hsz.w;
  graphics_draw_text(ctx, hourStr, hourFont,
                     GRect(startX, yBig, hsz.w + 4, 30),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  // Bottom-align the shorter minutes box to the hour's baseline.
  graphics_draw_text(ctx, minStr, minFont,
                     GRect(minX, yBig + (hsz.h - msz.h), msz.w + 6, 30),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  // Later-day marker: small "+" tucked above the minutes, adding no width.
  if (!d->today) {
    graphics_draw_text(ctx, "+", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(minX, yBig - 4, msz.w + 6, 16),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
  elec_draw_small_line(ctx, yPosition, small);
}

int Electricity_getHeight() {
  return SidebarWidgets_hideIdentifier
      ? (layout.heartRateHeight - (layout.heartRateValueY + ELEC_Y_NUDGE))
      : layout.heartRateHeight;
}

void Electricity_draw(GContext *ctx, int yPosition) {
  char nowStr[12], avgStr[12];
  int16_t v;
  if (Electricity_getCurrentPrice(&v)) {
    elec_format_price(v, settings.decimalSeparator, nowStr, sizeof(nowStr));
  } else {
    strcpy(nowStr, "--");
  }
  if (Electricity_getTodayAverage(&v)) {
    elec_format_price(v, settings.decimalSeparator, avgStr, sizeof(avgStr));
  } else {
    strcpy(avgStr, "--");
  }
  elec_draw_two_line(ctx, yPosition, nowStr, avgStr, currentSidebarFont);
}

int NextCheap_getHeight() {
  return SidebarWidgets_hideIdentifier
      ? (layout.heartRateHeight - (layout.heartRateValueY + ELEC_Y_NUDGE))
      : layout.heartRateHeight;
}

void NextCheap_draw(GContext *ctx, int yPosition) {
  char smallStr[12];
  ElecDisplay d;
  if (Electricity_getNextCheap(settings.elecQuietStart, settings.elecQuietEnd,
                               settings.elecCheapFactorPct,
                               settings.elecCheapFloorCenti,
                               settings.elecCheapCeilingCenti, &d)) {
    elec_format_price(d.avgCenti, settings.decimalSeparator, smallStr, sizeof(smallStr));
    if (d.now) {
      elec_draw_two_line(ctx, yPosition, "NOW", smallStr, currentSidebarFont);
    } else {
      elec_draw_split_time(ctx, yPosition, &d, smallStr);
    }
  } else {
    elec_draw_two_line(ctx, yPosition, "--", "--", currentSidebarFont);
  }
}

int CheapestHour_getHeight() {
  return SidebarWidgets_hideIdentifier
      ? (layout.heartRateHeight - (layout.heartRateValueY + ELEC_Y_NUDGE))
      : layout.heartRateHeight;
}

void CheapestHour_draw(GContext *ctx, int yPosition) {
  char smallStr[12];
  ElecDisplay d;
  if (Electricity_getCheapestHour(settings.elecQuietStart, settings.elecQuietEnd, &d)) {
    elec_format_price(d.avgCenti, settings.decimalSeparator, smallStr, sizeof(smallStr));
    if (d.now) {
      elec_draw_two_line(ctx, yPosition, "NOW", smallStr, currentSidebarFont);
    } else {
      elec_draw_split_time(ctx, yPosition, &d, smallStr);
    }
  } else {
    elec_draw_two_line(ctx, yPosition, "--", "--", currentSidebarFont);
  }
}

/***** Generic crypto / currency widget *****/

int CryptoSlot_getHeight() { return basic_widget_height(); }

void CryptoSlot_draw(GContext *ctx, int yPosition) {
  graphics_context_set_text_color(ctx, theme.sidebarTextColor);

  uint8_t wid = SidebarWidgets_currentWidgetType;
  CryptoSlot *s = Crypto_isWid(wid) ? Crypto_find(wid)
                : Currency_isWid(wid) ? Currency_find(wid)
                : Tuya_find(wid);
  const char *label = (s && s->label[0]) ? s->label : "--";
  const char *value = (s && s->valid) ? s->value : "--";

  // A long value (e.g. "104000.00", "1.1552") overflows the sidebar in the
  // basic-widget value font on every board (verified for the old EUR widget), so
  // render label + value on two lines with the small sidebar font when the value
  // is wide; otherwise use the basic-widget layout.
  if (strlen(value) > 4) {
    int hs = SidebarWidgets_hideIdentifier ? (layout.basicWidgetY + 3) : 0;
    if (!SidebarWidgets_hideIdentifier) {
      graphics_draw_text(ctx, label, smSidebarFont,
                         GRect(layout.textRectX + SidebarWidgets_xOffset,
                               yPosition + layout.basicWidgetLabelY,
                               layout.textRectWidth, 20),
                         GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }
    graphics_draw_text(ctx, value, smSidebarFont,
                       GRect(layout.textRectX + SidebarWidgets_xOffset,
                             yPosition + layout.basicWidgetY + 3 - hs,
                             layout.textRectWidth, 20),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  } else {
    draw_basic_widget(ctx, yPosition, label, value, layout.basicWidgetY);
  }
}

/***** Tuya LED row *****/

#define TUYA_LED_COLS 3
#define TUYA_LED_PAD  2

// Number of LEDs actually drawn: zero configured/received states still draws one
// hollow ring, so a placed widget is visibly present but declares it knows nothing.
static uint8_t tuya_led_drawn_count() {
  return (TuyaLeds_count == 0) ? 1 : TuyaLeds_count;
}

static uint8_t tuya_led_rows() {
  uint8_t n = tuya_led_drawn_count();
  return (n + TUYA_LED_COLS - 1) / TUYA_LED_COLS;
}

// The preferred diameter/gap can overflow the sidebar (30px on the 144px boards,
// 34/39 on emery), which clipped the rightmost blob. Shrink the gap first (keeps the
// blobs as large as possible), then the diameter, until a full row fits inside the
// sidebar. Board-agnostic, so no per-platform tuning can get it wrong.
static int tuya_led_fit(int *gapOut) {
  int gap = layout.tuyaLedGap;
  int d = layout.tuyaLedDiameter;
  while (d > 3) {
    int r = d / 2;
    int rowW = TUYA_LED_COLS * d + (TUYA_LED_COLS - 1) * gap;
    int x0 = 15 + SidebarWidgets_xOffset - rowW / 2 + r;
    if ((x0 - r) >= 0 && (x0 + (TUYA_LED_COLS - 1) * (d + gap) + r) <= (sidebarWidth - 1)) { break; }
    if (gap > 1) { gap--; } else { d--; }
  }
  *gapOut = gap;
  return d;
}

int TuyaLeds_getHeight() {
  int gap;
  int d = tuya_led_fit(&gap);
  int rows = tuya_led_rows();
  return rows * d + (rows - 1) * gap + 2 * TUYA_LED_PAD;
}

void TuyaLeds_drawWidget(GContext *ctx, int yPosition) {
  int gap;
  const int d = tuya_led_fit(&gap);
  const int r = d / 2;
  const uint8_t n = tuya_led_drawn_count();
  const bool noData = (TuyaLeds_count == 0);

  // SidebarWidgets_xOffset centres content the same way the other widgets use it
  // (it is (sidebarWidth - 30) / 2, so 15 + it is the sidebar's centre column).
  const int centreX = 15 + SidebarWidgets_xOffset;

  int y = yPosition + TUYA_LED_PAD;
  uint8_t i = 0;
  while (i < n) {
    uint8_t cols = n - i;
    if (cols > TUYA_LED_COLS) { cols = TUYA_LED_COLS; }
    const int rowW = cols * d + (cols - 1) * gap;
    int x = centreX - rowW / 2 + r;
    for (uint8_t c = 0; c < cols; c++, i++) {
      uint8_t state = noData ? TUYA_LED_UNKNOWN : TuyaLeds_states[i];
      GPoint p = GPoint(x, y + r);
#ifdef PBL_COLOR
      if (state == TUYA_LED_ON || state == TUYA_LED_OFF) {
        graphics_context_set_fill_color(ctx, (state == TUYA_LED_ON) ? GColorGreen : GColorRed);
        graphics_fill_circle(ctx, p, r);
      }
      // Outline in the sidebar text colour keeps the blob readable on any sidebar
      // background, and IS the whole indicator for the unknown state.
      graphics_context_set_stroke_color(ctx, theme.sidebarTextColor);
      graphics_draw_circle(ctx, p, r);
#else
      // Black & white: on = filled, off = outline only, unknown = outline + centre dot.
      graphics_context_set_stroke_color(ctx, theme.sidebarTextColor);
      graphics_context_set_fill_color(ctx, theme.sidebarTextColor);
      if (state == TUYA_LED_ON) {
        graphics_fill_circle(ctx, p, r);
      } else if (state == TUYA_LED_UNKNOWN) {
        graphics_fill_circle(ctx, p, 1);
      }
      graphics_draw_circle(ctx, p, r);
#endif
      x += d + gap;
    }
    y += d + gap;
  }
}
