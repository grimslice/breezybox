# assert.sh - tiny test helpers for the BreezyBox on-device regression suite.
#
# Counters live in files, not shell vars: the interpreter forks subshells for
# $(...) and pipelines, so an in-memory counter set inside one would not survive.
# run.sh exports the counter/log paths below and sources this once; each test
# script then just calls the assert_* helpers.
#
#   RT_PASS RT_FAIL RT_SKIP  - one line appended per event; count via `wc -l`
#   RT_LOG                    - human-readable per-assertion log
#
# Policy: inline assert_eq for one-liners; capture-to-file + assert_diff against
# a committed fixture for multi-line output.

_log() { echo "$1" >> "$RT_LOG"; }

pass() { echo . >> "$RT_PASS"; _log "PASS $1"; }
fail() { echo . >> "$RT_FAIL"; _log "FAIL $1"; echo "    FAIL: $1"; }
skip() { echo . >> "$RT_SKIP"; _log "SKIP $1"; echo "    SKIP: $1"; }

# assert_eq <expected> <actual> [msg]   -- string equality (one-liners)
assert_eq() {
    if [ "$1" = "$2" ]; then pass "${3:-eq}"
    else fail "${3:-eq}: expected [$1] got [$2]"; fi
}

# assert_num <expected> <actual> [msg]  -- numeric equality; tolerates the
# leading whitespace some `wc` builds emit (uses -eq, which parses integers).
assert_num() {
    if [ "$2" -eq "$1" ] 2>/dev/null; then pass "${3:-num}"
    else fail "${3:-num}: expected [$1] got [$2]"; fi
}

# assert_ok <cmd...>   -- command exits 0
assert_ok() {
    if "$@" >/dev/null 2>&1; then pass "ok: $*"
    else fail "ok: $* (exit $?)"; fi
}

# assert_fail <cmd...> -- command exits non-zero
assert_fail() {
    if "$@" >/dev/null 2>&1; then fail "fail: $* (expected non-zero)"
    else pass "fail: $*"; fi
}

# assert_file <path>   -- exists and is non-empty
assert_file() {
    if [ -s "$1" ]; then pass "file: $1"
    else fail "file: $1 (missing or empty)"; fi
}

# assert_diff <expected_file> <actual_file>  -- equal contents (needs diff app)
assert_diff() {
    if diff "$1" "$2" >/dev/null 2>&1; then pass "diff: $2 == $1"
    else fail "diff: $2 != $1"; diff "$1" "$2" >> "$RT_LOG" 2>&1; fi
}

# assert_cksum <a> <b> -- identical checksum + byte count
assert_cksum() {
    ca=$(cksum < "$1"); cb=$(cksum < "$2")
    if [ "$ca" = "$cb" ]; then pass "cksum: $1 == $2"
    else fail "cksum: $1 ($ca) != $2 ($cb)"; fi
}

# have <cmd> [benign args...] -- true unless the command is absent (exit 127).
# Pass a harmless invocation; output is discarded. Used to skip tests whose
# dependency (an eget-installed ELF app) is not present.
have() { "$@" >/dev/null 2>&1; [ "$?" != 127 ]; }
