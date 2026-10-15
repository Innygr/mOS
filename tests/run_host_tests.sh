#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT_DIR="${TMPDIR:-/tmp}"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT/components/mission_link/include" \
  "$ROOT/components/mission_link/mission_link.c" \
  "$ROOT/tests/mission_link_codec_test.c" \
  -o "$OUT_DIR/mission_link_codec_test"
"$OUT_DIR/mission_link_codec_test"
rm -f "$OUT_DIR/mission_link_codec_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT/components/mission_link/include" \
  "$ROOT/components/mission_link/mission_link.c" \
  "$ROOT/components/mission_link/mission_link_service.c" \
  "$ROOT/tests/mission_link_service_test.c" \
  -o "$OUT_DIR/mission_link_service_test"
"$OUT_DIR/mission_link_service_test"
rm -f "$OUT_DIR/mission_link_service_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT/components/mission_link/include" \
  "$ROOT/components/mission_link/mission_link_queue.c" \
  "$ROOT/tests/mission_link_queue_test.c" \
  -o "$OUT_DIR/mission_link_queue_test"
"$OUT_DIR/mission_link_queue_test"
rm -f "$OUT_DIR/mission_link_queue_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT/components/mission_link/include" \
  "$ROOT/components/mission_link/mission_link_inbox.c" \
  "$ROOT/tests/mission_link_inbox_test.c" \
  -o "$OUT_DIR/mission_link_inbox_test"
"$OUT_DIR/mission_link_inbox_test"
rm -f "$OUT_DIR/mission_link_inbox_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT/components/mission_link/include" \
  "$ROOT/components/mission_link/mission_link.c" \
  "$ROOT/components/mission_link/mission_link_inbox.c" \
  "$ROOT/components/mission_link/mission_link_receive.c" \
  "$ROOT/tests/mission_link_receive_test.c" \
  -o "$OUT_DIR/mission_link_receive_test"
"$OUT_DIR/mission_link_receive_test"
rm -f "$OUT_DIR/mission_link_receive_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT/components/mission_link/include" \
  "$ROOT/components/mission_link/mission_link.c" \
  "$ROOT/components/mission_link/mission_link_queue.c" \
  "$ROOT/components/mission_link/mission_link_ack.c" \
  "$ROOT/tests/mission_link_ack_test.c" \
  -o "$OUT_DIR/mission_link_ack_test"
"$OUT_DIR/mission_link_ack_test"
rm -f "$OUT_DIR/mission_link_ack_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT/components/mission_link/include" \
  "$ROOT/components/mission_link/mission_link_queue.c" \
  "$ROOT/components/mission_link/mission_link_retry.c" \
  "$ROOT/tests/mission_link_retry_test.c" \
  -o "$OUT_DIR/mission_link_retry_test"
"$OUT_DIR/mission_link_retry_test"
rm -f "$OUT_DIR/mission_link_retry_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT/components/mission_link/include" \
  "$ROOT/components/mission_link/mission_link.c" \
  "$ROOT/components/mission_link/mission_link_queue.c" \
  "$ROOT/components/mission_link/mission_link_inbox.c" \
  "$ROOT/components/mission_link/mission_link_receive.c" \
  "$ROOT/components/mission_link/mission_link_ack.c" \
  "$ROOT/components/mission_link/mission_link_transport.c" \
  "$ROOT/tests/mission_link_transport_test.c" \
  -o "$OUT_DIR/mission_link_transport_test"
"$OUT_DIR/mission_link_transport_test"
rm -f "$OUT_DIR/mission_link_transport_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT/components/mos_shell/include" \
  "$ROOT/components/mos_shell/mos_gesture.c" \
  "$ROOT/components/mos_shell/mos_clock.c" \
  "$ROOT/components/mos_shell/mos_layout.c" \
  "$ROOT/tests/mos_gesture_test.c" \
  -o "$OUT_DIR/mos_gesture_test"
"$OUT_DIR/mos_gesture_test"
rm -f "$OUT_DIR/mos_gesture_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT/components/mos_shell/include" -I"$ROOT/tests" \
  "$ROOT/components/mos_shell/mos_gesture.c" \
  "$ROOT/components/mos_shell/mos_clock.c" \
  "$ROOT/components/mos_shell/mos_layout.c" \
  "$ROOT/components/mos_shell/mos_shell.c" \
  "$ROOT/tests/mos_shell_test.c" \
  -o "$OUT_DIR/mos_shell_test"
"$OUT_DIR/mos_shell_test"
rm -f "$OUT_DIR/mos_shell_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$ROOT/components/mission_link/include" \
  -I"$ROOT/components/mos_shell/include" \
  -I"$ROOT/components/mos_apps/include" -I"$ROOT/tests" \
  "$ROOT/components/mission_link/mission_link.c" \
  "$ROOT/components/mission_link/mission_link_service.c" \
  "$ROOT/components/mission_link/mission_link_queue.c" \
  "$ROOT/components/mission_link/mission_link_inbox.c" \
  "$ROOT/components/mission_link/mission_link_receive.c" \
  "$ROOT/components/mission_link/mission_link_ack.c" \
  "$ROOT/components/mission_link/mission_link_transport.c" \
  "$ROOT/components/mos_shell/mos_gesture.c" \
  "$ROOT/components/mos_shell/mos_clock.c" \
  "$ROOT/components/mos_shell/mos_layout.c" \
  "$ROOT/components/mos_shell/mos_shell.c" \
  "$ROOT/components/mos_apps/mos_app_messages.c" \
  "$ROOT/components/mos_apps/mos_app_settings.c" \
  "$ROOT/components/mos_apps/mos_app_store.c" \
  "$ROOT/tests/mos_messages_app_test.c" \
  -o "$OUT_DIR/mos_messages_app_test"
"$OUT_DIR/mos_messages_app_test"
rm -f "$OUT_DIR/mos_messages_app_test"
