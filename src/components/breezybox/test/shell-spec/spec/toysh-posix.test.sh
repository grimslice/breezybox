## oils_failures_allowed: 2
## compare_shells: bash dash mksh zsh ash yash

#### Fatal error
# http://landley.net/notes.html#20-06-2020

abc=${a?bc} echo hello; echo blah
## status: 1
## OK yash/dash/ash status: 2
## stdout-json: ""

#### Function def in pipeline
# http://landley.net/notes.html#26-05-2020

echo hello | potato() { echo abc; } | echo ha

## STDOUT:
ha
## END

#### var and func - http://landley.net/notes.html#19-03-2020
potato() { echo hello; }
potato=42
echo $potato

potato

## STDOUT:
42
hello
## END


#### IFS - http://landley.net/notes.html#05-03-2020
case $SH in zsh) exit ;; esac

IFS=x
chicken() { for i in "$@"; do echo =$i=; done;}
chicken one abc dxf ghi

echo ---
myfunc() { "$SH" -c 'IFS=x; for i in $@; do echo =$i=; done' blah "$@"; }
myfunc one "" two

## STDOUT:
=one=
=abc=
=d f=
=ghi=
---
=one=
==
=two=
## END

## BUG dash/ash STDOUT:
=one=
=abc=
=d f=
=ghi=
---
=one=
=two=
## END

## OK zsh STDOUT:
## END

#### IFS=x and '' and unquoted $@ - reduction of case above - copied into spec/word-split

setopt SH_WORD_SPLIT
#set -x

set -- one "" two

IFS=x

argv.py $@

for i in $@; do
  echo -$i-
done

## STDOUT:
['one', '', 'two']
-one-
--
-two-
## END

## BUG dash/ash/zsh STDOUT:
['one', 'two']
-one-
-two-
## END


#### for loop parsing - http://landley.net/notes.html#04-03-2020

$SH -c '
for i
in one two three
do echo $i;
done
'
echo $?

$SH -c 'for i; in one two three; do echo $i; done'
test $? -ne 0 && echo cannot-parse

## STDOUT:
one
two
three
0
cannot-parse
## END

#### Parsing $(( ))
# http://landley.net/notes.html#15-03-2020
$SH -c 'echo $((echo hello))'
if test $? -ne 0; then echo fail; fi
## stdout: fail

#### IFS - http://landley.net/notes.html#15-02-2020 (TODO: osh)

IFS=x
A=xabcxx
for i in $A; do echo =$i=; done
echo

unset IFS
A="   abc   def   "
for i in ""$A""; do echo =$i=; done

## STDOUT:
==
=abc=
==

==
=abc=
=def=
==
## END
## BUG zsh status: 1
## BUG zsh stdout-json: ""

#### IFS 2 - copied into spec/word-split
# this one appears different between osh and bash
A="   abc   def   "; for i in ""x""$A""; do echo =$i=; done

## STDOUT:
=x=
=abc=
=def=
==
## END
## BUG zsh status: 1
## BUG zsh stdout-json: ""

#### IFS 3
IFS=x; X="onextwoxxthree"; y=$X; echo $y
## STDOUT:
one two  three
## END
## BUG zsh STDOUT:
onextwoxxthree
## END

#### IFS 4

setopt SH_WORD_SPLIT

IFS=x

func1() {
  echo /$*/
  for i in $*; do echo -$i-; done
}
func1 "" ""

echo

func2() {
  echo /"$*"/
  for i in =$*=; do echo -$i-; done
}
func2 "" ""

## STDOUT:
/ /

/x/
-=-
-=-
## END
## BUG bash STDOUT:
/ /
--

/x/
-=-
-=-
## END
## BUG yash/zsh STDOUT:
/ /
--
--

/x/
-=-
-=-
## END

#### IFS 5
cc() { for i in $*; do echo -$i-; done;}; cc "" "" "" "" ""
cc() { echo =$1$2=;}; cc "" ""
## STDOUT:
==
## END
## BUG yash STDOUT:
--
--
--
--
--
==
## END
## BUG zsh status: 1
## BUG zsh stdout-json: ""

#### Can't parse extra }

$SH -c 'for i in a"$@"b;do echo =$i=;done;}' 123 456 789
## status: 2
## OK mksh/zsh status: 1
## STDOUT:
## END

#### Pipeline - http://landley.net/notes-2019.html#16-12-2019
echo hello | { read i; echo $i;} | { read i; echo $i;} | cat
echo hello | while read i; do echo -=$i=- | sed s/=/@/g ; done | cat
## STDOUT:
hello
-@hello@-
## END

