#!/usr/bin/env bash
# Regenerates the README screenshots (screenshots/readme/NN-name.png, native
# 200x228, unframed) and the animated hero GIF (screenshots/readme/hero.gif) on the
# emery emulator. The output is version-controlled; run it again after a visual change.
#
#   scripts/screenshots.sh            rebuild, run every scene, assemble the GIF
#   scripts/screenshots.sh --no-gif   stills only
#   scripts/screenshots.sh scene...   only the named scenes (no GIF), e.g. "night"
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
#   int Key=V ...         any integer AppMessage key (emu.sh int), e.g. WeatherTemperature
#   left|right|center LIST   widget list of a column, space separated slots:
#                           ID[/FONT][:i|u|n]   i = icon, u = unit, n = no identifier;
#                           ID is a number or an alias (see ALIASES in the python helper)
#                           FONT 0..16 (docs/widget_contract.md), default = position's
#                           rG<code>=SLOT,SLOT,...  rotating group, <code> 0..5 =
#                           5/10/30/60/120/300 s (the first member is shown)
#   rain A B C D E        max rain next 1/3/6/9/12 h in 1/100 mm/h (65535 = unknown)
#   elec N                flat electricity price, 1/100 c/kWh
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

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
OUT="$ROOT/screenshots/readme"
EMU="$ROOT/scripts/emu.sh"
SHOTS="$ROOT/screenshots/test"
GIF_DELAY=200   # centiseconds per hero frame

want_gif=1
only=()
for a in "$@"; do
  case "$a" in
    --no-gif) want_gif=0 ;;
    -h|--help) sed -n '2,10p' "$0"; exit 0 ;;
    *) only+=("$a"); want_gif=0 ;;
  esac
done

die() { echo "screenshots.sh: $*" >&2; exit 1; }
for tool in pebble magick python3; do
  command -v "$tool" >/dev/null || die "missing tool: $tool"
done

cleanup() {
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
out = []
for slot in sys.argv[1:]:
  if slot.startswith('rG'):
    code, mems = slot[2:].split('=')
    ms = [member(t) for t in mems.split(',')]
    out += [255, len(ms), int(code)] + [b for m in ms for b in m]
  else:
    out += member(slot)
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

selected=()
if [ ${#only[@]} -gt 0 ]; then
  for o in "${only[@]}"; do
    found=0
    for n in "${names[@]}"; do [ "$n" = "$o" ] && found=1; done
    [ $found = 1 ] || die "unknown scene: $o (have: ${names[*]})"
    selected+=("$o")
  done
else
  selected=("${names[@]}")
fi

# --- build + emulator -----------------------------------------------------------------
echo "Building TEST_HOOKS build (ROTATION_FAKE_SECONDS=0)..."
rm -rf build   # defines are not part of waf's change detection; also regenerates message keys
ROTATION_FAKE_SECONDS=0 "$EMU" build
[ -f build/emery/pebble-app.elf ] || die "TEST_HOOKS build failed"
echo "Fresh emulator..."
"$EMU" fresh
sleep 4   # the emulator needs a moment after install before it takes AppMessages

mkdir -p "$OUT" "$SHOTS"
if [ ${#only[@]} -eq 0 ]; then rm -f "$OUT"/[0-9][0-9]-*.png "$OUT"/hero*.gif; fi

# Index = position in the full scene list, so a single-scene run keeps its number.
idx=0
for n in "${names[@]}"; do
  idx=$((idx + 1))
  want=0; for s in "${selected[@]}"; do [ "$s" = "$n" ] && want=1; done
  [ $want = 1 ] || continue
  echo "Scene $(printf '%02d' $idx) $n"
  CAPTION=""
  while IFS= read -r l; do apply_line "$l"; done <"$WORK/defaults"
  while IFS= read -r l; do apply_line "$l"; done <"$WORK/scene.$n"
  capture "$OUT/$(printf '%02d' $idx)-$n.png"
done

# --- hero GIF -------------------------------------------------------------------------
if [ $want_gif = 1 ]; then
  echo "Assembling hero.gif..."
  # One palette over all frames: the watch is 8-bit colour, so this is lossless.
  # (`magick` global palette via -remap of a combined palette image.)
  frames=("$OUT"/[0-9][0-9]-*.png)
  magick "${frames[@]}" +append -colors 256 -unique-colors "$WORK/palette.png"
  magick -delay "$GIF_DELAY" -loop 0 "${frames[@]}" -dither None -remap "$WORK/palette.png" \
    -layers OptimizePlus "$OUT/hero.gif"
  # 2x nearest-neighbour version: crisp when the README shows it larger than 200 px.
  magick -delay "$GIF_DELAY" -loop 0 "${frames[@]}" -filter point -resize 200% -dither None \
    -remap "$WORK/palette.png" -layers OptimizePlus "$OUT/hero@2x.gif"
  ls -l "$OUT"/hero*.gif | awk '{print $5, $9}'
fi

# --- leave a plain release build ------------------------------------------------------
pebble kill >/dev/null 2>&1 || true
rm -rf build
pebble build 2>&1 | grep -E "error|finished" | grep -v -E "RWX|format-truncation" || true
echo "Done: $OUT (build/ is a plain release build; emulator stopped)"
