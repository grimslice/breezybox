#!/bin/sh
# build.sh - assemble the on-device regression bundle regtest.tgz.
#
# Copies the shared harness (lib/, run.sh), the tests, and the fixtures into a
# temp staging dir and packs it so it unpacks into a top-level "regtest/" dir on
# the device. Deliver the resulting dist/regtest.tgz however you like (httpd
# upload, serial, a GitHub release) and on the device:
#
#     tar xzf regtest.tgz
#     cd regtest && sh run.sh
#
# Requires host `tar` (this runs on the dev machine, not the device).
set -e
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

mkdir -p "$STAGE/regtest"
cp -R lib run.sh tests fixtures "$STAGE/regtest/"

# Drop editor/OS cruft that must not ship on the device.
find "$STAGE/regtest" -name '.DS_Store' -delete 2>/dev/null || true

mkdir -p dist
OUT="$PWD/dist/regtest.tgz"

# Produce a clean archive the on-device tar can read: no macOS AppleDouble
# (._*) sidecars and no pax extended headers (which the device tar unpacks as
# stray PaxHeader/ dirs). GNU tar (gtar) if present, else BSD tar with the
# equivalent flags; COPYFILE_DISABLE suppresses AppleDouble either way.
if command -v gtar >/dev/null 2>&1; then
    ( cd "$STAGE" && COPYFILE_DISABLE=1 gtar --format=ustar -czf "$OUT" regtest )
else
    ( cd "$STAGE" && COPYFILE_DISABLE=1 tar --format ustar -czf "$OUT" regtest )
fi

echo "built $OUT"
echo "contents:"
tar tzf "$OUT"
