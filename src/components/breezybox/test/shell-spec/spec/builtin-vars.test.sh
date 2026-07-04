## oils_failures_allowed: 1
## compare_shells: dash bash-4.4 mksh zsh

# Tests for builtins having to do with variables: export, readonly, unset, etc.
#
# Also see assign.test.sh.

#### Export sets a global variable
# Even after you do export -n, it still exists.
f() { export GLOBAL=X; }
f
echo $GLOBAL
printenv.py GLOBAL
## STDOUT:
X
X
## END

#### Export a global variable and unset it
f() { export GLOBAL=X; }
f
echo $GLOBAL
printenv.py GLOBAL
unset GLOBAL
echo g=$GLOBAL
printenv.py GLOBAL
## STDOUT: 
X
X
g=
None
## END

#### Export existing global variables
G1=g1
G2=g2
export G1 G2
printenv.py G1 G2
## STDOUT: 
g1
g2
## END

#### Export existing local variable
f() {
  local L1=local1
  export L1
  printenv.py L1
}
f
printenv.py L1
## STDOUT: 
local1
None
## END

#### Export a variable before defining it
export U
U=u
printenv.py U
## stdout: u

#### Unset exported variable, then define it again.  It's NOT still exported.
export U
U=u
printenv.py U
unset U
printenv.py U
U=newvalue
echo $U
printenv.py U
## STDOUT:
u
None
newvalue
None
## END

#### Exporting a parent func variable (dynamic scope)
# The algorithm is to walk up the stack and export that one.
inner() {
  export outer_var
  echo "inner: $outer_var"
  printenv.py outer_var
}
outer() {
  local outer_var=X
  echo "before inner"
  printenv.py outer_var
  inner
  echo "after inner"
  printenv.py outer_var
}
outer
## STDOUT:
before inner
None
inner: X
X
after inner
X
## END

#### Dependent export setting
# FOO is not respected here either.
export FOO=foo v=$(printenv.py FOO)
echo "v=$v"
## stdout: v=None

#### Exporting a variable doesn't change it
old=$PATH
export PATH
new=$PATH
test "$old" = "$new" && echo "not changed"
## stdout: not changed

#### can't export associative array (strict_array)
shopt -s strict_array

typeset -A a
a["foo"]=bar

export a
printenv.py a
## STDOUT:
None
## END
## OK mksh status: 1
## OK mksh stdout-json: ""
## OK osh status: 1
## OK osh stdout-json: ""

#### Unset a variable
foo=bar
echo foo=$foo
unset foo
echo foo=$foo
## STDOUT:
foo=bar
foo=
## END

#### Unset exit status
V=123
unset V
echo status=$?
## stdout: status=0

#### Unset nonexistent variable
unset ZZZ
echo status=$?
## stdout: status=0

#### Unset has dynamic scope
f() {
  unset foo
}
foo=bar
echo foo=$foo
f
echo foo=$foo
## STDOUT:
foo=bar
foo=
## END

#### Unset and scope (bug #653)
unlocal() { unset "$@"; }

level2() {
  local hello=yy

  echo level2=$hello
  unlocal hello
  echo level2=$hello
}

level1() {
  local hello=xx

  level2

  echo level1=$hello
  unlocal hello
  echo level1=$hello

  level2
}

hello=global
level1

# bash, mksh, yash agree here.
## STDOUT:
level2=yy
level2=xx
level1=xx
level1=global
level2=yy
level2=global
## END
## OK dash/ash/zsh STDOUT:
level2=yy
level2=
level1=xx
level1=
level2=yy
level2=
## END

#### unset of local reveals variable in higher scope

# OSH has a RARE behavior here (matching yash and mksh), but at least it's
# consistent.

x=global
f() {
  local x=foo
  echo x=$x
  unset x
  echo x=$x
}
f
## STDOUT:
x=foo
x=global
## END
## OK dash/bash/zsh/ash STDOUT:
x=foo
x=
## END

#### Unset invalid variable name
unset %
echo status=$?
## STDOUT:
status=2
## END
## OK bash/mksh STDOUT:
status=1
## END
## BUG zsh STDOUT:
status=0
## END
# dash does a hard failure!
## OK dash stdout-json: ""
## OK dash status: 2

#### Unset nonexistent variable
unset _nonexistent__
echo status=$?
## STDOUT:
status=0
## END

#### Unset -v
foo() {
  echo "function foo"
}
foo=bar
unset -v foo
echo foo=$foo
foo
## STDOUT: 
foo=
function foo
## END

#### Unset -f
foo() {
  echo "function foo"
}
foo=bar
unset -f foo
echo foo=$foo
foo
echo status=$?
## STDOUT: 
foo=bar
status=127
## END

#### Use local twice
f() {
  local foo=bar
  local foo
  echo $foo
}
f
## stdout: bar
## BUG zsh STDOUT:
foo=bar
bar
## END

#### Local without variable is still unset!
set -o nounset
f() {
  local foo
  echo "[$foo]"
}
f
## stdout-json: ""
## status: 1
## OK dash status: 2
# zsh doesn't support nounset?
## BUG zsh stdout: []
## BUG zsh status: 0

