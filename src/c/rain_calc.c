#include "rain_calc.h"
#include <stdio.h>

int rain_window_hours(int index) {
  static const int H[RAIN_WINDOWS] = { 1, 3, 6, 9, 12 };
  return (index >= 0 && index < RAIN_WINDOWS) ? H[index] : 0;
}

bool rain_parse(const uint8_t *data, int len, uint16_t out[RAIN_WINDOWS]) {
  if (!data || len != 2 * RAIN_WINDOWS) { return false; }
  for (int i = 0; i < RAIN_WINDOWS; i++) {
    out[i] = (uint16_t)(data[2 * i] | (data[2 * i + 1] << 8));
  }
  return true;
}

void rain_format_value(uint16_t hundredths, int unit, char sep, char *buf, size_t n) {
  if (hundredths == RAIN_UNKNOWN) { snprintf(buf, n, "--"); return; }
  if (unit == RAIN_UNIT_INCH) {
    // inch*100 = hundredths / 25.4, rounded half up
    unsigned v = ((unsigned)hundredths * 10u + 127u) / 254u;
    snprintf(buf, n, "%u%c%02u", v / 100, sep, v % 100);
  } else {
    unsigned v = ((unsigned)hundredths + 5u) / 10u;
    snprintf(buf, n, "%u%c%u", v / 10, sep, v % 10);
  }
}

void rain_format_unit(int index, int unit, char *buf, size_t n) {
  const char *u = unit == RAIN_UNIT_INCH ? "in" : "mm";
  int h = rain_window_hours(index);
  if (h <= 1) {
    snprintf(buf, n, "%s/h", u);
  } else {
    snprintf(buf, n, "hi %s/%d h", u, h);
  }
}
