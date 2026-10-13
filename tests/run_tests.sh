#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/.."
sh host/build_host.sh
exec sh tests/pet/check_all.sh
