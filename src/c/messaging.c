#include <pebble.h>
#include <string.h>
#include "weather.h"
#include "settings.h"
#include "messaging.h"
#include "electricity.h"
#include "crypto.h"
#include "currency.h"
#include "tuya.h"
#include "tuya_leds.h"
#include "languages.h"
#include "night_window_calc.h"
#include "widget_list.h"
#include "test_hooks.h"

void (*message_processed_callback)(void);

void messaging_requestNewWeatherData(bool cold) {
  // The request carries a single dummy key 0; its VALUE is the cold flag.
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint32(iter, 0, cold ? 1 : 0);
  app_message_outbox_send();
}

void messaging_init(void (*processed_callback)(void)) {
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

// Store a widget list tuple (a byte array) as the list of a position.
static void apply_widget_list(const Tuple *t, int pos) {
  if (t == NULL || t->type != TUPLE_BYTE_ARRAY) { return; }
  Settings_setWidgetList(pos, t->value->data, t->length);
}

void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Snapshot settings so the save below is gated on an ACTUAL change: this callback
  // runs for every inbound message, including pure data replies.
  //
  // `static` because the Pebble app stack is only ~2 KB and Settings alone is ~172 B;
  // the event loop is single-threaded so a non-reentrant handler is safe. memcpy (not
  // struct assignment) so padding bytes are copied too and memcmp cannot see phantom
  // differences.
  static Settings prevSettings;
  memcpy(&prevSettings, &settings, sizeof(Settings));

  TestHooks_handleMessage(iterator);

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

  Tuple *weatherRainMax_tuple = dict_find(iterator, MESSAGE_KEY_WeatherRainMax);
  if (weatherRainMax_tuple != NULL
      && rain_parse(weatherRainMax_tuple->value->data, weatherRainMax_tuple->length,
                    Weather_weatherInfo.rainMax)) {
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
  Tuple *useMetric_tuple = dict_find(iterator, MESSAGE_KEY_SettingUseMetric);
  Tuple *btVibe_tuple = dict_find(iterator, MESSAGE_KEY_SettingBluetoothVibe);
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
  Tuple *movingCentre_tuple = dict_find(iterator, MESSAGE_KEY_SettingMovingCentre);
  Tuple *minuteOffscreen_tuple = dict_find(iterator, MESSAGE_KEY_SettingMinuteOffscreen);
  Tuple *btWarnColor_tuple = dict_find(iterator, MESSAGE_KEY_SettingBtWarnColor);
  Tuple *elecQuietStart_tuple = dict_find(iterator, MESSAGE_KEY_SettingElecQuietStart);
  Tuple *elecQuietEnd_tuple = dict_find(iterator, MESSAGE_KEY_SettingElecQuietEnd);
  Tuple *elecFactor_tuple = dict_find(iterator, MESSAGE_KEY_SettingElecCheapFactorPct);
  Tuple *elecFloor_tuple = dict_find(iterator, MESSAGE_KEY_SettingElecCheapFloorCenti);
  Tuple *elecCeiling_tuple = dict_find(iterator, MESSAGE_KEY_SettingElecCheapCeilingCenti);
  Tuple *language_tuple = dict_find(iterator, MESSAGE_KEY_SettingLanguageID);
  Tuple *leadingZero_tuple = dict_find(iterator, MESSAGE_KEY_SettingShowLeadingZero);
  Tuple *hourlyVibe_tuple = dict_find(iterator, MESSAGE_KEY_SettingHourlyVibe);




  Tuple *widgetList_tuple = dict_find(iterator, MESSAGE_KEY_SettingWidgetList);
  Tuple *rightWidgetList_tuple = dict_find(iterator, MESSAGE_KEY_SettingRightWidgetList);
  Tuple *centerWidgetList_tuple = dict_find(iterator, MESSAGE_KEY_SettingCenterWidgetList);

  Tuple *altclockName_tuple = dict_find(iterator, MESSAGE_KEY_SettingAltClockName);
  Tuple *altclockOffset_tuple = dict_find(iterator, MESSAGE_KEY_SettingAltClockOffset);

  Tuple *decimalSeparator_tuple = dict_find(iterator, MESSAGE_KEY_SettingDecimalSep);

  Tuple *autobattery_tuple = dict_find(iterator, MESSAGE_KEY_SettingDisableAutobattery);

  Tuple *activateDisconnectIcon_tuple = dict_find(iterator, MESSAGE_KEY_SettingDisconnectIcon);

  Tuple *autoBatteryThreshold_tuple = dict_find(iterator, MESSAGE_KEY_SettingAutoBatteryThreshold);
  Tuple *fallbackColumn_tuple = dict_find(iterator, MESSAGE_KEY_SettingFallbackColumn);
  Tuple *fallbackPosition_tuple = dict_find(iterator, MESSAGE_KEY_SettingFallbackPosition);

  if(timeColor_tuple != NULL) {
    settings.timeColor = GColorFromHEX(timeColor_tuple->value->int32);
  }

  if(bgColor_tuple != NULL) {
    settings.timeBgColor = GColorFromHEX(bgColor_tuple->value->int32);
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
  if(movingCentre_tuple != NULL) {
    settings.fixedCentre = movingCentre_tuple->value->int32 ? 0 : 1;
  }
  if(minuteOffscreen_tuple != NULL) {
    settings.minuteFinite = minuteOffscreen_tuple->value->int32 ? 0 : 1;
  }
  if(btWarnColor_tuple != NULL) {
    settings.btWarnColor = GColorFromHEX(btWarnColor_tuple->value->int32);
  }


  if(useMetric_tuple != NULL) {
    settings.useMetric = (bool)useMetric_tuple->value->int8;
  }

  if(btVibe_tuple != NULL) {
    settings.btVibe = (bool)btVibe_tuple->value->int8;
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

  if(autobattery_tuple != NULL) {
    settings.disableAutobattery = (bool)autobattery_tuple->value->int8;
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







  // The three widget lists (payload v2, see widget_list.h). Each is clamped to its
  // position's byte budget and sanitized.
  apply_widget_list(widgetList_tuple, WIDGET_POS_LEFT);
  apply_widget_list(rightWidgetList_tuple, WIDGET_POS_RIGHT);
  apply_widget_list(centerWidgetList_tuple, WIDGET_POS_CENTER);

  if(altclockName_tuple != NULL) {
    strncpy(settings.altclockName, altclockName_tuple->value->cstring, sizeof(settings.altclockName));
  }

  if(altclockOffset_tuple != NULL) {
    settings.altclockOffset = altclockOffset_tuple->value->int8;
  }

  if(decimalSeparator_tuple != NULL) {
    settings.decimalSeparator = (char)decimalSeparator_tuple->value->int8;
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
  Tuple *precipUnit_tuple = dict_find(iterator, MESSAGE_KEY_SettingPrecipUnit);
  if (precipUnit_tuple != NULL) {
    int v = precipUnit_tuple->value->int32;
    if (v == RAIN_UNIT_MM || v == RAIN_UNIT_INCH) { settings.precipUnit = (uint8_t)v; }
  }
  if(fallbackPosition_tuple != NULL) {
    int v = fallbackPosition_tuple->value->int32;
    if (v >= 1 && v <= MAX_WIDGET_SLOTS + 1) { settings.fallbackPosition = (uint8_t)v; }
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
