## oils_failures_allowed: 0
## compare_shells: bash dash mksh

# Test numbers bigger than 255 (2^8 - 1) and bigger than 2^31 - 1
# Shells differ in their behavior here.  bash silently converts.

# I think we should implement the "unstrict" but deterministic bash behavior
# for compatibility, and then add shopt -s strict_status if we need it.

#### Truncating 'exit' status

$SH -c 'exit 255'
echo status=$?

$SH -c 'exit 256'
echo status=$?

$SH -c 'exit 257'
echo status=$?

echo ===

$SH -c 'exit -1'
echo status=$?

$SH -c 'exit -2'
echo status=$?

## STDOUT:
status=255
status=0
status=1
===
status=255
status=254
## END
## OK dash STDOUT:
status=255
status=0
status=1
===
status=2
status=2
## END

#### Truncating 'return' status
f() { return 255; }; f
echo status=$?

f() { return 256; }; f
echo status=$?

f() { return 257; }; f
echo status=$?

echo ===

f() { return -1; }; f
echo status=$?

f() { return -2; }; f
echo status=$?

## STDOUT:
status=255
status=0
status=1
===
status=255
status=254
## END

# dash aborts on bad exit code
## OK dash status: 2
## OK dash STDOUT:
status=255
status=256
status=257
===
## END


#### If empty command
if ''; then echo TRUE; else echo FALSE; fi
## stdout: FALSE
## status: 0

#### If subshell true
if `true`; then echo TRUE; else echo FALSE; fi
## stdout: TRUE
## status: 0

#### If subshell true WITH OUTPUT is different
if `sh -c 'echo X; true'`; then echo TRUE; else echo FALSE; fi
## stdout: FALSE
## status: 0

#### If subshell true WITH ARGUMENT
if `true` X; then echo TRUE; else echo FALSE; fi
## stdout: FALSE
## status: 0

#### If subshell false -- exit code is propagated in a weird way (strict_argv prevents)
if `false`; then echo TRUE; else echo FALSE; fi
## stdout: FALSE
## status: 0

#### Exit code when command sub evaluates to empty str, e.g. `false` (#2416)

# OSH had a bug here
`true`; echo $?
`false`; echo $?
$(true); echo $?
$(false); echo $?
echo ---

# OSH and others agree on these
eval true; echo $?
eval false; echo $?
`echo true`; echo $?
`echo false`; echo $?
## STDOUT:
0
1
0
1
---
0
1
0
1
## END

#### More test cases with empty argv

true $(false)
echo status=$?

$(exit 42)
echo status=$?

$(exit 42) $(exit 43)
echo status=$?

## STDOUT:
status=0
status=42
status=43
## END
