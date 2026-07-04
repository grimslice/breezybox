# shell-test

A small, dependency-free test battery for shell features, for developing the
**breezybox** shell. It reuses the [Oils](https://oils.pub) spec-test *corpus*
and *format*, but with a ~350-line Python 3 runner instead of Oils'
`test/sh_spec.py` — so there's **no Python 2 and no Docker** to run it.

## Scope of this suite (what "pass" is measured against)

The **goal is dash parity, within what a single-process, no-`fork`/`exec` shell on
an ESP32 can realistically do** — close to dash, without dragging in a full Linux
process model. The corpus is pruned in two stages to reflect that:

1. **Pruned to what `/bin/dash` passes.** Matching dash's *error/reject* path (e.g.
   `## status: 2` on a bash-only construct) counts as a pass just like matching its
   success path — so bash-flavored files stay, scored against dash's behavior.
2. **Pruned to what breezybox can reach.** Cases that require an OS process model are
   removed, because they can never pass without "becoming Linux":
   - **Job control / async** — background `&`, `jobs`/`wait`/`fg`/`bg`, `$!`.
   - **Signals** — `trap`, `kill`, signal delivery (poor fit for the cooperative,
     no-fork executor).
   - **Process identity / limits** — `$PPID`, `ulimit`, `times`, and fd-table
     introspection via Linux-only `/proc/$$/fd`.

   Removed whole files: `background`, `builtin-trap*`, `builtin-kill`, `builtin-times`.
   Removed individual cases: the `ulimit` block in `builtin-process`; the `&` cases in
   `shell-grammar`; `$!`/`$PPID` in `vars-special`; the background/`wait` case in
   `errexit`; the `/proc/$$/fd` cases in `redirect`.
3. **Pruned to the builtins + registered-apps exec model.** BreezyBox runs builtins
   and a fixed set of registered apps — there is **no filesystem-`PATH` lookup or
   `exec` of on-disk files**, and no Unix permission/device model on the FAT store.
   Cases that assume either can never pass on-device, so they're removed:
   - **External-program / PATH exec** — the whole `command_` file (PATH lookup, the
     `hash` cache, shebang dispatch, "permission denied", non-executable-on-PATH,
     exec-of-a-directory), and the two `pipeline` cases that assert a stage runs in
     its own process (`argv[0] eval in child`, `last command in its own process`).
   - **Permission / file-type stat** — the whole `builtin-umask` file (single-user,
     no permission bits), and the file-type/ownership `[` operators in
     `builtin-bracket`: `-b -c -S -p -k -u -g -G -O -ef` and the `-t` (fd-is-a-tty)
     cases. The string/int/`-e -f -d -r -w -x` operators stay.

   Removed whole files: `command_`, `builtin-umask`.
   Removed individual cases: the file-type/`-t` operator cases in `builtin-bracket`;
   the two in-own-process cases in `pipeline`.

   **In scope (kept, even if hard):** `$$` (pure shell state — a stable fake PID, as
   dash keeps it constant across subshells), `$LINENO`, full-fd `redirect` without
   `/proc`, the `exec`/`exit` builtins, and all "reject-like-dash" bash files
   (`array`, `dbracket`, …). These stay because they're achievable in-process, not
   because dash happens to pass them.

If a construct is genuinely impossible in the breezybox model, it does **not** belong
in this suite — remove it here rather than letting it sit as a permanent FAIL.

## Layout

| Path | What |
|------|------|
| [`specrun.py`](specrun.py) | The runner + pruner. No third-party deps. |
| [`run.sh`](run.sh) | Convenience wrapper (defaults to the whole suite). |
| [`spec/`](spec/) | The test files — Oils' suite, **pruned to what dash passes AND breezybox can realistically reach** (see Scope above). |
| [`bin/`](bin/) | Helper scripts the cases call (`argv.py`, `printenv.py`, …), with `python3` shebangs. |

The pristine upstream Oils corpus is not vendored here; recover any removed case from
an upstream Oils checkout if a construct later comes into scope.

