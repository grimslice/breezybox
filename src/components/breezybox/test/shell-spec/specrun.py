#!/usr/bin/env python3
"""
specrun.py - a small, dependency-free runner for the Oils spec-test format.

The Oils ".test.sh" files are self-checking shell snippets: a "#### desc"
header, a body of shell code, and "## stdout: / ## status: / ## STDOUT: ... ##
END" assertion blocks, with per-shell "## OK/N-I/BUG <shells> ..." overrides.

This reimplements just enough of test/sh_spec.py to run those files against any
shell binary (default /bin/dash) and print a modernish-style pass/fail table.
No Python 2, no Docker. It can also --prune cases that FAIL on a reference
shell, to distill a suite that a target shell (e.g. breezybox) should pass.

Format reference: spec/README.md and reference/oils/test/sh_spec.py.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

# Same grammar as sh_spec.py KEY_VALUE_RE.
KEY_VALUE_RE = re.compile(
    r'''
    \#\# \s+
    (?: (OK(?:-\d)? | BUG(?:-\d)? | N-I) \s+ ([\w+/]+) \s+ )?   # opt qualifier+shells
    ([\w\-]+)              # key
    :
    \s* (.*)               # value
    ''', re.VERBOSE)
END_RE = re.compile(r'\#\#\s+END\s*$')
CASE_RE = re.compile(r'^####\s*(.*)')

META_FIELDS = {
    'our_shell', 'compare_shells', 'suite', 'tags',
    'oils_failures_allowed', 'oils_cpp_failures_allowed', 'legacy_tmp_dir',
}

# Result levels (higher is better), mirroring sh_spec.Result ordering.
TIMEOUT, FAIL, BUG, NI, OK, PASS = range(6)
# Detailed (osh-flavored) names; --detail shows these.  By default ok/N-I/BUG
# all collapse to 'pass' -- for a dash baseline they all mean "dash matched".
LEVEL_NAME = {TIMEOUT: 'TIME', FAIL: 'FAIL', BUG: 'BUG', NI: 'N-I',
              OK: 'ok', PASS: 'pass'}


def mark_for(level, detail):
    if detail:
        return LEVEL_NAME[level]
    if level == TIMEOUT:
        return 'TIME'
    if level == FAIL:
        return 'FAIL'
    return 'pass'
QUAL_LEVEL = {None: PASS, 'OK': OK, 'OK-2': OK, 'OK-3': OK, 'OK-4': OK,
              'N-I': NI, 'BUG': BUG, 'BUG-2': BUG}


class Case(object):
    def __init__(self, desc, line_num):
        self.desc = desc
        self.line_num = line_num
        self.code = ''
        self.default = {}        # name -> value (ideal expected)
        self.per_shell = {}      # shell -> {name: value, 'qualifier': q}
        self.src_start = 0       # source line span (0-based, inclusive)
        self.src_end = 0         # exclusive


def _add_meta(case, qualifier, shells, name, value):
    for sh in shells.split('/'):
        d = case.per_shell.setdefault(sh, {})
        d[name] = value
        d['qualifier'] = qualifier


def parse_file(path):
    """Return (file_meta, [Case]).  Span-aware so cases can be rewritten."""
    with open(path, 'r', newline='') as f:
        lines = f.readlines()

    file_meta = {}
    cases = []
    i = 0
    n = len(lines)

    # File-level metadata before the first case.
    while i < n and not CASE_RE.match(lines[i]):
        m = KEY_VALUE_RE.match(lines[i])
        if m and m.group(1) is None and m.group(3) in META_FIELDS:
            file_meta[m.group(3)] = m.group(4).rstrip('\n')
        i += 1

    while i < n:
        m = CASE_RE.match(lines[i])
        if not m:
            i += 1
            continue
        case = Case(m.group(1).strip(), i + 1)
        case.src_start = i
        i += 1
        code_lines = []

        while i < n and not CASE_RE.match(lines[i]):
            line = lines[i]
            km = KEY_VALUE_RE.match(line)
            if km:
                qualifier, shells, name, value = km.groups()
                if name in ('STDOUT', 'STDERR'):
                    # Multiline block: collect raw lines until ## END / ## / ####.
                    name = name.lower()
                    i += 1
                    buf = []
                    while i < n:
                        if END_RE.match(lines[i]):
                            i += 1
                            break
                        if CASE_RE.match(lines[i]) or KEY_VALUE_RE.match(lines[i]):
                            break
                        buf.append(lines[i])
                        i += 1
                    value = ''.join(buf)
                    if qualifier:
                        _add_meta(case, qualifier, shells, name, value)
                    else:
                        case.default[name] = value
                    continue
                # Single-line key: value.
                if name in ('stdout', 'stderr'):
                    value += '\n'        # implicit newline, per format
                if qualifier:
                    _add_meta(case, qualifier, shells, name, value)
                else:
                    case.default[name] = value
                i += 1
                continue
            # A '#'-comment line (not '##') is ignored, like sh_spec.
            if line.lstrip().startswith('#') and not line.lstrip().startswith('##'):
                i += 1
                continue
            code_lines.append(line)
            i += 1

        # Trim trailing blank lines that merely separate cases.
        while code_lines and code_lines[-1].strip() == '':
            code_lines.pop()
        case.code = ''.join(code_lines)
        # Some cases supply the script via "## code: ..." instead of plain lines
        # (e.g. intentionally broken syntax that tests parse errors).
        if not case.code and 'code' in case.default:
            case.code = case.default['code']
            if not case.code.endswith('\n'):
                case.code += '\n'
        case.src_end = i
        cases.append(case)

    return file_meta, cases


def build_expected(case, lane):
    """Return list of (name, expected_bytes, level) assertions for a lane."""
    asserts = []
    shd = case.per_shell.get(lane, {})
    q = shd.get('qualifier')

    def encode(v):
        return v.encode('utf-8') if isinstance(v, str) else v

    for key in ('stdout', 'stderr'):
        if key in shd or (key + '-json') in shd:
            if key in shd:
                asserts.append((key, encode(shd[key]), QUAL_LEVEL[q]))
            if (key + '-json') in shd:
                asserts.append((key, encode(json.loads(shd[key + '-json'])), QUAL_LEVEL[q]))
        elif key in case.default or (key + '-json') in case.default:
            if key in case.default:
                asserts.append((key, encode(case.default[key]), PASS))
            if (key + '-json') in case.default:
                asserts.append((key, encode(json.loads(case.default[key + '-json'])), PASS))

    # status
    if 'status' in shd:
        asserts.append(('status', int(shd['status']), QUAL_LEVEL[q]))
    elif 'status' in case.default:
        asserts.append(('status', int(case.default['status']), PASS))
    else:
        asserts.append(('status', 0, PASS))   # default: must exit 0
    return asserts


def run_case(case, shell, env_base, timeout):
    """Run one case, returning (timed_out, actual, elapsed_seconds)."""
    tmp = tempfile.mkdtemp(prefix='specrun-')
    try:
        env = dict(env_base)
        env['SH'] = shell
        env['TMP'] = tmp
        start = time.monotonic()
        try:
            p = subprocess.run([shell], input=case.code.encode('utf-8'),
                               env=env, cwd=tmp, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, timeout=timeout)
        except subprocess.TimeoutExpired:
            return True, {}, time.monotonic() - start
        elapsed = time.monotonic() - start
        actual = {'stdout': p.stdout, 'stderr': p.stderr, 'status': p.returncode}
        return False, actual, elapsed
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def verdict(case, lane, actual):
    """Cell result = min over assertions (FAIL if any mismatch)."""
    level = PASS
    msgs = []
    for name, expected, alevel in build_expected(case, lane):
        got = actual[name]
        if got != expected:
            level = FAIL
            msgs.append((name, expected, got))
        else:
            level = min(level, alevel)
    # Guard against shell-internal tracebacks leaking (rare; matches sh_spec).
    if b'Traceback (most recent' in actual.get('stderr', b''):
        level = FAIL
    return level, msgs


# The spec tests were authored against GNU userland (Oils runs them in a Linux
# image).  On macOS, prefer Homebrew's GNU coreutils/sed/grep if present so that
# `wc`, `ls`, `sort`, `date` etc. behave as the tests expect -- otherwise BSD
# formatting differences produce false FAILs unrelated to the shell.
GNU_PATHS = [
    '/opt/homebrew/opt/coreutils/libexec/gnubin',
    '/opt/homebrew/opt/gnu-sed/libexec/gnubin',
    '/opt/homebrew/opt/grep/libexec/gnubin',
    '/usr/local/opt/coreutils/libexec/gnubin',
    '/usr/local/opt/gnu-sed/libexec/gnubin',
]


def make_env(repo_root):
    binhelpers = os.path.join(repo_root, 'bin')
    parts = [binhelpers]
    parts += [p for p in GNU_PATHS if os.path.isdir(p)]
    parts.append(os.environ.get('PATH', '/usr/bin:/bin'))
    return {
        'PATH': ':'.join(parts),
        'LC_ALL': 'C.UTF-8',
        'REPO_ROOT': repo_root,
        'HOME': os.environ.get('HOME', '/tmp'),
    }


def parse_range(s):
    if '-' in s:
        a, b = s.split('-', 1)
        return int(a), int(b)
    return int(s), int(s)


def fmt_diff(name, expected, got):
    def show(b):
        return repr(b.decode('utf-8', 'replace')) if isinstance(b, bytes) else repr(b)
    return '      [%s] expected %s got %s' % (name, show(expected), show(got))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('files', nargs='+', help='spec/*.test.sh files (or names)')
    ap.add_argument('--shell', default='/bin/dash', help='shell binary (default /bin/dash)')
    ap.add_argument('--lane', default=None,
                    help="annotation lane for OK/N-I/BUG overrides (default: shell basename)")
    ap.add_argument('-r', '--range', help='case range, e.g. 5 or 5-9')
    ap.add_argument('-v', '--verbose', action='store_true', help='show diff for non-pass')
    ap.add_argument('--timeout', type=float, default=10.0,
                    help='per-case timeout in seconds (default 10); a case that '
                         'exceeds it is marked TIME')
    ap.add_argument('--slow', type=float, default=2.0,
                    help='flag cases slower than this many seconds (default 2)')
    ap.add_argument('--summary', action='store_true', help='per-file summary only')
    ap.add_argument('-d', '--detail', action='store_true',
                    help='show osh-flavored marks (ok/N-I/BUG) instead of collapsing to pass')
    ap.add_argument('--prune', action='store_true',
                    help='rewrite files in place, removing cases that FAIL on the lane')
    args = ap.parse_args()

    repo_root = os.path.dirname(os.path.abspath(__file__))
    lane = args.lane or os.path.basename(args.shell)
    env_base = make_env(repo_root)
    rng = parse_range(args.range) if args.range else None

    import collections
    totals = collections.defaultdict(int)
    total_time = 0.0

    for fname in args.files:
        path = fname
        if not os.path.exists(path):
            cand = os.path.join(repo_root, 'spec', fname)
            if not cand.endswith('.test.sh'):
                cand += '.test.sh'
            path = cand
        if not os.path.exists(path):
            print('!! missing: %s' % fname, file=sys.stderr)
            continue

        _, cases = parse_file(path)
        results = []  # (case, level, msgs)
        print('\n### %s' % os.path.relpath(path, repo_root))
        for idx, case in enumerate(cases):
            if rng and not (rng[0] <= idx <= rng[1]):
                continue
            timed_out, actual, elapsed = run_case(case, args.shell, env_base,
                                                  args.timeout)
            if timed_out:
                level, msgs = TIMEOUT, []
            else:
                level, msgs = verdict(case, lane, actual)
            results.append((case, level, msgs))
            mark = mark_for(level, args.detail)
            totals[mark] += 1
            total_time += elapsed
            slow = elapsed >= args.slow
            if slow:
                totals['slow'] += 1
            note = '   [%.1fs]' % elapsed if slow else ''
            print('  %3d  %-5s %s%s' % (idx, mark, case.desc, note))
            if args.verbose and level <= FAIL:
                for name, exp, got in msgs:
                    print(fmt_diff(name, exp, got))

        if args.prune:
            prune_file(path, results)

    print('\n==== totals ====')
    if args.detail:
        order = ['pass', 'ok', 'N-I', 'BUG', 'FAIL', 'TIME']
    else:
        order = ['pass', 'FAIL', 'TIME']
    print('  ' + '  '.join('%s=%d' % (k, totals.get(k, 0)) for k in order))
    print('  time=%.1fs  slow(>=%.0fs)=%d' % (total_time, args.slow,
                                              totals.get('slow', 0)))
    # Exit non-zero if any unexpected FAIL/TIMEOUT remain.
    return 1 if (totals['FAIL'] or totals['TIME']) else 0


def prune_file(path, results):
    """Remove cases whose lane verdict is FAIL/TIMEOUT, rewriting the file."""
    drop = sorted((c.src_start, c.src_end) for c, lvl, _ in results
                  if lvl is not None and lvl <= FAIL)
    if not drop:
        return
    with open(path, 'r', newline='') as f:
        lines = f.readlines()
    keep = []
    cursor = 0
    for start, end in drop:
        keep.append((cursor, start))
        cursor = end
    keep.append((cursor, len(lines)))
    out = []
    for a, b in keep:
        out.extend(lines[a:b])
    with open(path, 'w', newline='') as f:
        f.writelines(out)
    print('  -- pruned %d FAIL case(s) from %s' % (len(drop), os.path.basename(path)))


if __name__ == '__main__':
    sys.exit(main())
