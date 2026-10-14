# Developer shortcuts. Emulator targets use --vnc consistently (mixing --vnc and
# non---vnc commands restarts QEMU); the scripted ones go through scripts/emu.sh.

PLATFORM = emery

# A fixed scene for demos and screenshots: 10:10, battery 63 %, 4321 steps,
# heart rate 72, 19 C weather. Needs a TEST_HOOKS build (make build_test_hooks).
EMULATOR_SCENE_INT = \
	TestTime=1010 \
	TestBattery=63 \
	TestSteps=4321 \
	TestHeartRate=72 \
	WeatherTemperature=19 \
	WeatherCondition=0

.PHONY: build
build:
	pebble build

# Build with the emulator test-hook AppMessages (src/c/test_hooks.c).
.PHONY: build_test_hooks
build_test_hooks:
	scripts/emu.sh build

.PHONY: test
test:
	npm test

.PHONY: install_cloudpebble
install_cloudpebble:
	pebble install --cloudpebble

.PHONY: install_emulator
install_emulator:
	pebble install --emulator=$(PLATFORM) --vnc

.PHONY: build_and_install_emulator
build_and_install_emulator: build install_emulator

.PHONY: build_and_install_cloudpebble
build_and_install_cloudpebble: build install_cloudpebble

# Fresh emulator state (kill + wipe + install) with the test hooks enabled.
.PHONY: fresh_emulator
fresh_emulator: build_test_hooks
	scripts/emu.sh fresh

.PHONY: kill_emulator
kill_emulator:
	-pebble kill
	pebble wipe

.PHONY: logs_emulator
logs_emulator:
	pebble logs --emulator=$(PLATFORM) --vnc

.PHONY: logs_cloudpebble
logs_cloudpebble:
	pebble logs --cloudpebble

.PHONY: send_emulator_scene
send_emulator_scene:
	scripts/emu.sh int $(EMULATOR_SCENE_INT)

# Screenshot every minute 10:00-10:59 to screenshots/test/sweep/ and check the
# edge strip of each against the expected tick pattern.
.PHONY: create_hand_position_screenshots
create_hand_position_screenshots:
	scripts/emu.sh sweep 0 59 10
	for f in screenshots/test/sweep/10*.png; do \
		printf '%s ' "$$(basename $$f)"; python3 scripts/check_strip.py "$$f" | tail -1; \
	done

# Regenerate src/c/widget_font_metrics.h from emulator font specimens (~10 min).
.PHONY: measure_fonts
measure_fonts:
	python3 scripts/measure_fonts.py

# Usage: make bump_version VERSION=n.n.n
.PHONY: bump_version
bump_version:
	scripts/bump_version.sh $(VERSION)
