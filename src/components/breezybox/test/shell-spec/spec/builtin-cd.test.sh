## compare_shells: dash bash mksh zsh ash
## oils_failures_allowed: 3
## oils_cpp_failures_allowed: 3

#### cd and $PWD
cd /
echo $PWD
## stdout: /

#### cd with 2 or more args is allowed (strict_arg_parse disabled)

mkdir -p foo
cd foo bar

## status: 0
## OK bash/zsh status: 1
## OK mksh status: 2

#### pwd
cd /
pwd
## STDOUT:
/
## END

#### pwd after cd ..
dir=$TMP/dir-one/dir-two
mkdir -p $dir
cd $dir
echo $(basename $(pwd))
cd ..
echo $(basename $(pwd))
## STDOUT:
dir-two
dir-one
## END

#### cd to nonexistent dir
cd /nonexistent/dir
echo status=$?
## stdout: status=1
## OK dash/ash/mksh stdout: status=2

#### cd away from dir that was deleted
dir=$TMP/cd-nonexistent
mkdir -p $dir
cd $dir
rmdir $dir
cd $TMP
echo $(basename $OLDPWD)
echo status=$?
## STDOUT:
cd-nonexistent
status=0
## END

#### arguments to pwd
pwd /
## status: 0
## OK zsh/mksh status: 1

