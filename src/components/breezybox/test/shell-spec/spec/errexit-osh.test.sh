## compare_shells: bash dash mksh ash

# OSH mechanisms:
#
# - shopt -s strict_errexit 
# - shopt -s command_sub_errexit
# - inherit_errexit (bash)
#
# Summary:
# - local assignment is different than global!  The exit code and errexit
#   behavior are different because the concept of the "last command" is
#   different.
# - ash has copied bash behavior!

#### command sub: errexit is NOT inherited and outer shell keeps going

# This is the bash-specific bug here:
# https://blogs.janestreet.com/when-bash-scripts-bite/
# See inherit_errexit below.
#
# I remember finding a script that relies on bash's bad behavior, so OSH copies
# it.  But you can opt in to better behavior.

set -o errexit
echo $(echo one; false; echo two)  # bash/ash keep going
echo parent status=$?
## STDOUT:
one two
parent status=0
## END
# dash and mksh: inner shell aborts, but outer one keeps going!
## OK dash/mksh STDOUT:
one
parent status=0
## END

#### command sub with inherit_errexit only
set -o errexit
shopt -s inherit_errexit || true
echo zero
echo $(echo one; false; echo two)  # bash/ash keep going
echo parent status=$?
## STDOUT:
zero
one
parent status=0
## END
## OK ash STDOUT:
zero
one two
parent status=0
## END

#### strict_errexit allows singleton pipeline
set -o errexit
shopt -s strict_errexit || true

if ! false; then
  echo yes
fi

## STDOUT:
yes
## END

#### strict_errexit with && || !
set -o errexit
shopt -s strict_errexit || true

if true && true; then
  echo A
fi

if true || false; then
  echo B
fi

if ! false && ! false; then
  echo C
fi

## STDOUT:
A
B
C
## END

#### strict_errexit detects proc in && || !
set -o errexit
shopt -s strict_errexit || true

myfunc() {
  echo 'failing'
  false
  echo 'should not get here'
}

if true && ! myfunc; then
  echo B
fi

if ! myfunc; then
  echo A
fi

## status: 1
## STDOUT:
## END

# POSIX shell behavior:

## OK bash/dash/mksh/ash status: 0
## OK bash/dash/mksh/ash STDOUT:
failing
should not get here
failing
should not get here
## END



#### command sub: last command fails but keeps going and exit code is 0
set -o errexit
echo $(echo one; false)  # we lost the exit code
echo status=$?
## STDOUT:
one
status=0
## END

#### global assignment with command sub: middle command fails
set -o errexit
s=$(echo one; false; echo two;)
echo "$s"
## status: 0
## STDOUT:
one
two
## END
# dash and mksh: whole thing aborts!
## OK dash/mksh stdout-json: ""
## OK dash/mksh status: 1

#### global assignment with command sub: last command fails and it aborts
set -o errexit
s=$(echo one; false)
echo status=$?
## stdout-json: ""
## status: 1

#### local: middle command fails and keeps going
set -o errexit
f() {
  echo good
  local x=$(echo one; false; echo two)
  echo status=$?
  echo $x
}
f
## STDOUT:
good
status=0
one two
## END
# for dash and mksh, the INNER shell aborts, but the outer one keeps going!
## OK dash/mksh STDOUT:
good
status=0
one
## END

#### local: last command fails and also keeps going
set -o errexit
f() {
  echo good
  local x=$(echo one; false)
  echo status=$?
  echo $x
}
f
## STDOUT:
good
status=0
one
## END

#### global assignment when last status is failure
# this is a bug I introduced
set -o errexit
x=$(false) || true   # from abuild
[ -n "$APORTSDIR" ] && true
BUILDDIR=${_BUILDDIR-$BUILDDIR}
echo status=$?
## STDOUT:
status=0
## END

#### errexit is silent (verbose_errexit for Oils)
shopt -u verbose_errexit 2>/dev/null || true
set -e
false
## stderr-json: ""
## status: 1

#### OLD: command sub in redirect in conditional
set -o errexit

if echo tmp_contents > $(echo tmp); then
  echo 2
fi
cat tmp
## STDOUT:
2
tmp_contents
## END

