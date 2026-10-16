#!/usr/bin/env bash
# Regenerates the README / appstore screenshots on the emery emulator: the animated hero
# GIF (screenshots/readme/hero.gif, hero@2x.gif) and the stills, which are frames of it
# copied under descriptive names (STILLS below; native 200x228, unframed). The output is
# version-controlled; run it again after a visual change.
#
#   scripts/screenshots.sh            rebuild, render the GIF frames, assemble the GIF and stills
#   scripts/screenshots.sh scene...   only the named scenes (SCENES below; no GIF) to
#                                     screenshots/test/scenes/, e.g. "default"
#   scripts/screenshots.sh --keep scene...   same on an already running TEST_HOOKS emulator:
#                                     no build, no fresh start, nothing stopped (tuning)
# GIF frames are saved to screenshots/gif_frames/ (gitignored). --gif-only (make hero_gif) is
# the same as no arguments.
#
# Needs: pebble-tool, ImageMagick (magick), python3.
# The screenshots come from a TEST_HOOKS build (fixed time/date/battery/health data over
# AppMessage) with ROTATION_FAKE_SECONDS=0 (rotating groups show their first member).
# The script rebuilds a plain `pebble build` at the end, so build/ is a release build.
#
# ---------------------------------------------------------------------------------
# Scene definitions
# ---------------------------------------------------------------------------------
# Every scene = the DEFAULTS lines, then the scene's own lines (later wins). Lines:
#   caption TEXT          one-line description (keep README.md's gallery captions in sync)
#   time HHMM             fixed clock time (TestTime); hands at 10:10 cover nothing
#   date YYYYMMDD         fixed calendar date (TestDate)
#   battery N             battery percent (1000+N = charging)
#   steps N | hr N        step count / heart rate
#   sleep TOTAL DEEP      minutes slept / minutes of it restful
#   int Key=V ...         any integer AppMessage key (emu.sh int), e.g. WeatherTemperature
#   left|right|center LIST   widget list of a column, space separated slots:
#                           ID[/FONT][:i|u|n]   i = icon, u = unit, n = no identifier;
#                           ID is a number or an alias (see ALIASES in the python helper)
#                           FONT 0..16 (docs/widget_contract.md), default = the position's font
#                           rG<code>=SLOT,SLOT,...  rotating group, <code> 0..5 =
#                           5/10/30/60/120/300 s (the first member is shown)
#                           `A B | C D` = group A B at the top, C D at the bottom, `empty`
#                           spacers added automatically (autopad in the python helper)
#   rain A B C D E        max rain next 1/3/6/9/12 h in 1/100 mm/h (65535 = unknown)
#   elec N                flat electricity price, 1/100 c/kWh
#   crypto WID,LABEL,VALUE ...      CryptoData slots (wid 200..215), e.g. 200,BTC/USD,62
#   currency WID,LABEL,VALUE ...    CurrencyData slots (wid 216..222), e.g. 216,EUR/USD,1.08
#   tuyaleds STATES       TuyaLeds string, one char per switch: 1 on, 0 off, ? unknown
#   colors 0xFG 0xBG      time colour and background colour (SettingColorTime/BG)
#   delay CS              hero GIF frames only: frame delay in 1/100 s
#   night                 night window active now (custom window around the real hour)
SCENES=$(cat <<'EOF'
defaults
time 1010
date 20260115
battery 63
steps 4321
hr 72
int SettingUseMetric=1 SettingPrecipUnit=0 SettingShowLeadingZero=0 SettingDisableAutobattery=1
int SettingDisconnectIcon=0 SettingLanguageID=0 SettingDecimalSep=46
int SettingColorTime=0 SettingColorBG=16777215
int SettingNightRotationMode=0 SettingNightColors=0 SettingNightSlowRotation=0
int SettingBatteryWarnPct=0 SettingBatteryWarnDays=0 SettingBtWarnBorder=0
int WeatherTemperature=19 WeatherCondition=0 WeatherForecastCondition=2 WeatherForecastHighTemp=18 WeatherForecastLowTemp=11 WeatherUVIndex=3
left
right
center
rain 0 0 0 0 0
elec 630

scene default
caption Default layout: weather and battery on the left, steps and heart rate on the right
left weather bat
right steps hr

scene three-columns
caption All three columns: widgets on both edges plus a centre column
left weather date_wkday_dd
right bat uv
center date_iso/2:n

scene mixed-fonts
caption Mixed fonts: small edge fonts with a large centre value
left weather/1 steps/1
right bat/1 hr/1
center steps/16:n
int WeatherTemperature=21

scene unit-vs-icon
caption Identifier modes: icon (top), unit (middle) and none (bottom) for the same widgets
left weather:i weather:u weather:n
right steps:i steps:u steps:n

scene weather
caption Weather: max rain in the next 3 h (mm/h), electricity price (c/kWh) and forecast low/high
left rain3:i rain3:u forecast
right elec:u uv
rain 40 140 140 140 140
elec 630
int WeatherCondition=5 WeatherTemperature=14 WeatherForecastCondition=5 WeatherForecastHighTemp=18 WeatherForecastLowTemp=11

scene dark-theme
caption Dark theme with custom colours
int SettingColorTime=5570390 SettingColorBG=0
left weather steps
right bat hr

scene light-theme
caption Light theme with a navy clock on cream
int SettingColorTime=85 SettingColorBG=16777130
left weather steps
right bat hr

scene night
caption Night mode: the night palette applies in the configured night window
int SettingNightRotationMode=2 SettingNightColors=1 SettingNightBgColor=0 SettingNightFgColor=16733525
time 2310
left weather steps
right bat hr
night

scene battery-warning
caption Battery warning: a coloured border when the charge drops below the threshold
battery 12
int SettingBatteryWarnPct=20 SettingBatteryWarnColor=16711680
left weather steps
right bat hr
EOF
)