## Test format

Each `spec/*.test.sh` file is a list of self-checking cases:

```sh
#### echo with a pipe
echo hi | wc -l
## stdout: 1
## status: 0
```

- `## stdout:` / `## stderr:` — one line of expected output (trailing newline implicit).
- `## STDOUT:` … `## END` — multiline expected output (byte-exact, as written).
- `## stdout-json: "..."` — JSON-encoded expected output (for empty strings, embedded NULs, etc.).
- `## status: N` — expected exit code (defaults to `0` if omitted).
- `## OK dash …`, `## N-I dash …`, `## BUG dash/ash …` — per-shell overrides of the
  ideal expectation. `OK` = acceptable variant, `N-I` = feature not implemented,
  `BUG` = known divergence. The shell list is `/`-separated.

The runner mirrors `sh_spec.py` semantics: per case it makes a fresh temp dir
(used as both `cwd` and `$TMP`), runs the code through the shell on stdin with a
minimal env (`$SH` = the shell path, `$PATH` includes `bin/`), and compares
stdout/stderr/status. A cell's result is the *worst* of its assertions.

> **macOS note:** the cases assume GNU userland. `specrun.py` automatically
> prepends Homebrew's `coreutils`/`gnu-sed`/`grep` gnubin dirs to `$PATH` when
> present, so `wc`/`ls`/`sort`/`date` match the tests. Without GNU coreutils you
> get spurious FAILs from BSD formatting differences (e.g. `wc -l` padding).

## Usage

```sh
./run.sh                        # whole suite vs /bin/dash
./run.sh smoke loop case_       # named files
./run.sh -v -r 3-5 smoke        # verbose expected-vs-got diff, cases 3..5
./run.sh --shell /bin/bash word-split   # any shell binary
./run.sh --slow 1 loop          # flag any case taking >= 1s

python3 specrun.py --shell /path/to/sh smoke   # direct invocation
```

Result marks: **`pass`** (dash matched the expected output) · **`FAIL`**
(unexpected difference) · **`TIME`** (case exceeded `--timeout`, default 10s).
Exit status is non-zero if any `FAIL`/`TIME` remain.

The totals line also reports total wall time and how many cases were **slow**
(`>= --slow` seconds, default 2) — each such case is tagged inline with its
elapsed time, e.g. `[3.4s]`, to catch unexpected delays during development.

Oils annotates *why* a non-ideal match is acceptable (`ok` = accepted variant,
`N-I` = feature not implemented in that shell, `BUG` = known divergence) — for a
dash baseline these all just mean "dash matched," so they're collapsed into
`pass` by default. Pass `-d`/`--detail` to see the breakdown.

## How `spec/` was pruned

`spec/` started as Oils' full 222-file suite, then:

```sh
python3 specrun.py spec/*.test.sh --prune   # drop cases that FAIL on /bin/dash
# then delete files left with no #### cases
```

This removed ~2400 cases that `/bin/dash` can't pass — the YSH (Oil language)
corpus and bash/zsh/ksh-only features (arrays, `extglob`, `process-sub`, `let`,
`${x:off:len}`, `${x//y/z}`, namerefs, …). The `ysh-*`/`zsh-*` files were then
dropped entirely, and the remaining `N-I dash` annotations relabeled `OK`, since
implementation status in *other* shells is irrelevant here. To re-baseline,
re-run the commands above against the `spec/` directory of an upstream Oils
checkout.

## Testing breezybox

The usual entry point is `../run_tests.py` (or `make run` in `../`), which
builds on this runner and adds CRASH/HANG classification. For direct runs,
point `--shell` at the host build:

```sh
python3 specrun.py --shell ../breezysh --lane dash smoke loop case_
```

`/bin/dash` is the oracle: every kept case is known-good on dash, so a `FAIL`
for breezybox is a real gap. `--lane dash` lets breezybox reuse dash's accepted
variants; without it, each case's *ideal* expectation applies.
