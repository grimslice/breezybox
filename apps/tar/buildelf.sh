#!/bin/sh
# tar links microtar alongside the app source.
set -e
../buildelf.sh tar.c microtar.c
