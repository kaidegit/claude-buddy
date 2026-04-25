#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-/tmp/claude-buddy-host-tests}"
CJSON_DIR="$ROOT_DIR/SiFli-SDK/external/cJSON-1.7.17"
CXX_BIN="${CXX:-c++}"
CC_BIN="${CC:-cc}"

mkdir -p "$BUILD_DIR"

"$CC_BIN" -std=c99 -I"$ROOT_DIR/tests/host" -I"$CJSON_DIR" \
  -c "$CJSON_DIR/cJSON.c" -o "$BUILD_DIR/cJSON.o"

COMMON_CXX_FLAGS=(
  -std=c++17
  -Wall
  -Wextra
  -I"$ROOT_DIR/app/src"
  -I"$ROOT_DIR/app/src/ascii"
  -I"$ROOT_DIR/app/src/core"
  -I"$CJSON_DIR"
  -I"$ROOT_DIR/tests/host"
)

ASCII_SRCS=(
  "$ROOT_DIR"/app/src/ascii/*.cpp
  "$ROOT_DIR"/app/src/ascii/buddies/*.cpp
)

"$CXX_BIN" "${COMMON_CXX_FLAGS[@]}" \
  "$ROOT_DIR/tests/host/test_json_line_assembler.cpp" \
  "$ROOT_DIR/app/src/core/JsonLineAssembler.cpp" \
  -o "$BUILD_DIR/test_json_line_assembler"

"$CXX_BIN" "${COMMON_CXX_FLAGS[@]}" \
  "$ROOT_DIR/tests/host/test_buddy_protocol.cpp" \
  "$ROOT_DIR/app/src/core/BuddyProtocol.cpp" \
  "$BUILD_DIR/cJSON.o" \
  -o "$BUILD_DIR/test_buddy_protocol"

"$CXX_BIN" "${COMMON_CXX_FLAGS[@]}" \
  "$ROOT_DIR/tests/host/test_buddy_app.cpp" \
  "$ROOT_DIR/app/src/core/BuddyApp.cpp" \
  "$ROOT_DIR/app/src/core/BuddyProtocol.cpp" \
  "$ROOT_DIR/app/src/core/JsonLineAssembler.cpp" \
  "$BUILD_DIR/cJSON.o" \
  -o "$BUILD_DIR/test_buddy_app"

"$CXX_BIN" "${COMMON_CXX_FLAGS[@]}" \
  "$ROOT_DIR/tests/host/test_buddy_ascii.cpp" \
  "${ASCII_SRCS[@]}" \
  -o "$BUILD_DIR/test_buddy_ascii"

"$BUILD_DIR/test_json_line_assembler"
"$BUILD_DIR/test_buddy_protocol"
"$BUILD_DIR/test_buddy_app"
"$BUILD_DIR/test_buddy_ascii"
