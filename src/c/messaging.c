#include <pebble.h>
#include <string.h>
#include "weather.h"
#include "settings.h"
#include "twt_status.h"
#include "midi_status.h"
#include "messaging.h"
#include "electricity.h"
#include "crypto.h"
#include "currency.h"
#include "tuya.h"
#include "tuya_leds.h"
#include "languages.h"
#include "date_header_calc.h"
#include "night_window_calc.h"
#include "widget_list.h"

void (*message_processed_callback)(void);

// Suppress recording-transition vibration on the first MIDI status message after
// launch, so we don't buzz merely because we learned recording was already active.
static bool s_midiSeen = false;

void messaging_requestNewWeatherData(bool cold) {
  // The request carries a single dummy key; its VALUE is the cold flag. Keeping the
  // key set unchanged means poll_request.ts's key-name filter still rejects the
  // companion-app status pushes that arrive on this same UUID.
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint32(iter, 0, cold ? 1 : 0);
  app_message_outbox_send();
}

void messaging_init(void (*processed_callback)(void)) {
  s_midiSeen = false;
  // register my custom callback
  message_processed_callback = processed_callback;

  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  // Open AppMessage
  // inbox must hold the 384-byte electricity price table (192 * int16) + overhead
  app_message_open(1024, 64);

  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Watch messaging is started!");
  app_message_register_inbox_received(inbox_received_callback);
}

