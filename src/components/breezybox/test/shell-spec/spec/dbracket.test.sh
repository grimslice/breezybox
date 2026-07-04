## oils_failures_allowed: 0
## compare_shells: bash-4.4 mksh

# NOTE: zsh passes about half, and fails about half.  It supports a subset of
# [[ I guess.

#### && chain
[[ t && t && '' ]] || echo false
## stdout: false

#### precedence of && and || in a command context
if test True || test '' && test ''; then
  echo YES
else
  echo "NO precedence"
fi
## stdout: NO precedence

# http://tldp.org/LDP/abs/html/testconstructs.html#DBLBRACKETS

#### [[ at runtime doesn't work
dbracket=[[
$dbracket foo == foo ]]
## status: 127

#### [[ with env prefix doesn't work
FOO=bar [[ foo == foo ]]
## status: 127

#### Argument that looks like a real operator
[[ -f < ]] && echo 'should be parse error'
## status: 2
## OK mksh status: 1

#### [[ -z '>' ]]
[[ -z '>' ]] || echo false  # -z is operator
## stdout: false

#### test whether ']]' is empty
[[ ']]' ]]
echo status=$?
## status: 0

