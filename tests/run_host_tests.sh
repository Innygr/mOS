#!/bin/sh
# Compile and run every host-side test with strict warnings.
# Set SANITIZE=1 to also build with AddressSanitizer and UndefinedBehaviorSanitizer.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT_DIR=$(mktemp -d)
trap 'rm -rf "$OUT_DIR"' EXIT

COMP="$ROOT/components/mission_link"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pedantic -I$COMP/include -I$ROOT/tests"
if [ "${SANITIZE:-0}" = "1" ]; then
  CFLAGS="$CFLAGS -g -fsanitize=address,undefined -fno-sanitize-recover=undefined"
fi

run_test() {
  name=$1
  shift
  # shellcheck disable=SC2086
  cc $CFLAGS "$@" -o "$OUT_DIR/$name"
  "$OUT_DIR/$name"
}

run_test codec_test \
  "$COMP/mission_link.c" \
  "$ROOT/tests/mission_link_codec_test.c"

run_test service_test \
  "$COMP/mission_link.c" "$COMP/mission_link_service.c" \
  "$ROOT/tests/mission_link_service_test.c"

run_test rx_test \
  "$COMP/mission_link.c" "$COMP/mission_link_service.c" \
  "$COMP/mission_link_transport.c" "$COMP/mission_link_rx.c" \
  "$ROOT/tests/fake_transport.c" "$ROOT/tests/mission_link_rx_test.c"
