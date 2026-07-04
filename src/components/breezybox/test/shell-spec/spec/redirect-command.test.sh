## oils_failures_allowed: 0
## compare_shells: bash dash mksh zsh

# Notes:
# - ash is just like dash, so don't bother testing
# - zsh fails several cases

#### >$file touches a file
rm -f myfile
test -f myfile
echo status=$?

>myfile
test -f myfile
echo status=$?

## STDOUT:
status=1
status=0
## END

## BUG zsh STDOUT:
status=1
## END

# regression for OSH
## stderr-json: ""

#### $(< file; end) is not a special case

seq 5 6 > myfile

# zsh prints the file each time!
# other shells do nothing?

foo=$(echo begin; < myfile)
echo $foo
echo ---

foo=$(< myfile; echo end)
echo $foo
echo ---

foo=$(< myfile; <myfile)
echo $foo
echo ---

## STDOUT:
begin
---
end
---

---
## END

## BUG zsh STDOUT:
begin
5
6
---
5
6
end
---
5
6
5
6
---
## END

#### < file in pipeline and subshell doesn't work
echo FOO > file2

# This only happens in command subs, which is weird
< file2 | tr A-Z a-z
( < file2 )
echo end
## STDOUT:
end
## END
## BUG zsh STDOUT:
foo
FOO
end
## END

#### Leading redirect in a simple command
echo hello >$TMP/hello.txt  # temporary fix
<$TMP/hello.txt cat
## stdout: hello

#### Redirect in the middle of a simple command
f=$TMP/out
echo -n 1 2 '3 ' > $f
echo -n 4 5 >> $f '6 '
echo -n 7 >> $f 8 '9 '
echo -n >> $f 1 2 '3 '
echo >> $f -n 4 5 '6'

cat $f
echo
## STDOUT:
1 2 3 4 5 6 7 8 9 1 2 3 4 5 6
## END

#### Redirect in command sub
FOO=$(echo foo 1>&2)
echo $FOO
## stdout:
## stderr: foo

#### Redirect in the middle of two assignments
FOO=foo >$TMP/out.txt BAR=bar printenv.py FOO BAR
tac $TMP/out.txt
## STDOUT:
bar
foo
## END
## BUG zsh STDOUT:
## END

#### Redirect in function body
fun() { echo hi; } 1>&2
fun
## STDOUT:
## END
## STDERR:
hi
## END

#### Redirect in function body AND function call
fun() { echo hi; } 1>&2
fun 2>&1
## STDOUT:
hi
## END
## STDERR:
## END

#### redirect if
if true; then
  echo if-body
fi >out

cat out

## STDOUT:
if-body
## END

#### redirect case
case foo in
  foo)
    echo case-body
    ;;
esac > out

cat out

## STDOUT:
case-body
## END

#### redirect while
while true; do
  echo while-body
  break
done > out

cat out

## STDOUT:
while-body
## END

#### redirect for loop
for i in $(seq 3)
do
  echo $i
done > $TMP/redirect-for-loop.txt
cat $TMP/redirect-for-loop.txt
## STDOUT:
1
2
3
## END

#### redirect subshell
( echo foo ) 1>&2
## stderr: foo
## stdout-json: ""

#### Prefix redirect for loop -- not allowed
>$TMP/redirect2.txt for i in $(seq 3)
do
  echo $i
done
cat $TMP/redirect2.txt
## status: 2
## OK mksh status: 1
## BUG zsh status: 0
## BUG zsh STDOUT:
1
2
3
## END

#### Brace group redirect
# Suffix works, but prefix does NOT work.
# That comes from '| compound_command redirect_list' in the grammar!
{ echo block-redirect; } > $TMP/br.txt
cat $TMP/br.txt | wc -c
## stdout: 15

#### Redirect function stdout
f() { echo one; echo two; }
f > $TMP/redirect-func.txt
cat $TMP/redirect-func.txt
## STDOUT:
one
two
## END

#### Nested function stdout redirect
# Shows that a stack is necessary.
inner() {
  echo i1
  echo i2
}
outer() {
  echo o1
  inner > $TMP/inner.txt
  echo o2
}
outer > $TMP/outer.txt
cat $TMP/inner.txt
echo --
cat $TMP/outer.txt
## STDOUT:
i1
i2
--
o1
o2
## END
