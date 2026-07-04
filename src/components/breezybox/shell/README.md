# BreezyBox shell scripting core

A small POSIX-ish shell interpreter, enabled at compile time with
`CONFIG_BREEZYBOX_SHELL_SCRIPTING` (default **off**).

## Layout

The core is **pure C** — it includes only `<stdio.h>`, `<stdlib.h>`,
`<string.h>`, `<ctype.h>` and `sh_port.h`. All platform coupling lives behind
the port seam, so the same core runs on the device and host-native for tests.

| File            | Role                                                        |
|-----------------|-------------------------------------------------------------|
| `sh_lex.c/.h`   | Tokenizer: quotes, operators, comments, here-docs, line continuation. |
| `sh_parse.c/.h` | Recursive-descent parser → AST.                             |
| `sh_expand.c`   | Parameter/command/arithmetic expansion, word-splitting, globbing hook. |
| `sh_arith.c/.h` | `$(( ))` evaluator (signed long, POSIX operator subset).    |
| `sh_glob.c/.h`  | Pattern matching (`case`, `${v#p}`) + single-directory pathname expansion. |
| `sh_state.c`    | Shell variables, functions, `local` scopes, positional params, flags. |
| `sh_builtins.c` | `test [ true false : echo exit export unset cd pwd read eval . source set shift local return break continue`. |
| `sh_exec.c`     | Tree-walking executor, exit-status propagation, `sh_run_string`. |
| `sh_redir.c`    | Ordered fd-level redirection over stdin/stdout/stderr (shared FILE* swap). |
| `sh_port.h`     | Platform interface (external exec, cwd, temp files).        |
| `sh_port_esp.c` | Device impl — wraps the ELF loader / esp_console registry.  |
| `sh_port_host.c`| Host impl — `fork`/`execvp` + real host FS (test only).     |

## Language subset

Implemented: quoting (single/double, backslash), parameter expansion
(`$VAR`, `${VAR}` with `:- - :+ + := = :? ?`, `# ## % %%` strip, `${#v}`),
positional/special params (`$1..`, `$@ $* $# $? $0`), IFS word-splitting,
command substitution `$(...)`/backticks, arithmetic `$(( ))`, tilde `~`,
globbing (`* ? [...]`, final path component only), assignments (persistent or
temporary command prefix), `if`/`elif`/`else`, `while`/`until`, `for`, `case`,
functions with `local`/`return`, `{ }` groups and `( )` subshells, `test`/`[`,
`&&`/`||`, `!`, `;` and newline separators, `|` pipelines (temp-file
semantics), redirection (`< > >> >& <& >|` on fds 0-2, here-docs `<< <<-`),
`#` comments, `break`/`continue [n]`, `set -e/-u`, `eval`, `.`/`source`,
`read`, `shift`.

**Out of scope:** anything needing a real process model — background `&`, job
control, signals/`trap`, `$!`/`$PPID`, concurrent pipes, `exec`, fd-table
manipulation beyond 0-2 — plus bash extensions (arrays, `[[ ]]`, `${x//y/z}`,
substring expansion, ...).

## Pipes

Pipes are **temp-file based** (`a | b` ≈ `a > tmp; b < tmp`), not concurrent.
This is a documented limitation and will surprise streaming/interactive/binary
pipelines.

## Testing

See `../test/` — a host build (`make`) compiles the core with `sh_port_host.c`
into `breezysh` (optionally ASan/UBSan), and `run_tests.py` drives the lifted
OSH/dash spec subset, classifying results as PASS / FAIL / ERROR(crash|hang).
The bar: FAIL and ERROR at zero.
