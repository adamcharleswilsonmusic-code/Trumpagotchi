#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/../.."
mkdir -p build/pet
"${CC:-cc}" -O1 -g -fsanitize=address,undefined -ffp-contract=off -Wall -Wextra -Werror \
  -DOM_HOST -Ifirmware/src/dsp tests/pet/audio_test.c firmware/src/dsp/omni.c -o build/pet/audio_test
build/pet/audio_test
