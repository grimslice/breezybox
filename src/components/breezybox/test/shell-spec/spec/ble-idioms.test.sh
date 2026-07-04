## compare_shells: bash zsh mksh ash
## oils_failures_allowed: 5

#### dynamic arith varname: assign
vec2_set () {
  local this=$1 x=$2 y=$3
  : $(( ${this}_x = $2 ))
  : $(( ${this}_y = y ))
}
vec2_set a 3 4
vec2_set b 5 12
echo a_x=$a_x a_y=$a_y
echo b_x=$b_x b_y=$b_y
## STDOUT:
a_x=3 a_y=4
b_x=5 b_y=12
## END

#### dynamic arith varname: read

vec2_load() {
  local this=$1
  x=$(( ${this}_x ))
  : $(( y = ${this}_y ))
}
a_x=12 a_y=34
vec2_load a
echo x=$x y=$y
## STDOUT:
x=12 y=34
## END

#### dynamic arith varname: copy/add
shopt -s eval_unsafe_arith  # for RHS

vec2_copy () {
  local this=$1 rhs=$2
  : $(( ${this}_x = $(( ${rhs}_x )) ))
  : $(( ${this}_y = ${rhs}_y ))
}
vec2_add () {
  local this=$1 rhs=$2
  : $(( ${this}_x += $(( ${rhs}_x )) ))
  : $(( ${this}_y += ${rhs}_y ))
}
a_x=3 a_y=4
b_x=4 b_y=20
vec2_copy c a
echo c_x=$c_x c_y=$c_y
vec2_add c b
echo c_x=$c_x c_y=$c_y
## STDOUT:
c_x=3 c_y=4
c_x=7 c_y=24
## END

#### Issue #1069 [49] BUG: \return 0 does not work
f0() { return 3;          echo unexpected; return 0; }
f1() { \return 3;         echo unexpected; return 0; }
f0; echo "status=$?"
f1; echo "status=$?"
## STDOUT:
status=3
status=3
## END


#### Issue #1069 [57] BUG: variable v is invisible after IFS= eval 'local v=...'
v=x
case $SH in
mksh) f() { IFS= eval 'typeset v=1'; echo "l:$v"; } ;;
*)    f() { IFS= eval 'local   v=1'; echo "l:$v"; } ;;
esac
f
echo "g:$v"
## STDOUT:
l:1
g:x
## END


#### Issue #1069 [57] - Variable v should be visible after IFS= eval 'local v=...'

set -u

f() {
  # The temp env messes it up
  IFS= eval "local v=\"\$*\""

  # Bug does not appear with only eval
  # eval "local v=\"\$*\""

  #declare -p v
  echo v=$v

  # test -v v; echo "v defined $?"
}

f h e l l o

## STDOUT:
v=hello
## END


