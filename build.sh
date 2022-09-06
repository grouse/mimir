#!/bin/bash

ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
EXTERNAL="$ROOT/external"
BUILD_DIR="$ROOT/build"

mkdir -p "$BUILD_DIR"

DEFINITIONS=""
WARNINGS="-Wall -Wextra -Wno-missing-braces -Wno-logical-op-parentheses -Wno-c99-designator -Wno-char-subscripts"
FLAGS="$DEFINITIONS $WARNINGS -g -fno-exceptions -fno-rtti -std=c++17 -fno-color-diagnostics"

TS_DIR="$EXTERNAL/tree-sitter-0.20.6"
TS_CPP_DIR="$EXTERNAL/tree-sitter-cpp-master"
TS_RS_DIR="$EXTERNAL/tree-sitter-rust-0.20.1"
TS_BASH_DIR="$EXTERNAL/tree-sitter-bash-master"
TS_CS_DIR="$EXTERNAL/tree-sitter-c-sharp-0.19.1"
TS_LUA_DIR="$EXTERNAL/tree-sitter-lua-master"
TS_COMMENT_DIR="$EXTERNAL/tree-sitter-comment-master"

INCLUDE_DIR="-I$ROOT -I$TS_DIR/lib/include"
LIBS="-ldl -lX11 -lGLX -lpthread"

pushd "$BUILD_DIR"
#clang -O3 -g -w -c -I$TS_DIR/src -I$TS_DIR/lib/include -o tree_sitter.o $TS_DIR/lib/src/lib.c

#clang -O3 -g -w -c -I$TS_CPP_DIR/src -o tree_sitter_cpp_parser.o $TS_CPP_DIR/src/parser.c
#clang -O3 -g -w -c -I$TS_CPP_DIR/src -o tree_sitter_cpp_scanner.o $TS_CPP_DIR/src/scanner.cc
TS_CPP="tree_sitter_cpp_parser.o tree_sitter_cpp_scanner.o"

#clang -O3 -g -w -c -I$TS_RS_DIR/src -o tree_sitter_rust_parser.o $TS_RS_DIR/src/parser.c
#clang -O3 -g -w -c -I$TS_RS_DIR/src -o tree_sitter_rust_scanner.o $TS_RS_DIR/src/scanner.c
TS_RS="tree_sitter_rust_parser.o tree_sitter_rust_scanner.o"

#clang -O3 -g -w -c -I$TS_BASH_DIR/src -o tree_sitter_bash_parser.o $TS_BASH_DIR/src/parser.c
#clang -O3 -g -w -c -I$TS_BASH_DIR/src -o tree_sitter_bash_scanner.o $TS_BASH_DIR/src/scanner.cc
TS_BASH="tree_sitter_bash_parser.o tree_sitter_bash_scanner.o"

#clang -O3 -g -w -c -I$TS_CS_DIR/src -o tree_sitter_cs_parser.o $TS_CS_DIR/src/parser.c
#clang -O3 -g -w -c -I$TS_CS_DIR/src -o tree_sitter_cs_scanner.o $TS_CS_DIR/src/scanner.c
TS_CS="tree_sitter_cs_parser.o tree_sitter_cs_scanner.o"

#clang -O3 -g -w -c -I$TS_LUA_DIR/src -o tree_sitter_lua_parser.o $TS_LUA_DIR/src/parser.c
#clang -O3 -g -w -c -I$TS_LUA_DIR/src -o tree_sitter_lua_scanner.o $TS_LUA_DIR/src/scanner.c
TS_LUA="tree_sitter_lua_parser.o tree_sitter_lua_scanner.o"

#clang -O3 -g -w -c -I$TS_COMMENT_DIR/src -o tree_sitter_comment_parser.o $TS_COMMENT_DIR/src/parser.c
#clang -O3 -g -w -c -I$TS_COMMENT_DIR/src -o tree_sitter_comment_scanner.o $TS_COMMENT_DIR/src/scanner.c
TS_COMMENT="tree_sitter_comment_parser.o tree_sitter_comment_scanner.o"


TS_OBJS="tree_sitter.o $TS_CPP $TS_RS $TS_BASH $TS_CS $TS_LUA $TS_COMMENT"

clang++ -O0 -ferror-limit=0 $FLAGS $INCLUDE_DIR $LIBS -o mimir $ROOT/src/linux_mimir.cpp $TS_OBJS

popd
