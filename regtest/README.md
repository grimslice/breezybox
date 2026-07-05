# BreezyBox regression suite

An on-device, self-checking test bundle for BreezyBox's shell, core commands,
and eget-installed ELF apps.

## Run on a device

1. Install the ELF apps the suite exercises (once):

       eget valdanylchuk/breezybox

   This provides `tar`, `gzip`/`gunzip`, and the new `diff`/`grep`. `cksum` and
   `printf` are built-in. Tests whose dependency is missing are **skipped**, not
   failed.

2. Get `regtest.tgz` onto the device (httpd upload, serial, etc.), then:

       tar xzf regtest.tgz
       cd regtest
       sh run.sh

   Output is a per-file and total `N OK, M FAIL, K SKIP`, plus a detailed
   `regtest.log`. Exit status is non-zero if anything failed.

## Layout

    lib/assert.sh   shared assert_* helpers (counters kept in files)
    run.sh          entry point: preflight, run tests, aggregate, log
    tests/NN_*.sh   test scripts, run in filename order
    fixtures/       committed expected-output files (for assert_diff)
    build.sh        packs the above into dist/regtest.tgz (run on the dev host)

## Build the bundle

    ./build.sh        # -> dist/regtest.tgz

## Host dry-run

You can smoke-test the harness and shell semantics against the host shell build
(`src/components/breezybox/test/breezysh`), which execs your system tools for
external commands:

    cd regtest && ../src/components/breezybox/test/breezysh run.sh

Expected caveat: the **gzip** test fails on macOS/Linux hosts because their
`gzip` does not accept the `gzip <in> <out>` two-argument form that the BreezyBox
gzip app uses. It passes on-device. Everything else runs green on the host.
