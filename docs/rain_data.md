# Rain data (phone to watch)

Data for the "maximum hourly precipitation in the next N hours" widgets (ids
40..44): what the phone sends and how the watch shows it.

## Watch side

- `src/c/rain_calc.{c,h}` (pure, host-tested in `tests/test_rain_calc.c`): parse, value
  text, unit label.
- `WeatherInfo.rainMax[5]` (last field, persisted with the weather blob; an older, shorter
  blob loads with all values unknown). `settings.precipUnit` (0 mm, 1 inch).
- Value: mm with one decimal (`1.4`, `0.0`), inch with two (`0.06`), the configured
  decimal separator, `--` when unknown. Unit: `mm/h` / `in/h` for 1 h (stacked `mm` over
  `h`), otherwise `hi mm/3 h` (stacked `hi mm` over `3 h`). Icon: the light-rain PDC.
- Placed ids count as weather widgets (`disableWeather`), and a rain widget is "source
  missing" while its value is unknown, so a cold poll fills it.

## Message key

`WeatherRainMax` (declared at the end of `package.json` `pebble.messageKeys`,
next to `SettingPrecipUnit`). A byte array of **10 bytes** = **5 little-endian
uint16** values:

| index | bytes | window |
|-------|-------|--------|
| 0 | 0-1 | max over the next 1 h  |
| 1 | 2-3 | max over the next 3 h  |
| 2 | 4-5 | max over the next 6 h  |
| 3 | 6-7 | max over the next 9 h  |
| 4 | 8-9 | max over the next 12 h |

Value = `uint16` in **hundredths of a millimetre** (140 = 1.40 mm/h), the maximum
HOURLY amount (mm per hour) over the window, not a sum. Always millimetres:
conversion to inches (`mm / 25.4`) happens on the watch.

**Unknown sentinel: `0xFFFF`** (bytes `FF FF`). Real values are clamped to at most
`0xFFFE`. `0` means "known, dry".

It is sent in the same AppMessage as the other weather keys on every weather
fetch (both sources), and takes part in the same "skip send if byte-identical
to the last acknowledged payload unless forced (cold)" logic. Because it
is always present, a poll whose only change is the rain maxima still sends.

C side: read with `dict_find(iter, MESSAGE_KEY_WeatherRainMax)`, check
`tuple->length == 10`, then `uint16_t v = data[2*i] | (data[2*i+1] << 8)`.

## Setting

`SettingPrecipUnit` (radiogroup "Precipitation unit"): `0` = mm (default),
`1` = inch. Sent straight through like the other `Setting*` keys (a 0/1 int).
It only controls how the watch formats the value. The config shows it only when a
rain widget is placed.

## Reserved widget ids

| id | widget |
|----|--------|
| 40 | rain max, next 1 h |
| 41 | rain max, next 3 h |
| 42 | rain max, next 6 h |
| 43 | rain max, next 9 h |
| 44 | rain max, next 12 h |

Index into the byte array = `id - 40`. Phone side already treats these ids as
needing weather: `index.ts` sets `disable_weather = 'no'` when any is placed, and
`config_clay_custom.ts` shows the Weather section (location, data source) plus
`SettingPrecipUnit` when any is placed. The ids are in `widget_options.ts`,
`config_widget_list.ts` (labels `WEATHER max rain N h`) and `widget_contract.md`.

## The hourly series p[0..11]

`p[i]` = precipitation (mm) falling during the hour `[H + i h, H + (i+1) h)`,
with `H` = now rounded down to a full hour. So `p[0]` is **the current hour**
(partially elapsed). Both sources stamp an hourly amount with the END of the
hour it accumulated over, so `p[i]` is the sample stamped `H + (i+1) h`; samples
not on an exact full hour are ignored. An hour with no usable sample (missing,
NaN, `null`) is unknown. Negative values are clamped to 0.

Max over the first N hours skips unknown hours: it is the maximum of the known
ones, and `0xFFFF` only when none of the N hours is known. Pure code and tests:
`src/ts/rain_max.ts`, `tests/rain_max.test.js`.

### FMI

Same stored query the temperature already uses
(`fmi::forecast::edited::weather::scandinavia::point::timevaluepair`,
`timestep=60`). The request's `parameters` became
`temperature,smartsymbol,Precipitation1h`; no extra request. Parameter and query
follow `pebble-sae-weather-graph`, which uses the same query with
`Precipitation1h` (mm in the hour). The response carries the parameter name as
requested, so the parser matches `param=` case-insensitively
(`weather_fmi_fc_parse.ts`, field `precip`).

The window used to be 00:00-23:59 local today; it now ends at the later of that and
`H + 12 h` so the next 12 hours are covered after dinner. The parser takes the day
end so today's high/low are unchanged.

### Open-Meteo

The existing request gained `hourly=uv_index,precipitation` (mm per hour,
the sum over the PRECEDING hour, stamped at its end). `timezone=auto` makes
`hourly.time` local ISO strings of the location, so they are converted to epoch
with the response's `utc_offset_seconds`, not the phone's zone
(`openMeteoPrecipSamples`). FMI failure falls back to Open-Meteo, so the rain
values then come from Open-Meteo.
