#!/bin/sh

set -xe

CC="${CXX:-cc}"
PKGS="sdl2 glew freetype2"
CFLAGS="-Wall -Wextra -std=c11 -pedantic -ggdb"
LIBS=-lm
SRC="src/main.c src/la.c src/editor.c src/file_browser.c src/free_glyph.c src/simple_renderer.c src/common.c src/lexer.c src/pdf_viewer.c"
OUT="build/ded"

if [ `uname` = "Darwin" ]; then
    CFLAGS+=" -framework OpenGL"
fi

mkdir -p build
$CC $CFLAGS `pkg-config --cflags $PKGS` -o "$OUT" $SRC $LIBS `pkg-config --libs $PKGS`
