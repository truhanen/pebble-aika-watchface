#!/usr/bin/env bash
# Emulator helper for Nyquist (emery). Every emulator-facing call uses --vnc
# consistently (mixing --vnc and non---vnc restarts QEMU; see the
# pebble-emulator skill).
#
#   scripts/emu.sh build               TEST_HOOKS build (clean if package.json changed)
#   scripts/emu.sh fresh               kill + wipe + install
#   scripts/emu.sh install             install over the running emulator
#   scripts/emu.sh int  Key=V ...      send int AppMessage values, keys by name
#   scripts/emu.sh bytes Key=HEX ...   send byte-array AppMessage values, keys by name
#   scripts/emu.sh string Key=S ...    send string AppMessage values, keys by name
#   scripts/emu.sh shot NAME           screenshot to screenshots/test/NAME.png
#   scripts/emu.sh sweep FROM TO [H]   TestTime=H:MM for MM in FROM..TO, a shot each
#                                      (screenshots/test/sweep/HHMM.png)
#   scripts/emu.sh logs                follow app logs
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
PLATFORM=emery
UUID=$(python3 -c "import json;print(json.load(open('package.json'))['pebble']['uuid'])")
SHOTS="$ROOT/screenshots/test"
mkdir -p "$SHOTS"

key_id() {
  local f
  f=$(find build -name message_keys.auto.c 2>/dev/null | head -1)
  [ -n "$f" ] || { echo "no build/ message keys; run '$0 build' first" >&2; exit 1; }
  local id
  id=$(sed -n "s/^uint32_t MESSAGE_KEY_$1 = \([0-9]*\);/\1/p" "$f")
  [ -n "$id" ] || { echo "unknown message key: $1" >&2; exit 1; }
  echo "$id"
}

send() {
  local type=$1; shift
  local args=()
  for kv in "$@"; do
    args+=("$(key_id "${kv%%=*}")=${kv#*=}")
  done
  pebble send-app-message --emulator "$PLATFORM" --vnc --app-uuid "$UUID" "--$type" "${args[@]}" >/dev/null
}

shot() {
  pebble screenshot --vnc --no-open "$SHOTS/$1.png" >/dev/null 2>&1
  echo "$SHOTS/$1.png"
}

cmd=${1:-}; shift || true
case "$cmd" in
  build)
    # message keys are generated at configure time: reconfigure when package.json changes
    if [ ! -f build/c4che/_cache.py ] || [ build/c4che/_cache.py -ot package.json ]; then rm -rf build; fi
    TEST_HOOKS=1 pebble build 2>&1 | grep -E "error|warning:|finished" | grep -v -E "RWX|format-truncation" || true
    ;;
  fresh)
    pebble kill >/dev/null 2>&1 || true
    pebble wipe >/dev/null 2>&1 || true
    pebble install --emulator "$PLATFORM" --vnc 2>&1 | tail -1
    ;;
  install)
    pebble install --emulator "$PLATFORM" --vnc 2>&1 | tail -1
    ;;
  int|bytes|string)
    send "$cmd" "$@"
    ;;
  shot)
    shot "$1"
    ;;
  sweep)
    from=$1; to=$2; hour=${3:-10}
    mkdir -p "$SHOTS/sweep"
    for ((m = from; m <= to; m++)); do
      hhmm=$(printf "%02d%02d" "$hour" "$m")
      send int "TestTime=$((10#$hhmm))"
      shot "sweep/$hhmm" >/dev/null
    done
    echo "$SHOTS/sweep"
    ;;
  logs)
    pebble logs --emulator "$PLATFORM" --vnc
    ;;
  *)
    sed -n '2,17p' "$0"; exit 1
    ;;
esac