# ---------------------------------------------------------------------------------
# Hero GIF: one layout built up frame by frame (independent of the scenes above), at a
# constant 2 fps (the last frame is held for 2 s). One GIF second = one watch minute:
# every frame advances the fake clock (TestTime/TestSeconds) by its own delay x 60. The
# first frame applies the defaults above and the gifdefaults; later frames send only what
# they list; gifeach lines are re-sent in every frame (the emulator's phone side overwrites
# the weather on its own). `delay CS` is the frame delay in 1/100 s. In a widget list
# `A B | C D` stacks A B at the top and C D at the bottom (spacers are added
# automatically, see autopad); bottom groups are listed top to bottom, so the corner
# widget is the last one. `colors 0xFG 0xBG` sets the time and background colours.
# The "bigger" frame is all Leco, one step up: S Gothic (3) -> M Leco (6), M Gothic (5) ->
# L Leco (9), L Bitham (7) -> XL Leco (10); the date, weekday, steps and heart rate take
# M Leco (6) (the date then doesn't hit the corner icons). Weekday and seconds are M Gothic (5)
# in the normal frames and S Gothic (3) in the smaller frame; seconds are L Leco (9) in the
# Leco frames. So they never touch the digital time.
# The last (2 s) frame repeats the Leco fonts.
# README / appstore stills: "<GIF frame name> <still name>", copied to
# screenshots/readme/NN-<still name>.png, numbered in this order.
STILLS="corners little-widgets
grow3 many-widgets
bigger leco
color3 night"

