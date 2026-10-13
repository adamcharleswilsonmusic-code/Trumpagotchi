#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/../.."
PET_TEST_BIN="$(mktemp -d "${TMPDIR:-/tmp}/trumpagotchi-pet.XXXXXX")"
trap 'rm -rf "$PET_TEST_BIN"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined tests/pet/care_test.c -o "$PET_TEST_BIN/care_test"
"$PET_TEST_BIN/care_test"
