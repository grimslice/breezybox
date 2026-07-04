## compare_shells: bash dash mksh zsh
## oils_failures_allowed: 0

# Note: zsh passes most of these tests too

#### Case statement
case a in
  a) echo A ;;
  *) echo star ;;
esac

for x in a b; do
  case $x in
    # the pattern is DYNAMIC and evaluated on every iteration
    $x) echo loop ;;
    *) echo star ;;
  esac
done
## STDOUT:
A
loop
loop
## END

#### Case with empty condition
case $empty in
  ''|foo) echo match ;;
  *) echo no ;;
esac
## stdout: match

#### Match a literal with a glob character
x='*.py'
case "$x" in
  '*.py') echo match ;;
esac
## stdout: match

#### Match a literal with a glob character with a dynamic pattern
x='b.py'
pat='[ab].py'
case "$x" in
  $pat) echo match ;;
esac
## stdout: match
## BUG zsh stdout-json: ""

#### Multiple Patterns Match
x=foo
result='-'
case "$x" in
  f*|*o) result="$result X"
esac
echo $result
## stdout: - X

#### matching the byte 0xff against empty string - DISABLED - CI only bug?

case $SH in *osh) echo soil-ci-buster-slim-bug; exit ;; esac

# This doesn't make a difference on my local machine?
# Is the underlying issue how libc fnmatch() respects Unicode?

#LC_ALL=C
#LC_ALL=C.UTF-8

c=$(printf \\377)

# OSH prints -1 here
#echo "${#c}"

case $c in
  '')   echo a ;;
  "$c") echo b ;;
esac

case "$c" in
  '')   echo a ;;
  "$c") echo b ;;
esac

## STDOUT:
b
b
## END

## OK osh STDOUT:
soil-ci-buster-slim-bug
## END

#### case \n bug regression

case
in esac

## STDOUT:
## END
## status: 2
## OK mksh status: 1
## OK zsh status: 127

