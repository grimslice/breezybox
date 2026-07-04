## oils_failures_allowed: 0
## compare_shells: dash bash mksh

#### zero args: [ ]
[ ] || echo false
## stdout: false

#### one arg: [ x ] where x is one of '=' '!' '(' ']'
[ = ]
echo status=$?
[ ] ]
echo status=$?
[ '!' ]
echo status=$?
[ '(' ]
echo status=$?
## STDOUT: 
status=0
status=0
status=0
status=0
## END

#### one arg: empty string is false.  Equivalent to -n.
test 'a'  && echo true
test ''   || echo false
## STDOUT:
true
false
## END

#### two args: -z with = ! ( ]
[ -z = ]
echo status=$?
[ -z ] ]
echo status=$?
[ -z '!' ]
echo status=$?
[ -z '(' ]
echo status=$?
## STDOUT: 
status=1
status=1
status=1
status=1
## END

#### three args
[ foo = '' ]
echo status=$?
[ foo -a '' ]
echo status=$?
[ foo -o '' ]
echo status=$?
[ ! -z foo ]
echo status=$?
[ \( foo \) ]
echo status=$?
## STDOUT: 
status=1
status=1
status=0
status=0
status=0
## END

#### four args
[ ! foo = foo ]
echo status=$?
[ \( -z foo \) ]
echo status=$?
## STDOUT: 
status=1
status=1
## END

#### test with extra args is syntax error
test -n x ]
echo status=$?
test -n x y
echo status=$?
## STDOUT:
status=2
status=2
## END