void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Snapshot the three persisted structs so each save below can be gated on an ACTUAL
  // change. This callback runs for every inbound message -- including pure data replies
  // (weather / electricity / crypto / currency / tuya) and the companion apps' status
  // pushes, which trackworktime sends every minute whether anything changed or not.
  // Previously each of those wrote settings (2 keys) + the status blob (2 keys) to
  // flash unconditionally, i.e. thousands of identical writes per day.
  //
  // `static` because the Pebble app stack is only ~2 KB and Settings alone is ~172 B;
  // the event loop is single-threaded so a non-reentrant handler is safe. memcpy (not
  // struct assignment) so padding bytes are copied too and memcmp cannot see phantom
  // differences.
  static Settings prevSettings;
  static TwtStatus prevTwt;
  static MidiStatus prevMidi;
  memcpy(&prevSettings, &settings, sizeof(Settings));
  memcpy(&prevTwt, &twt_status, sizeof(TwtStatus));
  memcpy(&prevMidi, &midi_status, sizeof(MidiStatus));

  bool weatherDataUpdated = false;

  // does this message contain current weather conditions?
  Tuple *weatherTemp_tuple = dict_find(iterator, MESSAGE_KEY_WeatherTemperature);
  if(weatherTemp_tuple != NULL) {
    Weather_weatherInfo.currentTemp = (int)weatherTemp_tuple->value->int32;
    weatherDataUpdated = true;
  }

  // station name accompanies an observation-station temperature (FMI). A
  // temperature update WITHOUT a name (Open-Meteo, or the FMI forecast
  // fallback) clears any stale name so it isn't shown for the wrong source.
  Tuple *weatherStation_tuple = dict_find(iterator, MESSAGE_KEY_WeatherStationName);
  if (weatherStation_tuple != NULL) {
    strncpy(Weather_weatherInfo.stationName, weatherStation_tuple->value->cstring,
            sizeof(Weather_weatherInfo.stationName) - 1);
    Weather_weatherInfo.stationName[sizeof(Weather_weatherInfo.stationName) - 1] = '\0';
    weatherDataUpdated = true;
  } else if (weatherTemp_tuple != NULL) {
    Weather_weatherInfo.stationName[0] = '\0';
    weatherDataUpdated = true;
  }

  Tuple *weatherConditions_tuple = dict_find(iterator, MESSAGE_KEY_WeatherCondition);
  if(weatherConditions_tuple != NULL) {
    Weather_setCurrentCondition(weatherConditions_tuple->value->int32);
    weatherDataUpdated = true;
  }

  Tuple *weatherUVIndex_tuple = dict_find(iterator, MESSAGE_KEY_WeatherUVIndex);
  if(weatherUVIndex_tuple != NULL) {
    Weather_weatherInfo.currentUVIndex = (int)weatherUVIndex_tuple->value->int32;
    weatherDataUpdated = true;
  }

  Tuple *weatherForecastCondition_tuple = dict_find(iterator, MESSAGE_KEY_WeatherForecastCondition);
  if(weatherForecastCondition_tuple != NULL) {
    Weather_setForecastCondition(weatherForecastCondition_tuple->value->int32);
    weatherDataUpdated = true;
  }

  Tuple *weatherForecastHigh_tuple = dict_find(iterator, MESSAGE_KEY_WeatherForecastHighTemp);
  if(weatherForecastHigh_tuple != NULL) {
    Weather_weatherInfo.todaysHighTemp = (int)weatherForecastHigh_tuple->value->int32;
    weatherDataUpdated = true;
  }

  Tuple *weatherForecastLow_tuple = dict_find(iterator, MESSAGE_KEY_WeatherForecastLowTemp);
  if(weatherForecastLow_tuple != NULL) {
    Weather_weatherInfo.todaysLowTemp = (int)weatherForecastLow_tuple->value->int32;
    weatherDataUpdated = true;
  }

  // only save new weather if weather info was recieved
  if(weatherDataUpdated) {
    Weather_saveData();
  }

  // does this message contain electricity price data?
  bool electricityUpdated = false;
  Tuple *elecStart_tuple = dict_find(iterator, MESSAGE_KEY_ElecStartEpoch);
  if (elecStart_tuple != NULL) {
    Electricity_info.startEpoch = (uint32_t)elecStart_tuple->value->uint32;
    electricityUpdated = true;
  }
  Tuple *elecPrices_tuple = dict_find(iterator, MESSAGE_KEY_ElecPrices);
  if (elecPrices_tuple != NULL) {
    int n = elecPrices_tuple->length / 2;          // little-endian int16 per quarter
    if (n > ELEC_MAX_QUARTERS) { n = ELEC_MAX_QUARTERS; }
    uint8_t *d = elecPrices_tuple->value->data;
    for (int i = 0; i < n; i++) {
      Electricity_info.prices[i] =
          (int16_t)((uint16_t)d[2 * i] | ((uint16_t)d[2 * i + 1] << 8));
    }
    Electricity_info.count = (uint16_t)n;
    electricityUpdated = true;
  }
  if (electricityUpdated) {
    Electricity_saveData();
  }

  // Crypto: one packed string of wid/label/value triplets; parse + persist it.
  Tuple *crypto_tuple = dict_find(iterator, MESSAGE_KEY_CryptoData);
  if (crypto_tuple != NULL && crypto_tuple->type == TUPLE_CSTRING) {
    Crypto_parse(crypto_tuple->value->cstring);
    persist_write_string(CRYPTO_PERSIST_KEY_DATA, crypto_tuple->value->cstring);
  }

  // Currency: same packed wid/label/value format as crypto, own message key.
  Tuple *currency_tuple = dict_find(iterator, MESSAGE_KEY_CurrencyData);
  if (currency_tuple != NULL && currency_tuple->type == TUPLE_CSTRING) {
    Currency_parse(currency_tuple->value->cstring);
    persist_write_string(CURRENCY_PERSIST_KEY_DATA, currency_tuple->value->cstring);
  }

  // Tuya sensors: same packed wid/label/value format as crypto, own message key.
  Tuple *tuya_tuple = dict_find(iterator, MESSAGE_KEY_TuyaData);
  if (tuya_tuple != NULL && tuya_tuple->type == TUPLE_CSTRING) {
    Tuya_parse(tuya_tuple->value->cstring);
    persist_write_string(TUYA_PERSIST_KEY_DATA, tuya_tuple->value->cstring);
  }

  // Tuya LED row: compact per-switch state string ('1'/'0'/'?'), own message key.
  Tuple *tuya_leds_tuple = dict_find(iterator, MESSAGE_KEY_TuyaLeds);
  if (tuya_leds_tuple != NULL && tuya_leds_tuple->type == TUPLE_CSTRING) {
    TuyaLeds_parse(tuya_leds_tuple->value->cstring);
    persist_write_string(TUYA_LEDS_PERSIST_KEY_DATA, tuya_leds_tuple->value->cstring);
    APP_LOG(APP_LOG_LEVEL_INFO, "tuya leds: %s", tuya_leds_tuple->value->cstring);
  }

  // does this message contain new config information?
  Tuple *timeColor_tuple = dict_find(iterator, MESSAGE_KEY_SettingColorTime);
  Tuple *bgColor_tuple = dict_find(iterator, MESSAGE_KEY_SettingColorBG);
  Tuple *sidebarColor_tuple = dict_find(iterator, MESSAGE_KEY_SettingColorSidebar);
  Tuple *sidebarPos_tuple = dict_find(iterator, MESSAGE_KEY_SettingSidebarOnLeft);
  Tuple *sidebarTextColor_tuple = dict_find(iterator, MESSAGE_KEY_SettingSidebarTextColor);
  Tuple *useMetric_tuple = dict_find(iterator, MESSAGE_KEY_SettingUseMetric);
  Tuple *btVibe_tuple = dict_find(iterator, MESSAGE_KEY_SettingBluetoothVibe);
  Tuple *midiVibe_tuple = dict_find(iterator, MESSAGE_KEY_SettingMidiVibe);
  Tuple *midiSecPrec_tuple = dict_find(iterator, MESSAGE_KEY_SettingMidiSecondPrecision);
  Tuple *bigDate_tuple = dict_find(iterator, MESSAGE_KEY_SettingBigDate);
  Tuple *bigDateMonth_tuple = dict_find(iterator, MESSAGE_KEY_SettingBigDateMonth);
  Tuple *twtShowRemaining_tuple = dict_find(iterator, MESSAGE_KEY_SettingTwtShowRemaining);
  Tuple *twtTargetVibe_tuple = dict_find(iterator, MESSAGE_KEY_SettingTwtTargetVibe);
  Tuple *twtBudgetVibe_tuple = dict_find(iterator, MESSAGE_KEY_SettingTwtBudgetVibe);
  Tuple *pollInterval_tuple = dict_find(iterator, MESSAGE_KEY_SettingPollIntervalMin);
  Tuple *nightRotMode_tuple = dict_find(iterator, MESSAGE_KEY_SettingNightRotationMode);
  Tuple *nightRotStart_tuple = dict_find(iterator, MESSAGE_KEY_SettingNightRotationStart);
  Tuple *nightRotEnd_tuple = dict_find(iterator, MESSAGE_KEY_SettingNightRotationEnd);
  Tuple *nightSlowRot_tuple = dict_find(iterator, MESSAGE_KEY_SettingNightSlowRotation);
  Tuple *nightColors_tuple = dict_find(iterator, MESSAGE_KEY_SettingNightColors);
  Tuple *nightBgColor_tuple = dict_find(iterator, MESSAGE_KEY_SettingNightBgColor);
  Tuple *nightFgColor_tuple = dict_find(iterator, MESSAGE_KEY_SettingNightFgColor);
  Tuple *batWarnPct_tuple = dict_find(iterator, MESSAGE_KEY_SettingBatteryWarnPct);
  Tuple *batWarnDays_tuple = dict_find(iterator, MESSAGE_KEY_SettingBatteryWarnDays);
  Tuple *batWarnColor_tuple = dict_find(iterator, MESSAGE_KEY_SettingBatteryWarnColor);
  Tuple *btWarnBorder_tuple = dict_find(iterator, MESSAGE_KEY_SettingBtWarnBorder);
  Tuple *btWarnColor_tuple = dict_find(iterator, MESSAGE_KEY_SettingBtWarnColor);
  Tuple *elecQuietStart_tuple = dict_find(iterator, MESSAGE_KEY_SettingElecQuietStart);
  Tuple *elecQuietEnd_tuple = dict_find(iterator, MESSAGE_KEY_SettingElecQuietEnd);
  Tuple *elecFactor_tuple = dict_find(iterator, MESSAGE_KEY_SettingElecCheapFactorPct);
  Tuple *elecFloor_tuple = dict_find(iterator, MESSAGE_KEY_SettingElecCheapFloorCenti);
  Tuple *elecCeiling_tuple = dict_find(iterator, MESSAGE_KEY_SettingElecCheapCeilingCenti);
  Tuple *twtStatusBgColor_tuple = dict_find(iterator, MESSAGE_KEY_SettingTwtStatusBgColor);
  Tuple *twtFlashColor_tuple = dict_find(iterator, MESSAGE_KEY_SettingTwtFlashColor);
  Tuple *dateBgColor_tuple = dict_find(iterator, MESSAGE_KEY_SettingDateBgColor);
  Tuple *sidebarBgLeft_tuple = dict_find(iterator, MESSAGE_KEY_SettingSidebarBgColorLeft);
  Tuple *sidebarBgRight_tuple = dict_find(iterator, MESSAGE_KEY_SettingSidebarBgColorRight);
  Tuple *language_tuple = dict_find(iterator, MESSAGE_KEY_SettingLanguageID);
  Tuple *leadingZero_tuple = dict_find(iterator, MESSAGE_KEY_SettingShowLeadingZero);
  Tuple *batteryPct_tuple = dict_find(iterator, MESSAGE_KEY_SettingShowBatteryPct);
  Tuple *clockFont_tuple = dict_find(iterator, MESSAGE_KEY_SettingClockFontId);
  Tuple *bigDateFont_tuple = dict_find(iterator, MESSAGE_KEY_SettingBigDateFont);
  Tuple *hourlyVibe_tuple = dict_find(iterator, MESSAGE_KEY_SettingHourlyVibe);
  Tuple *useLargeFonts_tuple = dict_find(iterator, MESSAGE_KEY_SettingUseLargeFonts);

  Tuple *widget0Id_tuple = dict_find(iterator, MESSAGE_KEY_SettingWidget0ID);
  Tuple *widget1Id_tuple = dict_find(iterator, MESSAGE_KEY_SettingWidget1ID);
  Tuple *widget2Id_tuple = dict_find(iterator, MESSAGE_KEY_SettingWidget2ID);

  Tuple *widget2_0Id_tuple = dict_find(iterator, MESSAGE_KEY_SettingWidget2_0ID);
  Tuple *widget2_1Id_tuple = dict_find(iterator, MESSAGE_KEY_SettingWidget2_1ID);
  Tuple *widget2_2Id_tuple = dict_find(iterator, MESSAGE_KEY_SettingWidget2_2ID);

  Tuple *secondaryAlwaysOn_tuple = dict_find(iterator, MESSAGE_KEY_SettingSecondaryAlwaysOn);

  Tuple *widgetList_tuple = dict_find(iterator, MESSAGE_KEY_SettingWidgetList);
  Tuple *statusStripFullWidth_tuple = dict_find(iterator, MESSAGE_KEY_SettingStatusStripFullWidth);
  Tuple *rightWidgetList_tuple = dict_find(iterator, MESSAGE_KEY_SettingRightWidgetList);

  Tuple *altclockName_tuple = dict_find(iterator, MESSAGE_KEY_SettingAltClockName);
  Tuple *altclockOffset_tuple = dict_find(iterator, MESSAGE_KEY_SettingAltClockOffset);

  Tuple *decimalSeparator_tuple = dict_find(iterator, MESSAGE_KEY_SettingDecimalSep);
  Tuple *healthUseDistance_tuple = dict_find(iterator, MESSAGE_KEY_SettingHealthUseDistance);

  Tuple *autobattery_tuple = dict_find(iterator, MESSAGE_KEY_SettingDisableAutobattery);

  Tuple *activateDisconnectIcon_tuple = dict_find(iterator, MESSAGE_KEY_SettingDisconnectIcon);

  Tuple *autoBatteryThreshold_tuple = dict_find(iterator, MESSAGE_KEY_SettingAutoBatteryThreshold);
  Tuple *fallbackColumn_tuple = dict_find(iterator, MESSAGE_KEY_SettingFallbackColumn);
  Tuple *fallbackPosition_tuple = dict_find(iterator, MESSAGE_KEY_SettingFallbackPosition);
  Tuple *clockStyle_tuple = dict_find(iterator, MESSAGE_KEY_SettingClockStyle);
  Tuple *analogTicks_tuple = dict_find(iterator, MESSAGE_KEY_SettingAnalogTicks);
  Tuple *analogDigital_tuple = dict_find(iterator, MESSAGE_KEY_SettingAnalogDigitalClock);
  Tuple *statusClockDigital_tuple = dict_find(iterator, MESSAGE_KEY_SettingStatusClockDigital);

  if(timeColor_tuple != NULL) {
    settings.timeColor = GColorFromHEX(timeColor_tuple->value->int32);
  }

  if(bgColor_tuple != NULL) {
    settings.timeBgColor = GColorFromHEX(bgColor_tuple->value->int32);
  }

  if(sidebarColor_tuple != NULL) {
    settings.sidebarColor = GColorFromHEX(sidebarColor_tuple->value->int32);
  }

  if(sidebarTextColor_tuple != NULL) {
    // text can only be black or white, so we'll enforce that here
    settings.sidebarTextColor = GColorFromHEX(sidebarTextColor_tuple->value->int32);
  }

  if(twtStatusBgColor_tuple != NULL) {
    int32_t v = twtStatusBgColor_tuple->value->int32;
    settings.twtStatusBgColor = (v < 0) ? GColorClear : GColorFromHEX(v); // <0 = inherit
  }

  if(twtFlashColor_tuple != NULL) {
    settings.twtFlashColor = GColorFromHEX(twtFlashColor_tuple->value->int32); // always solid
  }

  if(dateBgColor_tuple != NULL) {
    int32_t v = dateBgColor_tuple->value->int32;
    settings.dateBgColor = (v < 0) ? GColorClear : GColorFromHEX(v); // <0 = inherit
  }

  if(sidebarBgLeft_tuple != NULL) {
    int32_t v = sidebarBgLeft_tuple->value->int32;
    settings.sidebarBgColorLeft = (v < 0) ? GColorClear : GColorFromHEX(v); // <0 = inherit
  }

  if(sidebarBgRight_tuple != NULL) {
    int32_t v = sidebarBgRight_tuple->value->int32;
    settings.sidebarBgColorRight = (v < 0) ? GColorClear : GColorFromHEX(v); // <0 = inherit
  }

  // Warning frame. Out-of-range values are ignored rather than clamped, matching the
  // night-window apply above: a garbled message must not rewrite a good setting.
  if(batWarnPct_tuple != NULL) {
    int v = batWarnPct_tuple->value->int32;
    if (v >= 0 && v <= 100) { settings.batteryWarnPct = (uint8_t)v; }
  }
  if(batWarnDays_tuple != NULL) {
    int v = batWarnDays_tuple->value->int32;
    if (v >= 0 && v <= 100) { settings.batteryWarnDaysTenths = (uint8_t)v; }
  }
  if(batWarnColor_tuple != NULL) {
    settings.batteryWarnColor = GColorFromHEX(batWarnColor_tuple->value->int32);
  }
  if(btWarnBorder_tuple != NULL) {
    settings.btWarnBorder = btWarnBorder_tuple->value->int32 ? 1 : 0;
  }
  if(btWarnColor_tuple != NULL) {
    settings.btWarnColor = GColorFromHEX(btWarnColor_tuple->value->int32);
  }

  if(sidebarPos_tuple != NULL) {
    settings.sidebarOnLeft = (bool)sidebarPos_tuple->value->int8;
  }

  if(useMetric_tuple != NULL) {
    settings.useMetric = (bool)useMetric_tuple->value->int8;
  }

  if(btVibe_tuple != NULL) {
    settings.btVibe = (bool)btVibe_tuple->value->int8;
  }

  if(midiVibe_tuple != NULL) {
    settings.midiVibe = (bool)midiVibe_tuple->value->int8;
  }

  if(midiSecPrec_tuple != NULL) {
    settings.midiSecondPrecision = (bool)midiSecPrec_tuple->value->int8;
  }

  if(bigDate_tuple != NULL) {
    settings.showBigDate = (bool)bigDate_tuple->value->int8;
  }

  if(bigDateMonth_tuple != NULL) {
    settings.showBigDateMonth = (bool)bigDateMonth_tuple->value->int8;
  }

  if(twtShowRemaining_tuple != NULL) {
    settings.twtShowRemaining = (bool)twtShowRemaining_tuple->value->int8;
  }

  if(twtTargetVibe_tuple != NULL) {
    settings.twtTargetVibe = (bool)twtTargetVibe_tuple->value->int8;
  }

  if(twtBudgetVibe_tuple != NULL) {
    settings.twtBudgetVibe = (bool)twtBudgetVibe_tuple->value->int8;
  }

  if(pollInterval_tuple != NULL) {
    int v = pollInterval_tuple->value->int32;
    if (v >= 5 && v <= 240) { settings.pollIntervalMin = (uint8_t)v; }
  }

  // Night window: apply only in-range values, like the electricity quiet hours. An
  // out-of-range mode is dropped rather than defaulted, so a garbled dict cannot silently
  // freeze the sidebar.
  if(nightRotMode_tuple != NULL) {
    int v = nightRotMode_tuple->value->int32;
    if (v >= NIGHT_WINDOW_OFF && v <= NIGHT_WINDOW_CUSTOM) {
      settings.nightMode = (uint8_t)v;
    }
  }
  if(nightRotStart_tuple != NULL) {
    int v = nightRotStart_tuple->value->int32;
    if (v >= 0 && v <= 23) { settings.nightStartHour = (uint8_t)v; }
  }
  if(nightRotEnd_tuple != NULL) {
    int v = nightRotEnd_tuple->value->int32;
    if (v >= 0 && v <= 23) { settings.nightEndHour = (uint8_t)v; }
  }
  if(nightSlowRot_tuple != NULL) {
    settings.nightSlowRotation = nightSlowRot_tuple->value->int32 ? 1 : 0;
  }
  if(nightColors_tuple != NULL) {
    settings.nightColors = nightColors_tuple->value->int32 ? 1 : 0;
  }
  if(nightBgColor_tuple != NULL) {
    settings.nightBgColor = GColorFromHEX(nightBgColor_tuple->value->int32);
  }
  if(nightFgColor_tuple != NULL) {
    settings.nightFgColor = GColorFromHEX(nightFgColor_tuple->value->int32);
  }

  if(elecQuietStart_tuple != NULL) {
    int v = elecQuietStart_tuple->value->int32;
    if (v >= 0 && v <= 23) { settings.elecQuietStart = (uint8_t)v; }
  }
  if(elecQuietEnd_tuple != NULL) {
    int v = elecQuietEnd_tuple->value->int32;
    if (v >= 0 && v <= 23) { settings.elecQuietEnd = (uint8_t)v; }
  }
  if(elecFactor_tuple != NULL) {
    int v = elecFactor_tuple->value->int32;
    if (v >= 1 && v <= 100) { settings.elecCheapFactorPct = (uint8_t)v; }
  }
  if(elecFloor_tuple != NULL) {
    settings.elecCheapFloorCenti = (int16_t)elecFloor_tuple->value->int32;
  }
  if(elecCeiling_tuple != NULL) {
    settings.elecCheapCeilingCenti = (int16_t)elecCeiling_tuple->value->int32;
  }

  if(leadingZero_tuple != NULL) {
    settings.showLeadingZero = (bool)leadingZero_tuple->value->int8;
  }

  if(batteryPct_tuple != NULL) {
    settings.showBatteryPct = (bool)batteryPct_tuple->value->int8;
  }

  if(autobattery_tuple != NULL) {
    settings.disableAutobattery = (bool)autobattery_tuple->value->int8;
  }

  if(clockFont_tuple != NULL && clockFont_tuple->value->int8 >= FONT_SETTING_DEFAULT
     && clockFont_tuple->value->int8 <= FONT_SETTING_BITHAM) {
    settings.clockFontId = clockFont_tuple->value->int8;
  }

  if(bigDateFont_tuple != NULL && bigDateFont_tuple->value->int8 >= BIG_DATE_FONT_BITHAM
     && bigDateFont_tuple->value->int8 <= BIG_DATE_FONT_SERIF) {
    settings.bigDateFontId = bigDateFont_tuple->value->int8;
  }

  if(useLargeFonts_tuple != NULL) {
    settings.useLargeFonts = (bool)useLargeFonts_tuple->value->int8;
  }

  if(hourlyVibe_tuple != NULL && hourlyVibe_tuple->value->int8 >= NO_VIBE
     && hourlyVibe_tuple->value->int8 <= VIBE_EVERY_HALF_HOUR) {
    settings.hourlyVibe = hourlyVibe_tuple->value->int8;
  }

  if(language_tuple != NULL && language_tuple->value->int8 >= LANGUAGE_EN
     && language_tuple->value->int8 <= LANGUAGE_IW) {
    settings.languageId = language_tuple->value->int8;
  }

  // Apply widget IDs, but reject out-of-range values. Defends against a stale /
  // mismatched config dict (e.g. from an app-message key-id change) writing
  // garbage widget types into settings.
  if(widget0Id_tuple != NULL && widget0Id_tuple->value->int8 >= EMPTY
     && widget0Id_tuple->value->int8 <= MAX_WIDGET_TYPE) {
    settings.widgets[0] = widget0Id_tuple->value->int8;
  }

  if(widget1Id_tuple != NULL && widget1Id_tuple->value->int8 >= EMPTY
     && widget1Id_tuple->value->int8 <= MAX_WIDGET_TYPE) {
    settings.widgets[1] = widget1Id_tuple->value->int8;
  }

  if(widget2Id_tuple != NULL && widget2Id_tuple->value->int8 >= EMPTY
     && widget2Id_tuple->value->int8 <= MAX_WIDGET_TYPE) {
    settings.widgets[2] = widget2Id_tuple->value->int8;
  }

  if(widget2_0Id_tuple != NULL && widget2_0Id_tuple->value->int8 >= EMPTY
     && widget2_0Id_tuple->value->int8 <= MAX_WIDGET_TYPE) {
    settings.widgets2[0] = widget2_0Id_tuple->value->int8;
  }

  if(widget2_1Id_tuple != NULL && widget2_1Id_tuple->value->int8 >= EMPTY
     && widget2_1Id_tuple->value->int8 <= MAX_WIDGET_TYPE) {
    settings.widgets2[1] = widget2_1Id_tuple->value->int8;
  }

  if(widget2_2Id_tuple != NULL && widget2_2Id_tuple->value->int8 >= EMPTY
     && widget2_2Id_tuple->value->int8 <= MAX_WIDGET_TYPE) {
    settings.widgets2[2] = widget2_2Id_tuple->value->int8;
  }

  if(secondaryAlwaysOn_tuple != NULL) {
    settings.secondaryAlwaysOn = (bool)secondaryAlwaysOn_tuple->value->int8;
  }

  // Variable-length widget priority list: a byte array, one widget id per byte,
  // length = number of widgets. Preferred over the 6 legacy SettingWidget*ID keys
  // (still parsed above as a fallback for an un-updated phone). Clamp count to the
  // buffer and reject out-of-range ids.
  if (widgetList_tuple != NULL) {
    int len = widgetList_tuple->length;
    if (len > MAX_WIDGET_LIST) { len = MAX_WIDGET_LIST; }
    memcpy(settings.widgetList, widgetList_tuple->value->data, len);
    settings.widgetCount = WidgetList_sanitize(settings.widgetList, len, MAX_WIDGET_LIST);
    // Mirror the first member of each of the first 3 slots into the legacy widgets[0..2]
    // so the (unchanged) round path keeps tracking the configured widgets.
    WidgetSlot mslots[MAX_WIDGET_SLOTS];
    int ns = WidgetList_parse(settings.widgetList, settings.widgetCount, mslots, MAX_WIDGET_SLOTS);
    for (int i = 0; i < 3; i++) {
      settings.widgets[i] = (i < ns) ? (SidebarWidgetType)mslots[i].members[0] : EMPTY;
    }
  }

  // Right (secondary) sidebar list: a byte array, one widget id per byte. Parsed
  // exactly like SettingWidgetList; never mirrored into the legacy widgets[]
  // (round uses the left list only).
  if (rightWidgetList_tuple != NULL) {
    int len = rightWidgetList_tuple->length;
    if (len > MAX_WIDGET_LIST) { len = MAX_WIDGET_LIST; }
    memcpy(settings.rightWidgetList, rightWidgetList_tuple->value->data, len);
    settings.rightWidgetCount = WidgetList_sanitize(settings.rightWidgetList, len, MAX_WIDGET_LIST);
  }

  if (statusStripFullWidth_tuple != NULL) {
    settings.statusStripFullWidth = (bool)statusStripFullWidth_tuple->value->int8;
  }

  if(altclockName_tuple != NULL) {
    strncpy(settings.altclockName, altclockName_tuple->value->cstring, sizeof(settings.altclockName));
  }

  if(altclockOffset_tuple != NULL) {
    settings.altclockOffset = altclockOffset_tuple->value->int8;
  }

  if(decimalSeparator_tuple != NULL) {
    settings.decimalSeparator = (char)decimalSeparator_tuple->value->int8;
  }

  if(healthUseDistance_tuple != NULL) {
    settings.healthUseDistance = (bool)healthUseDistance_tuple->value->int8;
  }

  if(activateDisconnectIcon_tuple != NULL) {
    settings.activateDisconnectIcon = (bool)activateDisconnectIcon_tuple->value->int8;
  }

  if(autoBatteryThreshold_tuple != NULL) {
    int v = autoBatteryThreshold_tuple->value->int32;
    if (v >= 1 && v <= 100) { settings.autoBatteryThreshold = (uint8_t)v; }
  }
  if(fallbackColumn_tuple != NULL) {
    int v = fallbackColumn_tuple->value->int32;
    if (v >= 0 && v <= 2) { settings.fallbackColumn = (uint8_t)v; }
  }
  if(fallbackPosition_tuple != NULL) {
    int v = fallbackPosition_tuple->value->int32;
    if (v >= 1 && v <= MAX_WIDGET_SLOTS + 1) { settings.fallbackPosition = (uint8_t)v; }
  }
  if(clockStyle_tuple != NULL) {
    int v = clockStyle_tuple->value->int32;
    if (v >= CLOCK_STYLE_DIGITAL && v <= CLOCK_STYLE_ANALOG) { settings.clockStyle = (uint8_t)v; }
  }
  if(analogTicks_tuple != NULL) {
    int v = analogTicks_tuple->value->int32;
    if (v >= ANALOG_TICKS_HIDE && v <= ANALOG_TICKS_BOLD) { settings.analogTickStyle = (uint8_t)v; }
  }
  if(analogDigital_tuple != NULL) {
    settings.analogDigitalClock = (analogDigital_tuple->value->int32 != 0) ? 1 : 0;
  }
  if(statusClockDigital_tuple != NULL) {
    settings.statusClockDigital = (statusClockDigital_tuple->value->int32 != 0) ? 1 : 0;
  }

  // Save the new settings to persistent storage -- but only if a Setting* key actually
  // changed the struct. Settings_saveToStorage() also re-derives dynamicSettings, which
  // is a pure function of `settings` (settings.c Settings_updateDynamicSettings reads
  // only settings.*, writes only dynamicSettings.*, and nothing outside settings.c ever
  // writes dynamicSettings) -- so skipping the whole call on an unchanged struct cannot
  // stale the tick mode or the icon colours.
  if (memcmp(&prevSettings, &settings, sizeof(Settings)) != 0) {
    Settings_saveToStorage();
  }

  // does this message contain TrackWorkTime status?
  bool twtUpdated = false;
  Tuple *twtTracking_tuple = dict_find(iterator, MESSAGE_KEY_TWT_IS_TRACKING);
  if (twtTracking_tuple != NULL) {
    twt_status.isTracking = (twtTracking_tuple->value->uint8 != 0);
    twtUpdated = true;
  }
  Tuple *twtTaskId_tuple = dict_find(iterator, MESSAGE_KEY_TWT_TASK_ID);
  if (twtTaskId_tuple != NULL) {
    twt_status.taskId = twtTaskId_tuple->value->int32;
    twtUpdated = true;
  }
  Tuple *twtTaskName_tuple = dict_find(iterator, MESSAGE_KEY_TWT_TASK_NAME);
  if (twtTaskName_tuple != NULL) {
    strncpy(twt_status.taskName, twtTaskName_tuple->value->cstring, TWT_TASK_NAME_LEN);
    twt_status.taskName[TWT_TASK_NAME_LEN] = '\0';
    twtUpdated = true;
  }
  Tuple *twtWorked_tuple = dict_find(iterator, MESSAGE_KEY_TWT_WORKED_BEFORE_MIN);
  if (twtWorked_tuple != NULL) {
    twt_status.workedBeforeMin = twtWorked_tuple->value->int32;
    twtUpdated = true;
  }
  Tuple *twtTaskWorked_tuple = dict_find(iterator, MESSAGE_KEY_TWT_TASK_WORKED_BEFORE_MIN);
  if (twtTaskWorked_tuple != NULL) {
    twt_status.taskWorkedBeforeMin = twtTaskWorked_tuple->value->int32;
    twtUpdated = true;
  }
  Tuple *twtSegStart_tuple = dict_find(iterator, MESSAGE_KEY_TWT_SEGMENT_START);
  if (twtSegStart_tuple != NULL) {
    twt_status.segmentStartEpoch = twtSegStart_tuple->value->int32;
    twtUpdated = true;
  }
  Tuple *twtDailyTarget_tuple = dict_find(iterator, MESSAGE_KEY_TWT_DAILY_TARGET_MIN);
  if (twtDailyTarget_tuple != NULL) {
    int32_t t = twtDailyTarget_tuple->value->int32;
    // defence-in-depth: clamp to a sane range (0 .. 24h) so a stale/garbled dict
    // can't produce a wild denominator. 0 means "no target -> hide percent/bar".
    if (t < 0) t = 0;
    if (t > 1440) t = 1440;
    twt_status.dailyTargetMin = t;
    twtUpdated = true;
  }
  Tuple *twtTaskBudget_tuple = dict_find(iterator, MESSAGE_KEY_TWT_TASK_BUDGET_MIN);
  if (twtTaskBudget_tuple != NULL) {
    int32_t b = twtTaskBudget_tuple->value->int32;
    // defence-in-depth: clamp to a sane range (0 .. 100000 min ≈ 1666h). 0 = no budget.
    if (b < 0) b = 0;
    if (b > 100000) b = 100000;
    twt_status.taskBudgetMin = b;
    twtUpdated = true;
  }
  Tuple *twtTaskTotal_tuple = dict_find(iterator, MESSAGE_KEY_TWT_TASK_TOTAL_BEFORE_MIN);
  if (twtTaskTotal_tuple != NULL) {
    int32_t v = twtTaskTotal_tuple->value->int32;
    // clamp to the same sane range as the budget so a garbled dict can't overflow task_total
    if (v < 0) v = 0;
    if (v > 100000) v = 100000;
    twt_status.taskTotalBeforeMin = v;
    twtUpdated = true;
  }
  Tuple *twtDayGross_tuple = dict_find(iterator, MESSAGE_KEY_TWT_DAY_GROSS_BEFORE_MIN);
  if (twtDayGross_tuple != NULL) {
    int32_t g = twtDayGross_tuple->value->int32;
    // defence-in-depth: clamp to a sane range (0 .. 24h) like the daily target
    if (g < 0) g = 0;
    if (g > 1440) g = 1440;
    twt_status.dayGrossBeforeMin = g;
    twtUpdated = true;
  }
  // twtUpdated only means "a TWT key was present", and trackworktime pushes all 10 keys
  // every minute regardless of change -- so compare the struct before touching flash.
  if (twtUpdated && memcmp(&prevTwt, &twt_status, sizeof(TwtStatus)) != 0) {
    TwtStatus_save();
    // the redraw + layout happen via message_processed_callback() (redrawScreen -> apply_twt_layout)
  }

  bool midiUpdated = false;
  Tuple *midiRec_tuple = dict_find(iterator, MESSAGE_KEY_MIDI_IS_RECORDING);
  if (midiRec_tuple != NULL) {
    bool wasRecording = midi_status.isRecording;
    bool isRecording = (midiRec_tuple->value->uint8 != 0);
    midi_status.isRecording = isRecording;
    midiUpdated = true;

    if (settings.midiVibe && s_midiSeen && !quiet_time_is_active()
        && wasRecording != isRecording) {
      if (isRecording) {
        vibes_double_pulse();   // recording started
      } else {
        vibes_long_pulse();     // recording stopped
      }
      light_enable_interaction();   // briefly light the backlight, per the user's watch light settings
    }
    s_midiSeen = true;
  }
  Tuple *midiName_tuple = dict_find(iterator, MESSAGE_KEY_MIDI_DEVICE_NAME);
  if (midiName_tuple != NULL) {
    strncpy(midi_status.deviceName, midiName_tuple->value->cstring, MIDI_DEVICE_NAME_LEN);
    midi_status.deviceName[MIDI_DEVICE_NAME_LEN] = '\0';
    midiUpdated = true;
  }
  Tuple *midiStart_tuple = dict_find(iterator, MESSAGE_KEY_MIDI_REC_START);
  if (midiStart_tuple != NULL) {
    midi_status.recStartEpoch = midiStart_tuple->value->int32;
    midiUpdated = true;
  }
  // same as the TWT block: presence of a key is not a change
  if (midiUpdated && memcmp(&prevMidi, &midi_status, sizeof(MidiStatus)) != 0) {
    MidiStatus_save();
    // redraw + relayout happen via message_processed_callback() -> redrawScreen -> apply_twt_layout
  }

  // notify the main screen, in case something changed
  message_processed_callback();
}

void inbox_dropped_callback(AppMessageResult reason, void *context) {
  // APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  // APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed! %d %d %d", reason, APP_MSG_SEND_TIMEOUT, APP_MSG_SEND_REJECTED);

}

void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  // APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}