GIFFRAMES=$(cat <<'EOF'
gifdefaults
crypto 200,BTC/USD,62
currency 216,EUR/USD,1.08
gifeach
rain 0 0 0 0 40
int WeatherTemperature=19 WeatherCondition=0 WeatherForecastCondition=2 WeatherForecastHighTemp=18 WeatherForecastLowTemp=11
frame empty
delay 50
left 
right 
center 

frame time
delay 50
left 
right 
center date_iso/5 digital/7

frame corners
delay 50
left weather_icon | date_wkday/5
right bat_icon | seconds/5
center date_iso/5 digital/7

frame grow2
delay 50
left weather_icon weather/7:u | hr/5:i date_wkday/5
right bat_icon bat/5:u | 200/3:u seconds/5
center date_iso/5 digital/7

frame grow3
delay 50
left weather_icon weather/7:u forecast/3:u | steps/5:i hr/5:i date_wkday/5
right bat_icon bat/5:u battery_days/5:u | 216/3:u 200/3:u seconds/5
center date_iso/5 digital/7

frame full
delay 50
left weather_icon weather/7:u forecast/3:u rain12/3:u | steps/5:i hr/5:i date_wkday/5
right bat_icon bat/5:u battery_days/5:u | elec/3:u 216/3:u 200/3:u seconds/5
center date_iso/5 digital/7

frame bigger
delay 50
left weather_icon weather/10:u forecast/6:u rain12/6:u | steps/6:i hr/6:i date_wkday/6
right bat_icon bat/9:u battery_days/9:u | elec/6:u 216/6:u 200/6:u seconds/9
center date_iso/6 digital/10

frame bigger-back
delay 50
left weather_icon weather/7:u forecast/3:u rain12/3:u | steps/5:i hr/5:i date_wkday/5
right bat_icon bat/5:u battery_days/5:u | elec/3:u 216/3:u 200/3:u seconds/5
center date_iso/5 digital/7

frame smaller
delay 50
left weather_icon weather/5:u forecast/2:u rain12/2:u | steps/3:i hr/3:i date_wkday/3
right bat_icon bat/3:u battery_days/3:u | elec/2:u 216/2:u 200/2:u seconds/3
center date_iso/3 digital/5

frame smaller-back
delay 50
left weather_icon weather/7:u forecast/3:u rain12/3:u | steps/5:i hr/5:i date_wkday/5
right bat_icon bat/5:u battery_days/5:u | elec/3:u 216/3:u 200/3:u seconds/5
center date_iso/5 digital/7

frame no-units
delay 50
left weather_icon weather/7:n forecast/3:n rain12/3:n | steps/5:i hr/5:i date_wkday/5
right bat_icon bat/5:n battery_days/5:n | elec/3:n 216/3:n 200/3:n seconds/5
center date_iso/5 digital/7

frame units
delay 50
left weather_icon weather/7:u forecast/3:u rain12/3:u | steps/5:i hr/5:i date_wkday/5
right bat_icon bat/5:u battery_days/5:u | elec/3:u 216/3:u 200/3:u seconds/5
center date_iso/5 digital/7

frame color1
delay 50
left weather_icon weather/7:u forecast/3:u rain12/3:u | steps/5:i hr/5:i date_wkday/5
right bat_icon bat/5:u battery_days/5:u | elec/3:u 216/3:u 200/3:u seconds/5
center date_iso/5 digital/7
colors 0x000055 0xFFFFAA

frame color2
delay 50
left weather_icon weather/7:u forecast/3:u rain12/3:u | steps/5:i hr/5:i date_wkday/5
right bat_icon bat/5:u battery_days/5:u | elec/3:u 216/3:u 200/3:u seconds/5
center date_iso/5 digital/7
colors 0x55FF55 0x000000

frame color3
delay 50
left weather_icon weather/7:u forecast/3:u rain12/3:u | steps/5:i hr/5:i date_wkday/5
right bat_icon bat/5:u battery_days/5:u | elec/3:u 216/3:u 200/3:u seconds/5
center date_iso/5 digital/7
colors 0xFFAA00 0x000055

frame color4
delay 200
left weather_icon weather/10:u forecast/6:u rain12/6:u | steps/6:i hr/6:i date_wkday/6
right bat_icon bat/9:u battery_days/9:u | elec/6:u 216/6:u 200/6:u seconds/9
center date_iso/6 digital/10
colors 0xFFFFFF 0xAA0000
EOF
)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
OUT="$ROOT/screenshots/readme"
EMU="$ROOT/scripts/emu.sh"
SHOTS="$ROOT/screenshots/test"
GIF_DELAY=200   # centiseconds per hero frame

want_gif=1
keep=0
only=()
for a in "$@"; do
  case "$a" in
    --gif-only) ;;
    --keep) keep=1 ;;
    -h|--help) sed -n '2,13p' "$0"; exit 0 ;;
    *) only+=("$a"); want_gif=0 ;;
  esac
done

die() { echo "screenshots.sh: $*" >&2; exit 1; }
for tool in pebble magick python3; do
  command -v "$tool" >/dev/null || die "missing tool: $tool"
done

cleanup() {
  [ $keep = 1 ] && return 0
  pebble kill >/dev/null 2>&1 || true
}
trap cleanup EXIT
trap 'die "failed at line $LINENO (see output above)"' ERR

