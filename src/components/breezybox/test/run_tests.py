#!/usr/bin/env python3
"""
run_tests.py - drive the lifted OSH/dash spec subset against the host build of
the BreezyBox shell core (./breezysh).

This reuses the parser/runner from shell-spec/specrun.py, but classifies each
case for breezybox development:

    PASS               dash-expected output+status matched -- the ideal result,
                       an acceptable OK variant, or dash's own N-I/BUG output
    FAIL(unimpl/wrong) mismatch -- a missing feature OR a wrong result for
                       something we claim to support (needs review)
    ERROR(crash|hang)  the interpreter crashed (ASan/UBSan/segfault) or hung
                       -- these must be ZERO; they are real bugs.

Usage:
    ./run_tests.py [--shell ./breezysh] [--spec shell-spec] [files...]
"""
import argparse
import glob
import importlib.util
import os
import sys


def load_specrun(spec_dir):
    path = os.path.join(spec_dir, 'specrun.py')
    if not os.path.isfile(path):
        sys.exit("specrun.py not found in %s (mount the shell-test checkout)" % spec_dir)
    spec = importlib.util.spec_from_file_location('specrun', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


CRASH_MARKERS = (b'AddressSanitizer', b'runtime error:', b'Sanitizer',
                 b'Segmentation fault', b'stack-buffer', b'heap-buffer')


def classify(sr, case, lane, timed_out, actual):
    if timed_out:
        return 'HANG'
    stderr = actual.get('stderr', b'')
    status = actual.get('status', 0)
    if any(m in stderr for m in CRASH_MARKERS) or status < 0 or status in (139, 134, 136):
        return 'CRASH'
    level, _ = sr.verdict(case, lane, actual)
    # Anything above FAIL means we matched what dash does on this case -- the
    # ideal result (PASS), an acceptable variant (OK), or dash's own N-I/BUG
    # output. Matching dash's error/quirk path is as valid a pass as matching
    # its success path. Only FAIL (and TIMEOUT) mismatch.
    return 'PASS' if level > sr.FAIL else 'FAIL'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--shell', default='./breezysh')
    ap.add_argument('--spec',
                    default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                         'shell-spec'))
    ap.add_argument('--lane', default='dash')
    ap.add_argument('--timeout', type=float, default=5.0)
    ap.add_argument('-v', '--verbose', action='store_true')
    ap.add_argument('files', nargs='*')
    args = ap.parse_args()

    # Cases run with cwd set to a temp dir, so any relative path handed to the
    # child resolves against that temp dir, not here. Absolutize both --shell
    # and --spec: a relative --spec would put a bogus "<spec>/bin" on the
    # child's PATH, silently dropping the argv.py/printenv.py helpers that many
    # cases invoke -- turning ~24 real PASSes into FAILs (looks like flakiness).
    args.spec = os.path.abspath(args.spec)
    if os.sep in args.shell or os.path.exists(args.shell):
        args.shell = os.path.abspath(args.shell)

    sr = load_specrun(args.spec)
    env_base = sr.make_env(args.spec)

    files = args.files
    if not files:
        files = sorted(glob.glob(os.path.join(args.spec, 'spec', '*.test.sh')))

    totals = {'PASS': 0, 'FAIL': 0, 'CRASH': 0, 'HANG': 0}
    crash_list = []

    for path in files:
        name = os.path.basename(path)
        _, cases = sr.parse_file(path)
        counts = {'PASS': 0, 'FAIL': 0, 'CRASH': 0, 'HANG': 0}
        for case in cases:
            timed_out, actual, _ = sr.run_case(case, args.shell, env_base, args.timeout)
            cls = classify(sr, case, args.lane, timed_out, actual)
            counts[cls] += 1
            totals[cls] += 1
            if cls in ('CRASH', 'HANG'):
                crash_list.append((name, case.desc, cls))
            if args.verbose:
                print("  [%-5s] %s" % (cls, case.desc))
        print("%-28s pass %3d  fail %3d  crash %2d  hang %2d" %
              (name, counts['PASS'], counts['FAIL'], counts['CRASH'], counts['HANG']))

    print("-" * 60)
    print("TOTAL  PASS %d  FAIL(unimpl/wrong) %d  CRASH %d  HANG %d" %
          (totals['PASS'], totals['FAIL'], totals['CRASH'], totals['HANG']))
    if crash_list:
        print("\nBUGS (must be zero):")
        for f, d, c in crash_list:
            print("  %-6s %s :: %s" % (c, f, d))

    # Non-zero exit only if there were real bugs (crash/hang).
    return 1 if (totals['CRASH'] or totals['HANG']) else 0


if __name__ == '__main__':
    sys.exit(main())
