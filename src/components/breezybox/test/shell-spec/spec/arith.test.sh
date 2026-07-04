## compare_shells: bash dash mksh zsh


# Interesting interpretation of constants.
#
# "Constants with a leading 0 are interpreted as octal numbers. A leading ‘0x’
# or ‘0X’ denotes hexadecimal. Otherwise, numbers take the form [base#]n, where
# the optional base is a decimal number between 2 and 64 representing the
# arithmetic base, and n is a number in that base. If base# is omitted, then
# base 10 is used. When specifying n, the digits greater than 9 are represented
# by the lowercase letters, the uppercase letters, ‘@’, and ‘_’, in that order.
# If base is less than or equal to 36, lowercase and uppercase letters may be
# used interchangeably to represent numbers between 10 and 35. "
# 
# NOTE $(( 8#9 )) can fail, and this can be done at parse time...

#### Add one to var
i=1
echo $(($i+1))
## stdout: 2

#### $ is optional
i=1
echo $((i+1))
## stdout: 2

#### SimpleVarSub within arith
j=0
echo $(($j + 42))
## stdout: 42

#### BracedVarSub within ArithSub
echo $((${j:-5} + 1))
## stdout: 6

#### Arith word part
foo=1; echo $((foo+1))bar$(($foo+1))
## stdout: 2bar2

#### Arith sub with word parts
# Making 13 from two different kinds of sub.  Geez.
echo $((1 + $(echo 1)${undefined:-3}))
## stdout: 14

#### Arith sub within arith sub
# This is unnecessary but works in all shells.
echo $((1 + $((2 + 3)) + 4))
## stdout: 10

#### Backticks within arith sub
# This is unnecessary but works in all shells.
echo $((`echo 1` + 2))
## stdout: 3

#### Integer constant validation
check() {
  $SH -c "shopt --set strict_arith; echo $1"
  echo status=$?
}

check '$(( 0x1X ))'
check '$(( 09 ))'
check '$(( 2#A ))'
check '$(( 02#0110 ))'
## STDOUT:
status=1
status=1
status=1
status=1
## END

## OK dash STDOUT:
status=2
status=2
status=2
status=2
## END

## BUG zsh STDOUT:
status=1
9
status=0
status=1
6
status=0
## END

## BUG mksh STDOUT:
status=1
9
status=0
status=1
6
status=0
## END

#### Newline in the middle of expression
echo $((1
+ 2))
## stdout: 3

#### Increment undefined variables with nounset
set -o nounset
(( undef1++ ))
(( ++undef2 ))
echo "[$undef1][$undef2]"
## stdout-json: ""
## status: 1
## OK dash status: 2
## BUG mksh/zsh status: 0
## BUG mksh/zsh STDOUT:
[1][1]
## END

#### Augmented assignment
a=4
echo $((a+=1))
echo $a
## STDOUT:
5
5
## END

#### Comparison Ops
echo $(( 1 == 1 ))
echo $(( 1 != 1 ))
echo $(( 1 < 1 ))
echo $(( 1 <= 1 ))
echo $(( 1 > 1 ))
echo $(( 1 >= 1 ))
## STDOUT:
1
0
0
1
0
1
## END

#### Logical Ops
echo $((1 || 2))
echo $((1 && 2))
echo $((!(1 || 2)))
## STDOUT:
1
1
0
## END

#### Unary minus and plus
a=1
b=3
echo $((- a + + b))
## STDOUT:
2
## END

#### No floating point
echo $((1 + 2.3))
## status: 2
## OK bash/mksh status: 1
## BUG zsh status: 0

#### Octal constant
echo $(( 011 ))
## stdout: 9
## OK mksh/zsh stdout: 11

#### Dynamic octal constant
zero=0
echo $(( ${zero}11 ))
## stdout: 9
## OK mksh/zsh stdout: 11

#### Dynamic hex constants
zero=0
echo $(( ${zero}xAB ))
## stdout: 171

#### Hex constant with capital X
echo $(( 0XAA ))
## stdout: 170

#### Dynamic var names - result of runtime parse/eval
foo=5
x=oo
echo $(( foo + f$x + 1 ))
## stdout: 11

#### nounset with arithmetic
set -o nounset
x=$(( y + 5 ))
echo "should not get here: x=${x:-<unset>}"
## stdout-json: ""
## status: 1
## BUG dash/mksh/zsh stdout: should not get here: x=5
## BUG dash/mksh/zsh status: 0

#### Operator Precedence
echo $(( 1 + 2*3 - 8/2 ))
## stdout: 3

#### Comment not allowed in the middle of multiline arithmetic
echo $((
1 +
2 + \
3
))
echo $((
1 + 2  # not a comment
))
(( a = 3 + 4  # comment
))
echo [$a]
## status: 1
## STDOUT:
6
## END
## OK dash/osh status: 2
## OK bash STDOUT:
6
[]
## END
## OK bash status: 0

#### Add integer to associative array (a[0] decay)
typeset -A assoc
assoc[0]=42
echo $((assoc + 5))
## status: 0
## stdout: 47
## BUG dash status: 0
## BUG dash stdout: 5

#### assignment with dynamic var name
foo=bar
echo $(( x$foo = 42 ))
echo xbar=$xbar
## STDOUT:
42
xbar=42
## END

#### unary assignment with dynamic var name
foo=bar
xbar=42
echo $(( x$foo++ ))
echo xbar=$xbar
## STDOUT:
42
xbar=43
## END
## BUG dash status: 2
## BUG dash stdout-json: ""

#### Dynamic parsing on empty string
a=''
echo $(( a ))

a2=' '
echo $(( a2 ))
## STDOUT:
0
0
## END
 
#### Invalid constant

echo $((a + x42))
echo status=$?

# weird asymmetry -- the above is a syntax error, but this isn't
$SH -c 'echo $((a + 42x))'
echo status=$?

# regression
echo $((a + 42x))
echo status=$?
## status: 1
## STDOUT:
0
status=0
status=1
## END
## OK dash status: 2
## OK dash STDOUT:
0
status=0
status=2
## END
## BUG bash status: 0
## BUG bash STDOUT:
0
status=0
status=1
status=1
## END

#### Negative numbers with integer division /

echo $(( 10 / 3))
echo $((-10 / 3))
echo $(( 10 / -3))
echo $((-10 / -3))

echo ---

a=20
: $(( a /= 3 ))
echo $a

a=-20
: $(( a /= 3 ))
echo $a

a=20
: $(( a /= -3 ))
echo $a

a=-20
: $(( a /= -3 ))
echo $a

## STDOUT:
3
-3
-3
3
---
6
-6
-6
6
## END

#### Negative numbers with %

echo $(( 10 % 3))
echo $((-10 % 3))
echo $(( 10 % -3))
echo $((-10 % -3))

## STDOUT:
1
-1
1
-1
## END

#### s[0] with string '12 34'

s='12 34'
echo '12 34' $(( s[0] )) $(( s[1] ))
echo status=$?

## status: 1
## STDOUT:
## END

## OK dash status: 2

## BUG zsh status: 0
## BUG zsh STDOUT:
12 34 0 1
status=0
## END

# bash prints an error, but doesn't fail

## BUG bash status: 0
## BUG bash STDOUT:
status=1
## END