# Widget list text -> hex payload (docs/widget_contract.md, same bytes as
# src/ts/widget_list_payload.ts produces).
list_hex() {
  python3 -I - "$@" <<'PY'
import sys
ALIASES = {
  'empty': 0, 'battery': 2, 'bat': 2, 'altclock': 3, 'seconds': 5, 'week': 6,
  'weather': 7, 'forecast': 8, 'sleep': 9, 'steps': 10, 'beats': 11, 'hr': 12,
  'uv': 13, 'elec': 14, 'cheap_next': 18, 'cheapest': 19, 'distance': 21,
  'battery_days': 22, 'date_d_m': 26, 'date_dd_mm': 27, 'date_wkday_dd': 30,
  'date_wkday_d_m': 31, 'date_wkday': 32, 'date_dd_mon': 33, 'date_iso': 34,
  'digital': 35, 'bat_icon': 36, 'weather_icon': 37, 'bt_icon': 38,
  'tuya_leds': 23, 'sleep_bar': 25, 'deep': 20, 'sleep_deep': 24,
  'rain1': 40, 'rain3': 41, 'rain6': 42, 'rain9': 43, 'rain12': 44,
}
def member(tok):
  ident = 0
  if ':' in tok:
    tok, m = tok.split(':')
    ident = {'i': 0, 'u': 0x40, 'n': 0x20}[m]
  font = 31
  if '/' in tok:
    tok, f = tok.split('/')
    font = int(f)
  wid = ALIASES[tok] if tok in ALIASES else int(tok)
  return [wid, font | ident]
# --- autopad: "A B | C D" = group A B at the top, group C D at the bottom, `empty` spacers
# in between so the slack is ~0 (min gap 5 px). A Python port of the watch's slot heights
# (src/c/widget_layout_calc.c: wl_item_height); the table below knows the widgets used.
DIGIT_H = [9, 9, 11, 14, 15, 18, 14, 21, 20, 18, 22, 30, 25, 27, 29, 42, 42]   # widget_font_metrics.h
CAP_H = [9, 9, 11, 14, 15, 18, 14, 21, 20, 18, 23, 29, 25, 27, 29, 42, 42]
X_TOP = [7, 7, 10, 13, 10, 14, 6, 14, 13, 8, 9, 19, 11, 11, 13, 18, 18]
CAP_TOP = [5, 5, 7, 10, 6, 10, 6, 9, 8, 8, 9, 13, 11, 11, 13, 18, 18]
BASE = [14, 14, 18, 24, 21, 28, 20, 30, 28, 26, 32, 42, 36, 38, 42, 60, 60]
DIGIT_TOP = [5, 5, 7, 10, 6, 10, 6, 9, 8, 8, 10, 12, 11, 11, 13, 18, 18]
DESC_BOTTOM = [16, 16, 21, 28, 25, 32, 20, 35, 35, 26, 32, 49, 36, 38, 42, 60, 60]
ICON_H = 23
# id: (has value, graphical icon, unit label or None)
INFO = {0: (0, 0, None), 2: (1, 1, '%'), 3: (1, 0, None), 5: (1, 0, None), 6: (1, 0, 'Wk'),
        7: (1, 1, '\u00b0C'), 8: (1, 1, '\u00b0C'), 9: (1, 1, 'h'), 10: (1, 1, 'steps'),
        11: (1, 0, None), 12: (1, 1, 'bpm'), 13: (1, 0, 'UVI'), 14: (1, 0, 'c/kWh'),
        18: (1, 0, 'c/kWh'), 19: (1, 0, 'c/kWh'), 20: (1, 1, 'h'), 21: (1, 1, 'km'),
        22: (1, 1, 'd'), 23: (0, 1, None), 24: (1, 1, 'h'), 25: (1, 1, 'h'),
        35: (1, 0, None), 36: (0, 1, None), 37: (0, 1, None), 38: (0, 1, None),
        40: (1, 1, 'mm/h'), 41: (1, 1, 'hi mm/3 h'), 42: (1, 1, 'hi mm/6 h'),
        43: (1, 1, 'hi mm/9 h'), 44: (1, 1, 'hi mm/12 h')}
for w in range(26, 35): INFO[w] = (1, 0, None)
for w in range(200, 216): INFO[w] = (1, 0, 'BTC/USD')   # crypto / currency: a fraction label
for w in range(216, 223): INFO[w] = (1, 0, 'EUR/USD')
def center_offset(outer, inner):
  d = outer - inner
  return d // 2 if d >= 0 else -((-d + 1) // 2)
def height(wid, font, ident):
  if wid not in INFO: sys.exit('autopad: no height model for widget %d' % wid)
  value, gfx, unit = INFO[wid]
  if font == 31: font = 5
  ink = DIGIT_H[font] if (value or wid == 0) else 0
  # identifier: unit -> icon -> none, or icon -> unit -> none (docs/widget_contract.md)
  order = {0: ['icon', 'unit'], 0x40: ['unit', 'icon'], 0x20: []}[ident]
  mode = next((m for m in order if (m == 'icon' and gfx) or (m == 'unit' and unit)), None)
  if mode is None:
    return ink
  if mode == 'icon':
    ih, inline, udesc = ICON_H, False, False
  elif '/' in unit and not unit.startswith('/') and not unit.endswith('/'):
    num = unit.split('/')[0]
    short = all(c in 'acemnorsuvwxzgpqy .,-_=+:;~' for c in num)
    trim = max(0, X_TOP[0] - CAP_TOP[0]) if short else 0
    ih, inline, udesc = (CAP_H[0] - trim + 1) + 1 + 1 + CAP_H[0], False, False   # wl_fraction_layout
  else:
    ih, inline, udesc = DIGIT_H[0], True, any(c in 'gjpqy,;()' for c in unit)
  if not ink:
    return ih
  if inline:
    it = (-DIGIT_TOP[font] + BASE[font] - BASE[0] + DIGIT_TOP[0])
  else:
    it = center_offset(ink, ih)
  top, bottom = min(0, it), max(ink, it + ih)
  if inline and udesc:
    bottom = max(bottom, it - DIGIT_TOP[0] + DESC_BOTTOM[0])
  return bottom - top
INNER, GAP = 214, 5
def autopad(top, bottom):
  heights = [height(m[0], m[1] & 31, m[1] & 0x60) for m in top + bottom]
  m = len(heights)
  best = None
  from itertools import combinations_with_replacement as cwr
  for k in range(1, 7):
    for combo in cwr(range(15), k):   # fonts 0..14 and 16 (15 duplicates 16)
      combo = tuple(16 if f == 15 else f for f in combo)
      total = sum(heights) + sum(DIGIT_H[f] for f in combo) + GAP * (m + k - 1)
      slack = INNER - total
      if slack >= 0 and (best is None or (slack, k) < best[:2]):
        best = (slack, k, combo)
    if best is not None and best[0] == 0: break
  if best is None: sys.exit('autopad: the group does not fit')
  return [[0, f] for f in best[2]]
groups, cur = [[]], 0
out = []
def emit(mems):
  global out
  for mm in mems: out += mm
for slot in sys.argv[1:]:
  if slot == '|':
    groups.append([]); continue
  if slot.startswith('rG'):
    code, mems = slot[2:].split('=')
    ms = [member(t) for t in mems.split(',')]
    groups[-1].append([255, len(ms), int(code)] + [b for m in ms for b in m])
  else:
    groups[-1].append(member(slot))
if len(groups) == 1:
  for g in groups[0]: out += g
else:
  top, bottom = groups[0], groups[1]
  if any(len(x) != 2 for x in top + bottom): sys.exit('autopad: no rotating groups')
  for g in top: out += g
  if top or bottom: emit(autopad(top, bottom))
  for g in bottom: out += g
print(bytes(out).hex())
PY
}