#### ] syntax errors
[
echo status=$?
test  # not a syntax error
echo status=$?
[ -n x  # missing ]
echo status=$?
[ -n x ] y  # extra arg after ]
echo status=$?
[ -n x y  # extra arg
echo status=$?
## STDOUT:
status=2
status=1
status=2
status=2
status=2
## END

#### -n
test -n 'a'  && echo true
test -n ''   || echo false
## STDOUT:
true
false
## END

#### ! -a
[ -z '' -a ! -z x ]
echo status=$?
## stdout: status=0

#### -o
[ -z x -o ! -z x ]
echo status=$?
## stdout: status=0

#### ( )
[ -z '' -a '(' ! -z x ')' ]
echo status=$?
## stdout: status=0

#### ( ) ! -a -o with system version of [
command [ --version
command [ -z '' -a '(' ! -z x ')' ] && echo true
## stdout: true

#### == is alias for =
[ a = a ] && echo true
[ a == a ] && echo true
## STDOUT: 
true
true
## END
## BUG dash STDOUT: 
true
## END
## BUG dash status: 2

#### [ with op variable
# OK -- parsed AFTER evaluation of vars
op='='
[ a $op a ] && echo true
[ a $op b ] || echo false
## status: 0
## STDOUT:
true
false
## END

#### [ with unquoted empty var
empty=''
[ $empty = '' ] && echo true
## status: 2

#### [ compare with literal -f
# Hm this is the same
var=-f
[ $var = -f ] && echo true
[ '-f' = $var ] && echo true
## STDOUT:
true
true
## END

#### [ '(' foo ] is runtime syntax error
[ '(' foo ]
echo status=$?
## stdout: status=2

#### -z '>' implies two token lookahead
[ -z ] && echo true  # -z is operand
[ -z '>' ] || echo false  # -z is operator
[ -z '>' -- ] && echo true  # -z is operand
## STDOUT:
true
false
true
## END

#### operator/operand ambiguity with ]
# bash parses this as '-z' AND ']', which is true.  It's a syntax error in
# dash/mksh.
[ -z -a ] ]
echo status=$?
## stdout: status=0
## OK mksh stdout: status=2
## OK dash stdout: status=2

#### operator/operand ambiguity with -a
# bash parses it as '-z' AND '-a'.  It's a syntax error in mksh but somehow a
# runtime error in dash.
[ -z -a -a ]
echo status=$?
## stdout: status=0
## OK mksh stdout: status=2
## OK dash stdout: status=1

#### -d
test -d $TMP
echo status=$?
test -d $TMP/__nonexistent_Z_Z__
echo status=$?
## STDOUT:
status=0
status=1
## END

#### -x
rm -f $TMP/x
echo 'echo hi' > $TMP/x
test -x $TMP/x || echo 'no'
chmod +x $TMP/x
test -x $TMP/x && echo 'yes'
test -x $TMP/__nonexistent__ || echo 'bad'
## STDOUT:
no
yes
bad
## END

#### -r
echo '1' > $TMP/testr_yes
echo '2' > $TMP/testr_no
chmod -r $TMP/testr_no  # remove read permission
test -r $TMP/testr_yes && echo 'yes'
test -r $TMP/testr_no || echo 'no'
## STDOUT:
yes
no
## END

#### -w
rm -f $TMP/testw_*
echo '1' > $TMP/testw_yes
echo '2' > $TMP/testw_no
chmod -w $TMP/testw_no  # remove write permission
test -w $TMP/testw_yes && echo 'yes'
test -w $TMP/testw_no || echo 'no'
## STDOUT:
yes
no
## END

#### [ a -eq b ]
[ a -eq a ]
echo status=$?
## STDOUT:
status=2
## END
## BUG mksh STDOUT:
status=0
## END

#### test -s
test -s __nonexistent
echo status=$?
touch $TMP/empty
test -s $TMP/empty
echo status=$?
echo nonempty > $TMP/nonempty
test -s $TMP/nonempty
echo status=$?
## STDOUT:
status=1
status=1
status=0
## END

#### -nt -ot
[ present -nt absent ] || exit 1
[ absent -ot present ] || exit 2
## status: 1

#### Overflow error
test -t 12345678910
echo status=$?
## STDOUT:
status=2
## END
## OK dash/bash STDOUT:
status=1
## END

#### Bug regression
test "$ipv6" = "yes" -a "$ipv6lib" != "none"
echo status=$?
## STDOUT:
status=1
## END


#### bug from pnut: negative number $((-1))

# https://lobste.rs/s/lplim1/design_self_compiling_c_transpiler#c_km2ywc

[ $((-42)) -le 0 ]
echo status=$?

[ $((-1)) -le 0 ]
echo status=$?

echo

[ -1 -le 0 ]
echo status=$?

[ -42 -le 0 ]
echo status=$?

echo

test -1 -le 0
echo status=$?

test -42 -le 0
echo status=$?

## STDOUT:
status=0
status=0

status=0
status=0

status=0
status=0
## END

#### negative octal numbers, etc.

# zero
[ -0 -eq 0 ]
echo zero=$?

# octal numbers can be negative
[ -0123 -eq -83 ]
echo octal=$?

# hex doesn't have negative numbers?
[ -0xff -eq -255 ]
echo hex=$?

# base N doesn't either
[ -64#a -eq -10 ]
echo baseN=$?

## STDOUT:
zero=0
octal=1
hex=2
baseN=2
## END

#### No octal, hex, base N conversion - leading 0 is a regular decimal

# arithmetic has octal conversion
echo $(( 073 ))
echo $(( -073 ))

echo

# Bracket does NOT have octal conversion!  That is annoying.
[ 073 -eq 73 ]
echo status=$?

[ -073 -eq -73 ]
echo status=$?

echo

[ 0xff -eq 255 ]
echo hex=$?
[ 64#a -eq 10 ]
echo baseN=$?

## STDOUT:
59
-59

status=0
status=0

hex=2
baseN=2
## END

## BUG mksh STDOUT:
73
-73

status=0
status=0

hex=2
baseN=2
## END

#### Looks like octal, but digit is too big

# arithmetic has octal conversion
echo $(( 083 ))
echo status=$?

echo $(( -083 ))
echo status=$?

echo

# Bracket does NOT have octal conversion!  That is annoying.
[ 083 -eq 83 ]
echo status=$?

[ -083 -eq -83 ]
echo status=$?

## status: 1
## STDOUT:
## END

## OK dash status: 2

## OK bash status: 0
## OK bash STDOUT:
status=1
status=1

status=0
status=0
## END

## OK mksh status: 0
## OK mksh STDOUT:
83
status=0
-83
status=0

status=0
status=0
## END

#### no recursive arith [ 1+2 -eq 3 ]

[ 1+2 -eq 3 ]
echo status=$?

s='1+2'
[ "$s" -eq 3 ]
echo status=$?

## STDOUT:
status=2
status=2
## END

## BUG mksh STDOUT:
status=0
status=0
## END
