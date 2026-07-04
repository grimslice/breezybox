## oils_failures_allowed: 3
## compare_shells: dash bash-4.4 mksh zsh


# NOTE:
# - $! is tested in background.test.sh
# - $- is tested in sh-options
#
# TODO: It would be nice to make a table, like:
#
# $$  $BASHPID  $PPID   $SHLVL   $BASH_SUBSHELL
#  X 
# (Subshell,  Command Sub,  Pipeline,  Spawn $0)
#
# And see whether the variable changed.

#### $PWD is set
# Just test that it has a slash for now.
echo $PWD | grep -q /
echo status=$?
## STDOUT:
status=0
## END

#### $PWD is not only set, but exported
env | grep -q PWD
echo status=$?
## stdout: status=0
## BUG mksh stdout: status=1

#### HOSTNAME OSTYPE can be changed
case $SH in zsh) exit ;; esac

#$SH -c 'echo hostname=$HOSTNAME'

HOSTNAME=x $SH -c 'echo hostname=$HOSTNAME'
OSTYPE=x $SH -c 'echo ostype=$OSTYPE'
echo

#PS4=x $SH -c 'echo ps4=$PS4'

# OPTIND is special
#OPTIND=xx $SH -c 'echo optind=$OPTIND'


## STDOUT:
hostname=x
ostype=x

## END

## BUG zsh STDOUT:
## END


#### $1 .. $9 are scoped, while $0 is not
fun() {
  case $0 in
    *sh)
      echo 'sh'
      ;;
    *sh-*)  # bash-4.4 is OK
      echo 'sh'
      ;;
  esac

  echo $1 $2
}
fun a b

## STDOUT:
sh
a b
## END
## BUG zsh STDOUT:
a b
## END

#### $?
echo $?  # starts out as 0
sh -c 'exit 33'
echo $?
## STDOUT:
0
33
## END
## status: 0

#### $#
set -- 1 2 3 4
echo $#
## stdout: 4
## status: 0

#### $$ looks like a PID
echo $$ | egrep -q '[0-9]+'  # Test that it has decimal digits
echo status=$?
## STDOUT:
status=0
## END

#### $$ doesn't change with subshell or command sub
# Just test that it has decimal digits
set -o errexit
die() {
  echo 1>&2 "$@"; exit 1
}
parent=$$
test -n "$parent" || die "empty PID in parent"
( child=$$
  test -n "$child" || die "empty PID in subshell"
  test "$parent" = "$child" || die "should be equal: $parent != $child"
  echo 'subshell OK'
)
echo $( child=$$
        test -n "$child" || die "empty PID in command sub"
        test "$parent" = "$child" || die "should be equal: $parent != $child"
        echo 'command sub OK'
      )
exit 3  # make sure we got here
## status: 3
## STDOUT:
subshell OK
command sub OK
## END
