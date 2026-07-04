## oils_failures_allowed: 0
## compare_shells: dash bash-4.4 mksh zsh

#### Env value doesn't persist
FOO=foo printenv.py FOO
echo -$FOO-
## STDOUT:
foo
--
## END

#### Env value with equals
FOO=foo=foo printenv.py FOO
## stdout: foo=foo

#### Env binding can use preceding bindings, but not subsequent ones
# This means that for ASSIGNMENT_WORD, on the RHS you invoke the parser again!
# Could be any kind of quoted string.
FOO="foo" BAR="[$FOO][$BAZ]" BAZ=baz printenv.py FOO BAR BAZ
## STDOUT:
foo
[foo][]
baz
## BUG mksh STDOUT:
foo
[][]
baz
## END

#### Env value with two quotes
FOO='foo'"adjacent" printenv.py FOO
## stdout: fooadjacent

#### Env value with escaped <
FOO=foo\<foo printenv.py FOO
## stdout: foo<foo

#### FOO=foo echo [foo]
FOO=foo echo "[$foo]"
## stdout: []

#### FOO=foo fun
fun() {
  echo "[$FOO]"
}
FOO=foo fun
## stdout: [foo]

#### Escaped = in command name
# foo=bar is in the 'spec/bin' dir.
foo\=bar
## stdout: HI

#### Env binding not allowed before compound command
# bash gives exit code 2 for syntax error, because of 'do'.
# dash gives 0 because there is stuff after for?  Should really give an error.
# mksh gives acceptable error of 1.
FOO=bar for i in a b; do printenv.py $FOO; done
## status: 2
## OK mksh/zsh status: 1

#### Trying to run keyword 'for'
FOO=bar for
## status: 127
## OK zsh status: 1

#### Empty env binding
EMPTY= printenv.py EMPTY
## stdout:

#### Assignment doesn't do word splitting
words='one two'
a=$words
argv.py "$a"
## stdout: ['one two']

#### Assignment doesn't do glob expansion
touch _tmp/z.Z _tmp/zz.Z
a=_tmp/*.Z
argv.py "$a"
## stdout: ['_tmp/*.Z']

#### assignments / array assignments not interpreted after 'echo'
a=1 echo b[0]=2 c=3
## stdout: b[0]=2 c=3
# zsh interprets [0] as some kind of glob
## OK zsh stdout-json: ""
## OK zsh status: 1

#### dynamic local variables (and splitting)
f() {
  local "$1"  # Only x is assigned here
  echo x=\'$x\'
  echo a=\'$a\'

  local $1  # x and a are assigned here
  echo x=\'$x\'
  echo a=\'$a\'
}
f 'x=y a=b'
## OK dash/bash/mksh STDOUT:
x='y a=b'
a=''
x='y'
a='b'
## END
# osh and zsh don't do word splitting
## STDOUT:
x='y a=b'
a=''
x='y a=b'
a=''
## END

#### readonly x= gives empty string (regression)
readonly x=
argv.py "$x"
## STDOUT:
['']
## END

#### 'local x' does not set variable
set -o nounset
f() {
  local x
  echo $x
}
f
## status: 1
## OK dash status: 2
## BUG zsh status: 0

#### 'local -a x' does not set variable
set -o nounset
f() {
  local -a x
  echo $x
}
f
## status: 1
## OK dash status: 2
## BUG zsh status: 0

#### declare in an if statement
# bug caught by my feature detection snippet in bash-completion
if ! foo=bar; then
  echo BAD
fi
echo $foo
if ! eval 'spam=eggs'; then
  echo BAD
fi
echo $spam
## STDOUT:
bar
eggs
## END


#### Modify a temporary binding
# (regression for bug found by Michael Greenberg)
f() {
  echo "x before = $x"
  x=$((x+1))
  echo "x after  = $x"
}
x=5 f
## STDOUT:
x before = 5
x after  = 6
## END

#### Reveal existence of "temp frame" (All shells disagree here!!!)
f() {
  echo "x=$x"

  x=mutated-temp  # mutate temp frame
  echo "x=$x"

  # Declare a new local
  local x='local'
  echo "x=$x"

  # Unset it
  unset x
  echo "x=$x"
}

x=global
x=temp-binding f
echo "x=$x"

## STDOUT:
x=temp-binding
x=mutated-temp
x=local
x=mutated-temp
x=global
## END
## OK dash/zsh STDOUT:
x=temp-binding
x=mutated-temp
x=local
x=
x=global
## END
## BUG bash STDOUT:
x=temp-binding
x=mutated-temp
x=local
x=global
x=global
## END
## BUG mksh STDOUT:
x=temp-binding
x=mutated-temp
x=local
x=mutated-temp
x=mutated-temp
## END
## BUG yash STDOUT:
# yash has no locals
x=temp-binding
x=mutated-temp
x=mutated-temp
x=
x=
## END

#### Test above without 'local' (which is not POSIX)
f() {
  echo "x=$x"

  x=mutated-temp  # mutate temp frame
  echo "x=$x"

  # Unset it
  unset x
  echo "x=$x"
}

x=global
x=temp-binding f
echo "x=$x"

## OK dash/zsh STDOUT:
x=temp-binding
x=mutated-temp
x=
x=global
## END
## BUG mksh/yash STDOUT:
x=temp-binding
x=mutated-temp
x=
x=
## END
## STDOUT:
x=temp-binding
x=mutated-temp
x=global
x=global
## END

#### Using ${x-default} after unsetting local shadowing a global
f() {
  echo "x=$x"
  local x='local'
  echo "x=$x"
  unset x
  echo "- operator = ${x-default}"
  echo ":- operator = ${x:-default}"
}
x=global
f
## OK dash/bash/zsh STDOUT:
x=global
x=local
- operator = default
:- operator = default
## END
## STDOUT:
x=global
x=local
- operator = global
:- operator = global
## END

#### Using ${x-default} after unsetting a temp binding shadowing a global
f() {
  echo "x=$x"
  local x='local'
  echo "x=$x"
  unset x
  echo "- operator = ${x-default}"
  echo ":- operator = ${x:-default}"
}
x=global
x=temp-binding f
## OK dash/zsh STDOUT:
x=temp-binding
x=local
- operator = default
:- operator = default
## END
## STDOUT:
x=temp-binding
x=local
- operator = temp-binding
:- operator = temp-binding
## END
## BUG bash STDOUT:
x=temp-binding
x=local
- operator = global
:- operator = global
## END

#### local a=loc $var c=loc
var='b'
b=global
echo $b
f() {
  local a=loc $var c=loc
  argv.py "$a" "$b" "$c"
}
f
## STDOUT:
global
['loc', '', 'loc']
## END
## BUG dash STDOUT:
global
['loc', 'global', 'loc']
## END

#### redirect after assignment builtin (eval redirects after evaluating arguments)

# See also: spec/redir-order.test.sh (#2307)
# The $(stdout_stderr.py) is evaluated *before* the 2>/dev/null redirection

readonly x=$(stdout_stderr.py) 2>/dev/null
echo done
## STDOUT:
done
## END
## STDERR:
STDERR
## END
## BUG zsh stderr-json: ""

#### redirect after command sub (like case above but without assignment builtin)
echo stdout=$(stdout_stderr.py) 2>/dev/null
## STDOUT:
stdout=STDOUT
## END
## STDERR:
STDERR
## END
