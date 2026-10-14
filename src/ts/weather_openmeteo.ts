// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

import * as weatherCommon from './weather';
import type { GeoPosition, WeatherDict } from './weather';

interface OpenMeteoResponse {
  current_weather: {
    temperature: number;
    weathercode: number;
    is_day: number;
  };
  daily: {
    temperature_2m_max: number[];
    temperature_2m_min: number[];
    weathercode: number[];
  };
  hourly: {
    time: string[];
    uv_index: number[];
  };
}

export function getWeatherFromCoords(pos: GeoPosition): void {
  console.log('getting that weather');
  const lat = pos.coords.latitude;
  const lon = pos.coords.longitude;

  const url = 'https://api.open-meteo.com/v1/forecast?latitude=' +
    lat + '&longitude=' + lon +
    '&current_weather=true' +
    '&daily=temperature_2m_max,temperature_2m_min,uv_index_max,weathercode' +
    '&hourly=uv_index' +
    '&timezone=auto';

  console.log(url);

  getAndSendWeather(url);
}

function getAndSendWeather(url: string): void {
  weatherCommon.xhrRequest(url, 'GET', (responseText) => {
    const json: OpenMeteoResponse = JSON.parse(responseText);

    // Handle current weather
    const temperature = Math.round(json.current_weather.temperature);
    const conditionCode = json.current_weather.weathercode;
    const isNight = json.current_weather.is_day !== 1;

    console.log('Current temperature is ' + temperature);
    console.log('Current condition code is ' + conditionCode);

    const iconToLoad = getIconForWeatherCode(conditionCode, isNight);

    // Handle forecast
    const forecastHigh = Math.round(json.daily.temperature_2m_max[0]);
    const forecastLow = Math.round(json.daily.temperature_2m_min[0]);
    const forecastCode = json.daily.weathercode[0];

    const uvIndex = getCurrentUVFromHourly(json);

    console.log('Forecast high/low: ' + forecastHigh + '/' + forecastLow);
    console.log('Forecast condition code: ' + forecastCode);
    console.log('UV Index: ' + uvIndex);
    console.log('Is Night ' + isNight);

    const forecastIcon = getIconForWeatherCode(forecastCode);

    const dictionary: WeatherDict = {
      WeatherTemperature: temperature,
      WeatherCondition: iconToLoad,
      WeatherForecastHighTemp: forecastHigh,
      WeatherForecastLowTemp: forecastLow,
      WeatherForecastCondition: forecastIcon,
      WeatherUVIndex: uvIndex,
    };

    console.log(JSON.stringify(dictionary));

    weatherCommon.sendWeatherToPebble(dictionary);
  });
}

function getCurrentUVFromHourly(json: OpenMeteoResponse): number {
  const now = new Date();
  const times = json.hourly.time;
  const uvValues = json.hourly.uv_index;

  let closestIndex = 0;
  let smallestDiff = Infinity;

  for (let i = 0; i < times.length; i++) {
    const t = new Date(times[i]);
    const diff = Math.abs(t.getTime() - now.getTime());
    if (diff < smallestDiff) {
      smallestDiff = diff;
      closestIndex = i;
    }
  }
  return Math.round(uvValues[closestIndex]);
}

function getIconForWeatherCode(code: number, isNight?: boolean): number {
  switch (code) {
    case 0:
      return isNight ? weatherCommon.icons.CLEAR_NIGHT : weatherCommon.icons.CLEAR_DAY;
    case 1:
    case 2:
      return isNight ? weatherCommon.icons.PARTLY_CLOUDY_NIGHT : weatherCommon.icons.PARTLY_CLOUDY;
    case 3:
    case 45:
    case 48:
      return weatherCommon.icons.CLOUDY_DAY;
    case 51:
    case 53:
    case 55:
    case 61:
    case 80:
      return weatherCommon.icons.LIGHT_RAIN;
    case 63:
    case 65:
    case 81:
    case 82:
      return weatherCommon.icons.HEAVY_RAIN;
    case 56:
    case 57:
    case 66:
    case 67:
      return weatherCommon.icons.RAINING_AND_SNOWING;
    case 71:
    case 77:
    case 85:
      return weatherCommon.icons.LIGHT_SNOW;
    case 73:
    case 75:
    case 86:
      return weatherCommon.icons.HEAVY_SNOW;
    case 95:
    case 96:
    case 99:
      return weatherCommon.icons.THUNDERSTORM;
    default:
      return weatherCommon.icons.WEATHER_GENERIC;
  }
}
