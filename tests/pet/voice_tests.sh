#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
mkdir -p build/pet
cc -O1 -g -fsanitize=address,undefined -ffp-contract=off -Wall -Wextra -Werror -DOM_HOST -Ifirmware/src/dsp tests/pet/voice_audio_test.c firmware/src/dsp/omni.c -o build/pet/voice_audio_test
build/pet/voice_audio_test
cc -O1 -g -fsanitize=address,undefined -ffp-contract=off -std=c99 -DOM_HOST -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen tests/pet/voice_ui_test.c firmware/src/dsp/omni.c -lm -o build/pet/voice_ui_test
build/pet/voice_ui_test
