## compare_shells: bash
## oils_failures_allowed: 0

# Tests nocasematch matching

#### [ matching
shopt -s nocasematch
[ a = A ]; echo $?
[ A = a ]; echo $?
## STDOUT:
1
1
## END

#### file matching
shopt -s nocasematch
touch a B
echo [A] [b]
## STDOUT:
[A] [b]
## END

#### parameter expansion matching
shopt -s nocasematch
foo=a
bar=A
echo "${foo#A}" "${foo#[A]}"
echo "${bar#a}" "${bar#[a]}"
## STDOUT:
a a
A A
## END
