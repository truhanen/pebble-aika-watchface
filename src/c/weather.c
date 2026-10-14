#include <pebble.h>
#include "weather.h"

// Has any weather ever reached this watch? (persist restore or a phone push)
static bool s_hasData = false;

bool Weather_hasData(void) { return s_hasData; }

WeatherInfo Weather_weatherInfo;

GDrawCommandImage* Weather_currentWeatherIcon;
GDrawCommandImage* Weather_forecastWeatherIcon;

uint32_t getConditionIcon(WeatherCondition conditionCode) {
  uint32_t iconToLoad;

  switch(conditionCode) {
    case CLEAR_DAY:
      iconToLoad = RESOURCE_ID_WEATHER_CLEAR_DAY;
      break;
    case CLEAR_NIGHT:
      iconToLoad = RESOURCE_ID_WEATHER_CLEAR_NIGHT;
      break;
    case CLOUDY_DAY:
      iconToLoad = RESOURCE_ID_WEATHER_CLOUDY;
      break;
    case HEAVY_RAIN:
      iconToLoad = RESOURCE_ID_WEATHER_HEAVY_RAIN;
      break;
    case HEAVY_SNOW:
      iconToLoad = RESOURCE_ID_WEATHER_HEAVY_SNOW;
      break;
    case LIGHT_RAIN:
      iconToLoad = RESOURCE_ID_WEATHER_LIGHT_RAIN;
      break;
    case LIGHT_SNOW:
      iconToLoad = RESOURCE_ID_WEATHER_LIGHT_SNOW;
      break;
    case PARTLY_CLOUDY_NIGHT:
      iconToLoad = RESOURCE_ID_WEATHER_PARTLY_CLOUDY_NIGHT;
      break;
    case PARTLY_CLOUDY:
      iconToLoad = RESOURCE_ID_WEATHER_PARTLY_CLOUDY;
      break;
    case RAINING_AND_SNOWING:
      iconToLoad = RESOURCE_ID_WEATHER_RAINING_AND_SNOWING;
      break;
    case THUNDERSTORM:
      iconToLoad = RESOURCE_ID_WEATHER_THUNDERSTORM;
      break;
    default:
      iconToLoad = RESOURCE_ID_WEATHER_GENERIC;
      break;
  }

  return iconToLoad;
}

void Weather_setCurrentCondition(int conditionCode) {

  uint32_t currentWeatherIcon = getConditionIcon(conditionCode);

  // ok, now load the new icon:
  gdraw_command_image_destroy(Weather_currentWeatherIcon);
  Weather_currentWeatherIcon = gdraw_command_image_create_with_resource(currentWeatherIcon);

  Weather_weatherInfo.currentIconResourceID = currentWeatherIcon;
}

void Weather_setForecastCondition(int conditionCode) {
  uint32_t forecastWeatherIcon = getConditionIcon(conditionCode);

  gdraw_command_image_destroy(Weather_forecastWeatherIcon);
  Weather_forecastWeatherIcon = gdraw_command_image_create_with_resource(forecastWeatherIcon);

  Weather_weatherInfo.forecastIconResourceID = forecastWeatherIcon;
}

void Weather_init() {
  // initialize weather with default values
  Weather_weatherInfo.currentTemp = INT32_MIN;
  Weather_weatherInfo.currentUVIndex = INT32_MIN;
  Weather_weatherInfo.todaysHighTemp = INT32_MIN;
  Weather_weatherInfo.todaysLowTemp = INT32_MIN;

  // if possible, load weather data from persistent storage
  if (persist_exists(WEATHER_PERSIST_KEY)) {
    WeatherInfo storedWeather;
    persist_read_data(WEATHER_PERSIST_KEY, &storedWeather, sizeof(storedWeather));
    memcpy(&Weather_weatherInfo, &storedWeather, sizeof(storedWeather));
    s_hasData = true;
  }

  // stationName is not relied on from persistence (older blobs lack it; it is
  // re-sent on the next FMI update) -- always start empty to avoid garbage.
  Weather_weatherInfo.stationName[0] = '\0';

  // load icon if needed
  if (Weather_weatherInfo.currentIconResourceID > 0) {
    Weather_currentWeatherIcon = gdraw_command_image_create_with_resource(Weather_weatherInfo.currentIconResourceID);
  } else {
    Weather_currentWeatherIcon = NULL;
  }
  
  if (Weather_weatherInfo.forecastIconResourceID > 0) {
    Weather_forecastWeatherIcon = gdraw_command_image_create_with_resource(Weather_weatherInfo.forecastIconResourceID);
  } else {
    Weather_forecastWeatherIcon = NULL;
  }

#ifdef SCREENSHOT_FIXTURES
  // Demo weather for appstore screenshots: forecast 18/11 °C, UV 3.
  Weather_weatherInfo.todaysHighTemp = 18;
  Weather_weatherInfo.todaysLowTemp = 11;
  Weather_weatherInfo.currentUVIndex = 3;
  Weather_weatherInfo.currentTemp = 16;
  Weather_setForecastCondition(PARTLY_CLOUDY);
#endif
}

void Weather_saveData() {
  s_hasData = true;
  persist_write_data(WEATHER_PERSIST_KEY, &Weather_weatherInfo, sizeof(WeatherInfo));
}

void Weather_deinit() {
  // save weather data to persistent storage
  Weather_saveData();

  // free memory
  gdraw_command_image_destroy(Weather_currentWeatherIcon);
  gdraw_command_image_destroy(Weather_forecastWeatherIcon);
}
