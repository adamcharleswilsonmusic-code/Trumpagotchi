#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
sh tests/pet/run_tests.sh
sh tests/pet/audio_tests.sh
sh tests/pet/voice_tests.sh
sh host/build_host.sh
mkdir -p build/pet/preview
build/host/omni_host tests/pet/integration.omni build/pet/preview
build/host/omni_host tests/pet/ui_input.omni build/pet/preview
build/host/omni_host tests/pet/care_animation.omni build/pet/preview
build/host/omni_host tests/pet/bongo_gap.omni build/pet/preview
build/host/omni_host tests/pet/news_music.omni build/pet/preview
mkdir -p build/bedtime build/snack-preview
build/host/omni_host tests/pet/bedtime.omni build/bedtime
build/host/omni_host tests/pet/snack_ui.omni build/snack-preview
cc -O2 -ffp-contract=off -std=c99 -DOM_HOST -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen tests/pet/praise_response.c firmware/src/dsp/omni.c -lm -o build/pet/praise_response
build/pet/praise_response
cc -O2 -ffp-contract=off -std=c99 -DOM_HOST -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen tests/pet/news_ui.c firmware/src/dsp/omni.c -lm -o build/pet/news_ui
mkdir -p build/room-preview
build/pet/news_ui
cc -O2 -ffp-contract=off -std=c99 -DOM_HOST -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen tests/pet/menu_ui.c firmware/src/dsp/omni.c -lm -o build/pet/menu_ui
build/pet/menu_ui
cc -O2 -ffp-contract=off -std=c99 -DOM_HOST -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen tests/pet/save_ui.c firmware/src/dsp/omni.c -lm -o build/pet/save_ui
build/pet/save_ui
cc -O2 -ffp-contract=off -std=c99 -DOM_HOST -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen tests/pet/energy_ui.c firmware/src/dsp/omni.c -lm -o build/pet/energy_ui
build/pet/energy_ui
cc -O2 -ffp-contract=off -std=c99 -DOM_HOST -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen tests/pet/poo_ui.c firmware/src/dsp/omni.c -lm -o build/pet/poo_ui
build/pet/poo_ui
cc -O2 -ffp-contract=off -std=c99 -DOM_HOST -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen tests/pet/wake_ui.c firmware/src/dsp/omni.c -lm -o build/pet/wake_ui
build/pet/wake_ui
cc -O1 -g -fsanitize=address,undefined -ffp-contract=off -std=c99 -DOM_HOST -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen tests/pet/game_over_ui.c firmware/src/dsp/omni.c -lm -o build/pet/game_over_ui
build/pet/game_over_ui
cc -O1 -g -fsanitize=address,undefined -ffp-contract=off -std=c99 -DOM_HOST -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen tests/pet/pause_ui.c firmware/src/dsp/omni.c -lm -o build/pet/pause_ui
build/pet/pause_ui
node tests/pet/browser_storage.mjs
cc -O2 tests/storage_test.c -o build/pet/storage_test
build/pet/storage_test
if test -f build/emu/omni.wasm; then node tests/pet/browser.mjs build/emu/omni.wasm; fi
