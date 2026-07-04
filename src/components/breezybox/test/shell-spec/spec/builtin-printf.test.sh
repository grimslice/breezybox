## oils_failures_allowed: 1
## compare_shells: dash bash mksh zsh ash

# printf
# bash-completion uses this odd printf -v construction.  It seems to mostly use
# %s and %q though.
#
# %s should just be
# declare $var='val'
#
# NOTE: 
# /usr/bin/printf %q "'" seems wrong.
# $ /usr/bin/printf  %q "'"
# ''\'''
#
# I suppose it is technically correct, but it looks very ugly.

#### printf with too few arguments
printf -- '-%s-%s-%s-\n' 'a b' 'x y'
## STDOUT:
-a b-x y--
## END

#### printf with too many arguments
printf -- '-%s-%s-\n' a b c d e
## STDOUT:
-a-b-
-c-d-
-e--
## END

#### printf width strings
printf '[%5s]\n' abc
printf '[%-5s]\n' abc
## STDOUT:
[  abc]
[abc  ]
## END

#### printf integer
printf '%d\n' 42
printf '%i\n' 42  # synonym
printf '%d\n' \'a # if first character is a quote, use character code
printf '%d\n' \"a # double quotes work too
printf '[%5d]\n' 42
printf '[%-5d]\n' 42
printf '[%05d]\n' 42
#printf '[%-05d]\n' 42  # the leading 0 is meaningless
#[42   ]
## STDOUT:
42
42
97
97
[   42]
[42   ]
[00042]
## END

#### printf %6.4d -- "precision" does padding for integers
printf '[%6.4d]\n' 42
printf '[%.4d]\n' 42
printf '[%6.d]\n' 42
echo --
printf '[%6.4d]\n' -42
printf '[%.4d]\n' -42
printf '[%6.d]\n' -42
## STDOUT:
[  0042]
[0042]
[    42]
--
[ -0042]
[-0042]
[   -42]
## END

#### printf %6.4x X o 
printf '[%6.4x]\n' 42
printf '[%.4x]\n' 42
printf '[%6.x]\n' 42
echo --
printf '[%6.4X]\n' 42
printf '[%.4X]\n' 42
printf '[%6.X]\n' 42
echo --
printf '[%6.4o]\n' 42
printf '[%.4o]\n' 42
printf '[%6.o]\n' 42
## STDOUT:
[  002a]
[002a]
[    2a]
--
[  002A]
[002A]
[    2A]
--
[  0052]
[0052]
[    52]
## END

#### %06d zero padding vs. %6.6d
printf '[%06d]\n' 42
printf '[%06d]\n' -42  # 6 TOTAL
echo --
printf '[%6.6d]\n' 42
printf '[%6.6d]\n' -42  # 6 + 1 for the - sign!!!
## STDOUT:
[000042]
[-00042]
--
[000042]
[-000042]
## END

#### %06x %06X %06o
printf '[%06x]\n' 42
printf '[%06X]\n' 42
printf '[%06o]\n' 42
## STDOUT:
[00002a]
[00002A]
[000052]
## END

#### printf %6.4s does both truncation and padding
printf '[%6s]\n' foo
printf '[%6.4s]\n' foo
printf '[%-6.4s]\n' foo
printf '[%6s]\n' spam-eggs
printf '[%6.4s]\n' spam-eggs
printf '[%-6.4s]\n' spam-eggs
## STDOUT:
[   foo]
[   foo]
[foo   ]
[spam-eggs]
[  spam]
[spam  ]
## END

#### printf %*.*s (width/precision from args)
printf '[%*s]\n' 9 hello
printf '[%.*s]\n' 3 hello
printf '[%*.3s]\n' 9 hello
printf '[%9.*s]\n' 3 hello
printf '[%*.*s]\n' 9 3 hello
## STDOUT:
[    hello]
[hel]
[      hel]
[      hel]
[      hel]
## END

#### unsigned / octal / hex
printf '[%u]\n' 42
printf '[%o]\n' 42
printf '[%x]\n' 42
printf '[%X]\n' 42
echo

printf '[%X]\n' \'a  # if first character is a quote, use character code
printf '[%X]\n' \'ab # extra chars ignored

