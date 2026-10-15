#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT="${TMPDIR:-/tmp}/mission_link_codec_test"
cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT/components/mission_link/include" \
  "$ROOT/components/mission_link/mission_link.c" \
  "$ROOT/tests/mission_link_codec_test.c" -o "$OUT"
"$OUT"
rm -f "$OUT"
