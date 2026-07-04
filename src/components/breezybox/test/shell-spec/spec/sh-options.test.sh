## compare_shells: bash dash mksh
## oils_failures_allowed: 1
## tags: interactive

# Test options to set, shopt, $SH.

#### set -o vi/emacs
set -o vi
echo $?
set -o emacs
echo $?
## STDOUT:
0
0
## END

#### nounset
echo "[$unset]"
set -o nounset
echo "[$unset]"
echo end  # never reached
## stdout: []
## status: 1
## OK dash status: 2

#### -u is nounset
echo "[$unset]"
set -u
echo "[$unset]"
echo end  # never reached
## stdout: []
## status: 1
## OK dash status: 2

#### noclobber off
set -o errexit

echo foo > can-clobber
echo status=$?
set +C

echo foo > can-clobber
echo status=$?
set +o noclobber

echo foo > can-clobber
echo status=$?
cat can-clobber

## STDOUT:
status=0
status=0
status=0
foo
## END

#### noclobber on >>
rm -f $TMP/no-clobber

set -C
echo foo >> $TMP/no-clobber
echo status=$?

cat $TMP/no-clobber
## STDOUT:
status=0
foo
## END

#### set without args lists variables
__GLOBAL=g
f() {
  local __mylocal=L
  local __OTHERLOCAL=L
  __GLOBAL=mutated
  set | grep '^__'
}
g() {
  local __var_in_parent_scope=D
  f
}
g
## status: 0
## STDOUT:
__GLOBAL=mutated
__OTHERLOCAL=L
__mylocal=L
__var_in_parent_scope=D
## END
## OK mksh STDOUT:
__GLOBAL=mutated
__var_in_parent_scope=D
__OTHERLOCAL=L
__mylocal=L
## END
## OK dash STDOUT:
__GLOBAL='mutated'
__OTHERLOCAL='L'
__mylocal='L'
__var_in_parent_scope='D'
## END

#### no-ops not shown by shopt -p

shopt -p | grep xpg
echo --
## STDOUT:
--
## END
## OK bash STDOUT:
shopt -u xpg_echo
--
## END