## STDOUT:
[42]
[52]
[2a]
[2A]

[61]
[61]
## END

#### negative numbers with unsigned / octal / hex
printf '[%u]\n' -42
echo status=$?

printf '[%o]\n' -42
echo status=$?

printf '[%x]\n' -42
echo status=$?

printf '[%X]\n' -42
echo status=$?

## STDOUT:
[18446744073709551574]
status=0
[1777777777777777777726]
status=0
[ffffffffffffffd6]
status=0
[FFFFFFFFFFFFFFD6]
status=0
## END

# osh DISALLOWS this because the output depends on the machine architecture.
## OK osh STDOUT:
status=1
status=1
status=1
status=1
## END

#### printf floating point (not required, but they all implement it)
printf '[%f]\n' 3.14159
printf '[%.2f]\n' 3.14159
printf '[%8.2f]\n' 3.14159
printf '[%-8.2f]\n' 3.14159
printf '[%-f]\n' 3.14159
printf '[%-f]\n' 3.14
## STDOUT:
[3.141590]
[3.14]
[    3.14]
[3.14    ]
[3.141590]
[3.140000]
## END
## OK osh stdout-json: ""
## OK osh status: 2

#### printf floating point with - and 0
printf '[%8.4f]\n' 3.14
printf '[%08.4f]\n' 3.14
printf '[%8.04f]\n' 3.14  # meaning less 0
printf '[%08.04f]\n' 3.14
echo ---
# these all boil down to the same thing.  The -, 8, and 4 are respected, but
# none of the 0 are.
printf '[%-8.4f]\n' 3.14
printf '[%-08.4f]\n' 3.14
printf '[%-8.04f]\n' 3.14
printf '[%-08.04f]\n' 3.14
## STDOUT:
[  3.1400]
[003.1400]
[  3.1400]
[003.1400]
---
[3.1400  ]
[3.1400  ]
[3.1400  ]
[3.1400  ]
## END
## OK osh STDOUT:
---
## END
## OK osh status: 2

#### printf eE fF gG
printf '[%e]\n' 3.14
printf '[%E]\n' 3.14
printf '[%f]\n' 3.14
# bash is the only one that implements %F?  Is it a synonym?
#printf '[%F]\n' 3.14
printf '[%g]\n' 3.14
printf '[%G]\n' 3.14
## STDOUT:
[3.140000e+00]
[3.140000E+00]
[3.140000]
[3.14]
[3.14]
## END
## OK osh stdout-json: ""
## OK osh status: 2

#### printf invalid backslash escape (is ignored)
printf '[\Z]\n'
## STDOUT:
[\Z]
## END

#### printf % escapes
printf '[%%]\n'
## STDOUT:
[%]
## END

#### printf %c ASCII

printf '%c\n' a
printf '%c\n' ABC
printf '%cZ\n' ABC

## STDOUT:
a
A
AZ
## END

#### printf negative numbers
printf '[%d] ' -42
echo status=$?
printf '[%i] ' -42
echo status=$?

# extra LEADING space too
printf '[%d] ' ' -42'
echo status=$?
printf '[%i] ' ' -42'
echo status=$?

# extra TRAILING space too
printf '[%d] ' ' -42 '
echo status=$?
printf '[%i] ' ' -42 '
echo status=$?

# extra TRAILING chars
printf '[%d] ' ' -42z'
echo status=$?
printf '[%i] ' ' -42z'
echo status=$?

exit 0  # ok

## STDOUT:
[-42] status=0
[-42] status=0
[-42] status=0
[-42] status=0
[-42] status=1
[-42] status=1
[-42] status=1
[-42] status=1
## END
# zsh is LESS STRICT
## OK zsh STDOUT:
[-42] status=0
[-42] status=0
[-42] status=0
[-42] status=0
[-42] status=0
[-42] status=0
[0] status=1
[0] status=1
## END

# osh is like zsh but has a hard failure (TODO: could be an option?)
## OK osh STDOUT:
[-42] status=0
[-42] status=0
[-42] status=0
[-42] status=0
[-42] status=0
[-42] status=0
status=1
status=1
## END

