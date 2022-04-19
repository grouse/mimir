#!/bin/bash

ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
BUILD_DIR="$ROOT/build"

mkdir -p "$BUILD_DIR"

DEFINITIONS=""
WARNINGS="-Wall -Wextra -Wno-missing-braces -Wno-logical-op-parentheses -Wno-c99-designator -Wno-char-subscripts"
FLAGS="$DEFINITIONS $WARNINGS -g -fno-exceptions -fno-rtti -std=c++17 -fno-color-diagnostics"


INCLUDE_DIR="-I$ROOT"
LIBS="-ldl -lX11 -lGLX -lpthread"

pushd "$BUILD_DIR"

clang++ -O0 -ferror-limit=0 $FLAGS $INCLUDE_DIR $LIBS -o mimir $ROOT/src/linux_mimir.cpp

popd
