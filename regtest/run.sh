#!/bin/sh
# run.sh - BreezyBox on-device regression runner.
#
# Usage (on device):  cd regtest && sh run.sh
#
# Sources lib/assert.sh, then each tests/NN_*.sh in order, aggregates the
# pass/fail/skip counters, writes regtest.log, and exits non-zero on any FAIL
# (skips do not fail the run). Run from inside the regtest directory.

# Absolute base so tests that cd around (tar) don't break relative paths.
BASE=$(pwd)
WORK="$BASE/_work"
rm -rf "$WORK"
mkdir -p "$WORK"

# Counter + log files, exported for the assert helpers (see lib/assert.sh).
RT_PASS="$WORK/pass"; RT_FAIL="$WORK/fail"; RT_SKIP="$WORK/skip"
RT_LOG="$BASE/regtest.log"
RT_TMP="$WORK/tmp"; mkdir -p "$RT_TMP"   # scratch space for tests
RT_FIX="$BASE/fixtures"                    # committed expected-output files
export RT_PASS RT_FAIL RT_SKIP RT_LOG RT_TMP RT_FIX
: > "$RT_PASS"; : > "$RT_FAIL"; : > "$RT_SKIP"; : > "$RT_LOG"

. ./lib/assert.sh

echo "BreezyBox regression suite"
echo "=========================="

# Run one test file: print a header, source it, then restore cwd (tests may cd
# around, and the interpreter's ( ) does not isolate cwd). Deliberately NOT a
# for-loop over the glob: sourcing a file that itself contains for/while loops
# corrupts an *enclosing* loop's state on this interpreter, which silently
# aborted the run after the first file. A flat call per file avoids that.
runtest() {
    [ -f "$1" ] || return 0
    echo ""
    echo "--- $1"
    _log "==== $1 ===="
    . "$1"
    cd "$BASE"
}

runtest tests/00_smoke.sh
runtest tests/10_fs.sh
runtest tests/20_textutils.sh
runtest tests/30_redir_pipe.sh
runtest tests/40_tar.sh
runtest tests/50_gzip.sh
runtest tests/60_tools.sh

P=$(wc -l < "$RT_PASS"); F=$(wc -l < "$RT_FAIL"); S=$(wc -l < "$RT_SKIP")
echo ""
echo "=========================="
echo "TOTAL: $P OK, $F FAIL, $S SKIP"
echo "(details in regtest.log)"

rm -rf "$WORK"
[ "$F" -eq 0 ]