# ash is MORE STRICT
## OK ash STDOUT:
[-42] status=0
[-42] status=0
[-42] status=0
[-42] status=0
[0] status=1
[0] status=1
[0] status=1
[0] status=1
## END


#### printf + and space flags
# I didn't know these existed -- I only knew about - and 0 !
printf '[%+d]\n' 42
printf '[%+d]\n' -42
printf '[% d]\n' 42
printf '[% d]\n' -42
## STDOUT:
[+42]
[-42]
[ 42]
[-42]
## END
## OK osh stdout-json: ""
## OK osh status: 2

#### printf # flag
# I didn't know these existed -- I only knew about - and 0 !
# Note: '#' flag for integers outputs a prefix ONLY WHEN the value is non-zero
printf '[%#o][%#o]\n' 0 42
printf '[%#x][%#x]\n' 0 42
printf '[%#X][%#X]\n' 0 42
echo ---
# Note: '#' flag for %f, %g always outputs the decimal point.
printf '[%.0f][%#.0f]\n' 3 3
# Note: In addition, '#' flag for %g does not omit zeroes in fraction
printf '[%g][%#g]\n' 3 3
## STDOUT:
[0][052]
[0][0x2a]
[0][0X2A]
---
[3][3.]
[3][3.00000]
## END
## OK osh STDOUT:
---
## END
## OK osh status: 2

#### Runtime error for invalid integer
x=3abc
printf '%d\n' $x
echo status=$?
printf '%d\n' xyz
echo status=$?
## STDOUT:
3
status=1
0
status=1
## END
# zsh should exit 1 in both cases
## BUG zsh STDOUT:
0
status=1
0
status=0
## END
# fails but also prints 0 instead of 3abc
## BUG ash STDOUT:
0
status=1
0
status=1
## END
# osh doesn't print anything invalid
## OK osh STDOUT:
status=1
status=1
## END

#### Regression for 'printf x y'
printf x y
printf '%s\n' z
## STDOUT:
xz
## END

#### printf positive integer overflow

# %i seems like a synonym for %d

for fmt in '%u\n' '%d\n'; do
  # bash considers this in range for %u
  # same with mksh
  # zsh cuts everything off after 19 digits
  # ash truncates everything
  printf "$fmt" '18446744073709551615'
  echo status=$?
  printf "$fmt" '18446744073709551616'
  echo status=$?
  echo
done

## STDOUT:
status=1
status=1

status=1
status=1

## END

## OK bash status: 0
## OK bash STDOUT:
18446744073709551615
status=0
18446744073709551615
status=0

9223372036854775807
status=0
9223372036854775807
status=0

## END

## OK dash/mksh status: 0
## OK dash/mksh STDOUT:
18446744073709551615
status=0
18446744073709551615
status=1

9223372036854775807
status=1
9223372036854775807
status=1

## END

## BUG ash status: 0
## BUG ash STDOUT:
18446744073709551615
status=0
0
status=1

0
status=1
0
status=1

## END

## BUG zsh status: 0
## BUG zsh STDOUT:
1844674407370955161
status=0
1844674407370955161
status=0

1844674407370955161
status=0
1844674407370955161
status=0

## END

#### printf negative integer overflow

# %i seems like a synonym for %d

for fmt in '%u\n' '%d\n'; do

  printf "$fmt" '-18446744073709551615'
  echo status=$?
  printf "$fmt" '-18446744073709551616'
  echo status=$?
  echo
done

## STDOUT:
status=1
status=1

status=1
status=1

## END

## OK bash status: 0
## OK bash STDOUT:
1
status=0
18446744073709551615
status=0

-9223372036854775808
status=0
-9223372036854775808
status=0

## END

## OK dash/mksh status: 0
## OK dash/mksh STDOUT:
1
status=0
18446744073709551615
status=1

-9223372036854775808
status=1
-9223372036854775808
status=1

## END

## BUG zsh status: 0
## BUG zsh STDOUT:
16602069666338596455
status=0
16602069666338596455
status=0

-1844674407370955161
status=0
-1844674407370955161
status=0

## END

## BUG ash status: 0
## BUG ash STDOUT:
0
status=1
0
status=1

