#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
#
# Compiles and runs every host-side C test (tests/test_*.c) with plain gcc against
# the pure modules under test -- no Pebble SDK, no emulator. Run via `npm run test:c`
# (and so by `npm test`, which chains it after the node --test suite), so these
# actually re-run on every change instead of only being compiled by hand, which is
# how all 12 of them were run before this script existed.
#
# Must be invoked with the package root as the working directory (true for `npm run`);
# the cd below makes it work standalone too. The -I and source paths match the gcc
# invocations documented in each test file's header comment.
set -euo pipefail

cd "$(dirname "$0")/.."

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

run_one() {
  local name="$1"; shift
  echo "--- $name ---"
  gcc -std=c11 -Wall -I src/c -o "$tmp/$name" "$@"
  "$tmp/$name"
}

run_one t_battery_days       tests/test_battery_days.c       src/c/battery_days_calc.c
run_one t_widget_list        tests/test_widget_list.c        src/c/widget_list.c
run_one t_fallback_place     tests/test_fallback_place.c     src/c/widget_list.c
run_one t_electricity_calc   tests/test_electricity_calc.c   src/c/electricity_calc.c
run_one t_night_window       tests/test_night_window_calc.c  src/c/night_window_calc.c src/c/electricity_calc.c
run_one t_night_theme_calc   tests/test_night_theme_calc.c   src/c/night_theme_calc.c
run_one t_warn_border_calc   tests/test_warn_border_calc.c   src/c/warn_border_calc.c
run_one t_poll_state_calc    tests/test_poll_state_calc.c    src/c/poll_state_calc.c
run_one t_clock_area_calc    tests/test_clock_area_calc.c    src/c/clock_area_calc.c
run_one t_date_header_calc   tests/test_date_header_calc.c   src/c/date_header_calc.c
run_one t_sleep_calc         tests/test_sleep_calc.c         src/c/sleep_calc.c
run_one t_twt_calc           tests/test_twt_calc.c           src/c/twt_calc.c
run_one t_data_slots         tests/test_data_slots.c         src/c/data_slots.c
run_one t_tuya_leds_parse    tests/test_tuya_leds_parse.c    src/c/tuya_leds_parse.c

echo "--- all host C tests passed ---"
