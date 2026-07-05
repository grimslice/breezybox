#!/bin/sh
# tar links microtar alongside the app source.
set -e
../buildelf_rv32.sh tar.c microtar.c