# Little-endian uint16 values -> hex (WeatherRainMax / ElecPrices).
le16_hex() {
  python3 -I -c 'import sys;print("".join(int(v).to_bytes(2,"little").hex() for v in sys.argv[1:]))' "$@"
}

send_int()   { "$EMU" int "$@"; }
send_bytes() { "$EMU" bytes "$@"; }

# Apply one scene line (see the syntax at the top).
apply_line() {
  local kw=${1%% *} rest=""
  [ "$1" = "$kw" ] || rest=${1#* }
  case "$kw" in
    caption) CAPTION=$rest ;;
    time)    send_int TestTime=$((10#$rest)) ;;
    date)    send_int TestDate=$((10#$rest)) ;;
    battery) send_int TestBattery="$rest" ;;
    steps)   send_int TestSteps="$rest" ;;
    hr)      send_int TestHeartRate="$rest" ;;
    delay)   FRAME_DELAY=$rest ;;
    colors)  send_int SettingColorTime=$((${rest% *})) SettingColorBG=$((${rest#* })) ;;
    sleep)   send_int TestSleepMin="${rest% *}" TestDeepSleepMin="${rest#* }" ;;
    crypto|currency)
             # "WID,LABEL,VALUE ..." -> the phone's wid<US>label<US>value<US>... string
             local key=CryptoData packed="" t
             [ "$kw" = currency ] && key=CurrencyData
             for t in $rest; do packed+="${packed:+$'\x1f'}${t//,/$'\x1f'}"; done
             "$EMU" string "$key=$packed" ;;
    tuyaleds) "$EMU" string "TuyaLeds=$rest" ;;
    int)     # shellcheck disable=SC2086
             send_int $rest ;;
    left|right|center)
      local key
      case "$kw" in
        left) key=SettingWidgetList ;; right) key=SettingRightWidgetList ;;
        center) key=SettingCenterWidgetList ;;
      esac
      # An empty list needs a real (empty) value; one Empty slot would reserve height.
      if [ -z "$rest" ]; then send_bytes "$key="; else
        # shellcheck disable=SC2086
        send_bytes "$key=$(list_hex $rest)"
      fi ;;
    rain)
      # shellcheck disable=SC2086
      send_bytes "WeatherRainMax=$(le16_hex $rest)" ;;
    elec)
      # 96 flat quarters starting 8 h before now (the price is read for the current time)
      local now start vals=()
      now=$(date +%s); start=$((now - now % 900 - 8 * 3600))
      for ((i = 0; i < 96; i++)); do vals+=("$rest"); done
      send_int ElecStartEpoch="$start"
      send_bytes "ElecPrices=$(le16_hex "${vals[@]}")" ;;
    night)
      # The night window follows the watch's REAL hour (not TestTime): a custom window
      # of [hour, hour+2) is active right now whenever the screenshot is taken.
      local h; h=$(date +%-H)
      send_int SettingNightRotationStart="$h" SettingNightRotationEnd=$(((h + 2) % 24)) ;;
    *) die "unknown scene line: $1" ;;
  esac
}

