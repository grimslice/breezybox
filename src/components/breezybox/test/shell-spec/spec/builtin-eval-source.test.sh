## compare_shells: dash bash-4.4 mksh zsh
## oils_failures_allowed: 0

#### Eval
eval "a=3"
echo $a
## stdout: 3

#### eval accepts/ignores --
eval -- echo hi
## STDOUT:
hi
## END
## BUG dash status: 127
## BUG dash stdout-json: ""

#### eval usage
eval -
echo $?
eval -z
echo $?
## STDOUT:
127
2
## END
## OK dash STDOUT:
127
127
## END
## OK-2 mksh status: 1
## OK-2 mksh STDOUT:
127
## END
## OK-3 zsh STDOUT:
0
127
## END

#### eval string with 'break continue return error'

set -e

sh_func_that_evals() {
  local code_str=$1
  for i in 1 2; do
    echo $i
    eval "$code_str"
  done
  echo 'end func'
}

for code_str in break continue return false; do
  echo "--- $code_str"
  sh_func_that_evals "$code_str"
done
echo status=$?

## status: 1
## STDOUT:
--- break
1
end func
--- continue
1
2
end func
--- return
1
--- false
1
## END

## BUG mksh STDOUT:
--- break
1
2
end func
--- continue
1
2
end func
--- return
1
--- false
1
## END

#### exit within eval (regression)
eval 'exit 42'
echo 'should not get here'
## stdout-json: ""
## status: 42

#### exit within source (regression)
cd $TMP
echo 'exit 42' > lib.sh
. ./lib.sh
echo 'should not get here'
## stdout-json: ""
## status: 42

#### Source
lib=$TMP/spec-test-lib.sh
echo 'LIBVAR=libvar' > $lib
. $lib  # dash doesn't have source
echo $LIBVAR
## stdout: libvar

#### Source with syntax error
# TODO: We should probably use dash behavior of a fatal error.
# Although set-o errexit handles this.  We don't want to break the invariant
# that a builtin like 'source' behaves like an external program.  An external
# program can't halt the shell!
echo 'echo >' > $TMP/syntax-error.sh
. $TMP/syntax-error.sh
echo status=$?
## stdout: status=2
## OK bash/mksh stdout: status=1
## OK zsh stdout: status=126
## OK dash stdout-json: ""
## OK dash status: 2

#### Eval with syntax error
eval 'echo >'
echo status=$?
## stdout: status=2
## OK bash/zsh stdout: status=1
## OK dash stdout-json: ""
## OK dash status: 2
## OK mksh stdout-json: ""
## OK mksh status: 1

#### source works for files in subdirectory
mkdir -p dir
echo "echo path" > dir/cmd
. dir/cmd
rm dir/cmd
## STDOUT:
path
## END

#### source doesn't crash when targeting a directory
cd $TMP
mkdir -p dir
. ./dir/
echo status=$?
## stdout: status=1
## OK dash/zsh/mksh stdout: status=0

