#!/bin/sh
# Build modplay for the host (Mac/desktop) against SDL2.
#   Usage: ./buildhost.sh    ->  dist/modplay
set -e
cd "$(dirname "$0")"
mkdir -p dist
cc -O2 -Wall -o dist/modplay modplay.c host_sdl.c $(sdl2-config --cflags --libs)
echo "built dist/modplay"