# Screenshot once the frame has settled: wait for two identical consecutive captures.
# (AppMessage handling and the redraw are asynchronous; a fixed sleep alone is flaky.)
capture() {
  local dest=$1 i
  sleep 2
  "$EMU" shot _a >/dev/null
  for i in 1 2 3 4 5 6; do
    sleep 1
    "$EMU" shot _b >/dev/null
    if cmp -s "$SHOTS/_a.png" "$SHOTS/_b.png"; then
      cp "$SHOTS/_b.png" "$dest"
      return 0
    fi
    mv "$SHOTS/_b.png" "$SHOTS/_a.png"
  done
  die "screen never settled for $dest"
}

# --- parse the scene data into one command file per scene -------------------------
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"; cleanup' EXIT
names=()
cur=""
while IFS= read -r line; do
  [ -n "$line" ] || continue
  case "$line" in
    defaults) cur="$WORK/defaults" ;;
    scene\ *) names+=("${line#scene }"); cur="$WORK/scene.${line#scene }" ;;
    *) [ -n "$cur" ] || die "scene data: line before any scene"; printf '%s\n' "$line" >>"$cur" ;;
  esac
done <<<"$SCENES"

fnames=()
cur=""
while IFS= read -r line; do
  [ -n "$line" ] || continue
  case "$line" in
    gifdefaults) cur="$WORK/gifdefaults" ;;
    gifeach) cur="$WORK/gifeach" ;;
    frame\ *) fnames+=("${line#frame }"); cur="$WORK/frame.${line#frame }" ;;
    *) [ -n "$cur" ] || die "frame data: line before any frame"; printf '%s\n' "$line" >>"$cur" ;;
  esac
done <<<"$GIFFRAMES"