0
status=1
0
status=1

## END

#### printf %b respects \c early return
printf '[%b]\n' 'ab\ncd\cxy'
echo $?
## STDOUT:
[ab
cd0
## END


#### printf %b supports octal escapes, both \141 and \0141

printf 'three %b\n' '\141'  # di
printf 'four  %b\n' '\0141'
echo

# trailing 9
printf '%b\n' '\1419'
printf '%b\n' '\01419'

# Notes:
#
# - echo -e: 
#   - NO  3 digit octal  - echo -e '\141' does not work
#   - YES 4 digit octal
# - printf %b
#   - YES 3 digit octal
#   - YES 4 digit octal
# - printf string (outer)
#   - YES 3 digit octal
#   - NO  4 digit octal
# - $'' and $PS1
#   - YES 3 digit octal
#   - NO  4 digit octal

## STDOUT:
three a
four  a

a9
a9
## END

## OK zsh STDOUT:
three \141
four  a

\1419
a9
## END

#### printf %b with truncated octal escapes

# 8 is not a valid octal digit

printf '%b\n' '\558'
printf '%b\n' '\0558'
echo

show_bytes() {
  od -A n -t x1
}
printf '%b' '\7' | show_bytes
printf '%b' '\07' | show_bytes
printf '%b' '\007' | show_bytes
printf '%b' '\0007' | show_bytes

## STDOUT:
-8
-8

 07
 07
 07
 07
## END

## OK zsh STDOUT:
\558
-8

 5c 37
 07
 07
 07
## END

#### printf %d %X support hex 0x5 and octal 055

echo hex
printf '%d\n' 0x55
printf '%X\n' 0x55

echo hex CAPS
printf '%d\n' 0X55
printf '%X\n' 0X55

echo octal 3
printf '%d\n' 055
printf '%X\n' 055

echo octal 4
printf '%d\n' 0055
printf '%X\n' 0055

echo octal 5
printf '%d\n' 00055
printf '%X\n' 00055

## STDOUT:
hex
85
55
hex CAPS
85
55
octal 3
45
2D
octal 4
45
2D
octal 5
45
2D
## END

## BUG zsh STDOUT:
hex
85
55
hex CAPS
85
55
octal 3
55
37
octal 4
55
37
octal 5
55
37
## END

#### printf %d with + prefix (positive sign)

echo decimal
printf '%d\n' +42

echo octal
printf '%d\n' +077

echo hex lowercase
printf '%d\n' +0xab

echo hex uppercase
printf '%d\n' +0XAB

## STDOUT:
decimal
42
octal
63
hex lowercase
171
hex uppercase
171
## END

## BUG zsh STDOUT:
decimal
42
octal
77
hex lowercase
171
hex uppercase
171
## END

#### leading spaces are accepted in value given to %d %X, but not trailing spaces

case $SH in zsh) exit ;; esac

# leading space is allowed
printf '%d\n' ' -123'
echo status=$?
printf '%d\n' ' -123 '
echo status=$?

echo ---

printf '%d\n' ' +077'
echo status=$?

printf '%d\n' ' +0xff'
echo status=$?

printf '%X\n' ' +0xff'
echo status=$?

printf '%x\n' ' +0xff'
echo status=$?

## STDOUT:
-123
status=0
-123
status=1
---
63
status=0
255
status=0
FF
status=0
ff
status=0
## END

## OK osh STDOUT:
-123
status=0
status=1
---
63
status=0
255
status=0
FF
status=0
ff
status=0
## END

## BUG ash STDOUT:
-123
status=0
0
status=1
---
63
status=0
255
status=0
FF
status=0
ff
status=0
## END

## BUG-2 zsh STDOUT:
## END


#### Arbitrary base 64#a is rejected (unlike in shell arithmetic)

printf '%d\n' '64#a'
echo status=$?

# bash, dash, and mksh print 64 and return status 1
# zsh and ash print 0 and return status 1
# OSH rejects it completely (prints nothing) and returns status 1

## STDOUT:
status=1
## END

## OK dash/bash/mksh STDOUT:
64
status=1
## END

## OK zsh/ash STDOUT:
0
status=1
## END
