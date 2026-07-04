## tags: interactive
## compare_shells: bash dash mksh zsh
## oils_failures_allowed: 0

#### empty stdin
# had a bug here
echo -n '' | $SH
## stdout-json: ""
## status: 0

#### args that look like flags are passed after script
script=$TMP/sh1.sh
echo 'argv.py "$@"' > $script
chmod +x $script
$SH $script --help --help -h
## stdout: ['--help', '--help', '-h']

#### exit with explicit arg
exit 42
## status: 42

#### exit with no args
false
exit
## status: 1

#### sh -c with multiple -- args

# variant of above case

$SH -c -- -- 'echo two'
echo status=$?

$SH -c -- -- -- 'echo two'
echo status=$?

$SH -c -z 'echo z'
if test $? -ne 0; then
  echo 'z failed'
fi

## STDOUT:
status=127
status=127
z failed
## END

#### sh -c with no arg after --

# variant of above case

$SH -c --
if test $? -ne 0; then
  echo failed
fi

$SH -c -
if test $? -ne 0; then
  echo failed
fi

## STDOUT:
failed
failed
## END