selected=()
if [ ${#only[@]} -gt 0 ]; then
  for o in "${only[@]}"; do
    found=0
    for n in "${names[@]}"; do [ "$n" = "$o" ] && found=1; done
    [ $found = 1 ] || die "unknown scene: $o (have: ${names[*]})"
    selected+=("$o")
  done
fi

# --- build + emulator -----------------------------------------------------------------
if [ $keep = 0 ]; then
echo "Building TEST_HOOKS build (ROTATION_FAKE_SECONDS=0)..."
rm -rf build   # defines are not part of waf's change detection; also regenerates message keys
ROTATION_FAKE_SECONDS=0 "$EMU" build
[ -f build/emery/pebble-app.elf ] || die "TEST_HOOKS build failed"
echo "Fresh emulator..."
"$EMU" fresh
sleep 4   # the emulator needs a moment after install before it takes AppMessages
fi

SCENEDIR="$SHOTS/scenes"
mkdir -p "$OUT" "$SCENEDIR"

# Index = position in the full scene list, so a single-scene run keeps its number.
idx=0
for n in "${names[@]}"; do
  idx=$((idx + 1))
  want=0; for s in ${selected[@]+"${selected[@]}"}; do [ "$s" = "$n" ] && want=1; done
  [ $want = 1 ] || continue
  echo "Scene $(printf '%02d' $idx) $n"
  CAPTION=""
  while IFS= read -r l; do apply_line "$l"; done <"$WORK/defaults"
  while IFS= read -r l; do apply_line "$l"; done <"$WORK/scene.$n"
  capture "$SCENEDIR/$(printf '%02d' $idx)-$n.png"
done

# --- hero GIF -------------------------------------------------------------------------
# Frames: GIFFRAMES. The fake clock starts at 09:00:00 and runs one watch minute per GIF
# second (frame i shows 09:00:00 + (sum of the earlier delays) x 60 s).
if [ $want_gif = 1 ]; then
  GIFDIR="$ROOT/screenshots/gif_frames"; rm -rf "$GIFDIR"; mkdir -p "$GIFDIR"   # gitignored
  elapsed=0; fi=0; gifargs=(); gifargs2=()
  for fn in "${fnames[@]}"; do
    fi=$((fi + 1))
    echo "Frame $(printf '%02d' $fi) $fn"
    secs=$((elapsed * 60 / 100)); t=$((9 * 3600 + secs))
    FRAME_DELAY=50
    if [ $fi = 1 ]; then
      while IFS= read -r l; do apply_line "$l"; done <"$WORK/defaults"
      while IFS= read -r l; do apply_line "$l"; done <"$WORK/gifdefaults"
    fi
    # (two test keys in one message make the pebble tool wait for an ack that never comes)
    send_int TestTime=$(( (t / 3600 % 24) * 100 + t / 60 % 60 ))
    send_int TestSeconds=$((t % 60))
    # the emulator's phone side refreshes the weather by itself: pin it in every frame
    while IFS= read -r l; do apply_line "$l"; done <"$WORK/gifeach"
    while IFS= read -r l; do apply_line "$l"; done <"$WORK/frame.$fn"
    while IFS= read -r l; do apply_line "$l"; done <"$WORK/gifeach"   # again: the phone side may have refreshed meanwhile
    f="$GIFDIR/$(printf '%02d' $fi)-$fn.png"
    capture "$f"
    gifargs+=(-delay "$FRAME_DELAY" "$f")
    elapsed=$((elapsed + FRAME_DELAY))
  done
  echo "Assembling hero.gif ($fi frames, $((elapsed / 100)).$((elapsed % 100)) s)..."
  # One palette over all frames: the watch is 8-bit colour, so this is lossless.
  magick "$GIFDIR"/[0-9][0-9]-*.png +append -colors 256 -unique-colors "$WORK/palette.png"
  magick -loop 0 "${gifargs[@]}" -dither None -remap "$WORK/palette.png" \
    -layers OptimizePlus "$OUT/hero.gif"
  # 2x nearest-neighbour version: crisp when the README shows it larger than 200 px.
  magick -loop 0 "${gifargs[@]}" -filter point -resize 200% -dither None \
    -remap "$WORK/palette.png" -layers OptimizePlus "$OUT/hero@2x.gif"
  ls -l "$OUT"/hero*.gif | awk '{print $5, $9}'
  # The stills: chosen GIF frames under descriptive names, numbered in STILLS order.
  rm -f "$OUT"/[0-9][0-9]-*.png
  si=0
  while read -r frame still; do
    [ -n "$frame" ] || continue
    src=$(ls "$GIFDIR"/[0-9][0-9]-"$frame".png 2>/dev/null | head -1)
    [ -n "$src" ] || die "STILLS: no GIF frame named $frame"
    si=$((si + 1)); dst="$(printf '%02d' $si)-$still.png"
    cp "$src" "$OUT/$dst"
    echo "Still $dst <- $(basename "$src")"
  done <<<"$STILLS"
fi

# --- leave a plain release build ------------------------------------------------------
if [ $keep = 1 ]; then echo "Done (emulator and build left as they are)"; exit 0; fi
pebble kill >/dev/null 2>&1 || true
rm -rf build
pebble build 2>&1 | grep -E "error|finished" | grep -v -E "RWX|format-truncation" || true
echo "Done: $OUT (build/ is a plain release build; emulator stopped)"
