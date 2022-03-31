#!/bin/sh -e
CFLAGS="\
-pedantic -Wall -Wextra \
-Wfatal-errors -std=c99 \
-D_POSIX_C_SOURCE=200809L $CFLAGS"

: ${CC:=$(command -v cc)}
: ${PREFIX:=/usr/local}

run() {
	printf '%s\n' "$*"
	"$@"
}

install() {
	[ -x fc ] || build
	run mkdir -p "$DESTDIR$PREFIX/bin/"
	run cp -f fc "$DESTDIR$PREFIX/bin/fc"
}

build() {
	run "$CC" "fc.c" $CFLAGS -o fc
}

if [ "$#" -gt 0 ]; then
	"$@"
else
	build
fi
