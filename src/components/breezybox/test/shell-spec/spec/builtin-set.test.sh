## compare_shells: bash dash mksh zsh 
## oils_failures_allowed: 0

#### set with both options and argv
set -o errexit a b c
echo "$@"
false
echo done
## stdout: a b c
## status: 1

#### nounset with "$@"
set a b c
set -u  # shouldn't touch argv
echo "$@"
## stdout: a b c

#### set -u -- clears argv
set a b c
set -u -- # shouldn't touch argv
echo "$@"
## stdout: 

#### set -u -- x y z
set a b c
set -u -- x y z
echo "$@"
## stdout: x y z

#### set -u with undefined variable exits the interpreter

# non-interactive
$SH -c 'set -u; echo before; echo $x; echo after'
if test $? -ne 0; then
  echo OK
fi

# interactive
$SH -i -c 'set -u; echo before; echo $x; echo after'
if test $? -ne 0; then
  echo OK
fi

## STDOUT:
before
OK
before
OK
## END

#### set -u with undefined var in interactive shell does NOT exit the interpreter

# In bash, it aborts the LINE only.  The next line is executed!

# non-interactive
$SH -c 'set -u; echo before; echo $x; echo after
echo line2
'
if test $? -ne 0; then
  echo OK
fi

# interactive
$SH -i -c 'set -u; echo before; echo $x; echo after
echo line2
'
if test $? -ne 0; then
  echo OK
fi

## STDOUT:
before
OK
before
line2
## END

## BUG dash/mksh/zsh STDOUT:
before
OK
before
OK
## END

#### set -u error can break out of nested evals
$SH -c '
set -u
test_function_2() {
  x=$blarg
}
test_function() {
  eval "test_function_2"
}

echo before
eval test_function
echo after
'
# status must be non-zero: bash uses 1, ash/dash exit 2
if test $? -ne 0; then
  echo OK
fi

## STDOUT:
before
OK
## END
## BUG zsh/mksh STDOUT:
before
after
## END

#### reset option with long flag
set -o errexit
set +o errexit
echo "[$unset]"
## stdout: []
## status: 0

#### reset option with short flag
set -u 
set +u
echo "[$unset]"
## stdout: []
## status: 0

#### set -eu (flag parsing)
set -eu 
echo "[$unset]"
echo status=$?
## stdout-json: ""
## status: 1
## OK dash status: 2

#### 'set' and 'eval' round trip

# NOTE: not testing arrays and associative arrays!
_space='[ ]'
_whitespace=$'[\t\r\n]'
_sq="'single quotes'"
_backslash_dq="\\ \""
_unicode=$'[\u03bc]'

# Save the variables
varfile=$TMP/vars-$(basename $SH).txt

set | grep '^_' > "$varfile"

# Unset variables
unset _space _whitespace _sq _backslash_dq _unicode
echo [ $_space $_whitespace $_sq $_backslash_dq $_unicode ]

# Restore them

. $varfile
echo "Code saved to $varfile" 1>&2  # for debugging

test "$_space" = '[ ]' && echo OK
test "$_whitespace" = $'[\t\r\n]' && echo OK
test "$_sq" = "'single quotes'" && echo OK
test "$_backslash_dq" = "\\ \"" && echo OK
test "$_unicode" = $'[\u03bc]' && echo OK

## STDOUT:
[ ]
OK
OK
OK
OK
OK
## END

## BUG zsh status: 1
## BUG zsh STDOUT:
[ ]
## END

#### set - - and so forth
set a b
echo "$@"

set - a b
echo "$@"

set -- a b
echo "$@"

set - -
echo "$@"

set -- --
echo "$@"

# note: zsh is different, and yash is totally different
## STDOUT:
a b
a b
a b
-
--
## END
## OK yash STDOUT:
a b
- a b
a b
- -
--
## END
## BUG zsh STDOUT:
a b
a b
a b

--
## END

#### set - stops option processing like set --
case $SH in zsh) exit ;; esac

show_options() {
  case $- in
    *v*) echo verbose-on ;;
  esac
  case $- in
    *x*) echo xtrace-on ;;
  esac
}

set -x - -v

show_options
echo argv "$@"

## STDOUT:
argv -v
## END

## OK zsh STDOUT:
## END

#### set - + and + -
set - +
echo "$@"

set + -
echo "$@"

## STDOUT:
+
+
## END

## BUG mksh STDOUT:
+
-
## END

## OK zsh/osh STDOUT:
+

## END

