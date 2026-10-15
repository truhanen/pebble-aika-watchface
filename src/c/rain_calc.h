#pragma once
// Pure helpers for the "max rain in the next N hours" widgets (ids 40..44). No Pebble SDK.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RAIN_WINDOWS 5          // N = 1, 3, 6, 9, 12 hours
#define RAIN_UNKNOWN 0xFFFF     // unknown sentinel of the phone's hundredths-of-mm values
#define RAIN_UNIT_MM 0
#define RAIN_UNIT_INCH 1

// Window length in hours for window index 0..4; 0 for an invalid index.
int rain_window_hours(int index);

// Parse the WeatherRainMax payload (10 bytes, 5 little-endian uint16). False (out
// untouched) when the length is wrong.
bool rain_parse(const uint8_t *data, int len, uint16_t out[RAIN_WINDOWS]);

// The value text: mm with one decimal ("1.4"), inch with two ("0.06"), `sep` as the
// decimal separator; "--" for RAIN_UNKNOWN. `hundredths` is mm/h * 100.
void rain_format_value(uint16_t hundredths, int unit, char sep, char *buf, size_t n);

// The unit label for window `index`: "mm/h" / "in/h" for the 1 h window, otherwise
// "hi mm/3 h" (the '/' makes the stacked fraction "hi mm" over "3 h").
void rain_format_unit(int index, int unit, char *buf, size_t n);
