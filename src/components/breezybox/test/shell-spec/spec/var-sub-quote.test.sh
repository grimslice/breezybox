## compare_shells: dash bash mksh

# Tests for the args in:
#
# ${foo:-}
#
# I think the weird single quote behavior is a bug, but everyone agrees.  It's
# a consequence of quote removal.
#
# WEIRD: single quoted default, inside double quotes.  Oh I guess this is
# because double quotes don't treat single quotes as special?
#
# OK here is the issue.  If we have ${} bare, then the default is parsed as
# LexState.OUTER.  If we have "${}", then it's parsed as LexState.DQ.  That
# makes sense I guess.  Vim's syntax highlighting is throwing me off.

#### "${empty:-}"
empty=
argv.py "${empty:-}"
## stdout: ['']

#### ${empty:-}
empty=
argv.py ${empty:-}
## stdout: []

#### substitution of IFS character, quoted and unquoted
IFS=:
s=:
argv.py $s
argv.py "$s"
## STDOUT:
['']
[':']
## END

#### :-
empty=''
argv.py ${empty:-a} ${Unset:-b}
## stdout: ['a', 'b']

#### -
empty=''
argv.py ${empty-a} ${Unset-b}
# empty one is still elided!
## stdout: ['b']

#### Inner single quotes
argv.py ${Unset:-'b'}
## stdout: ['b']

#### Inner double quotes
argv.py ${Unset:-"b"}
## stdout: ['b']

#### Inner double quotes, outer double quotes
argv.py "${Unset-"b"}"
## stdout: ['b']

#### Multiple words: no quotes
argv.py ${Unset:-a b c}
## stdout: ['a', 'b', 'c']

#### Multiple words: outer double quotes, no inner quotes
argv.py "${Unset:-a b c}"
## stdout: ['a b c']

#### part_value tree on RHS
v=${a:-${a:-"1 2" "3 4"}5 "6 7"}
argv.py "${v}"
## stdout: ['1 2 3 45 6 7']

#### Var with multiple words: no quotes
var='a b c'
argv.py ${Unset:-$var}
## stdout: ['a', 'b', 'c']

#### Multiple words: outer double quotes, no inner quotes
var='a b c'
argv.py "${Unset:-$var}"
## stdout: ['a b c']

#### Strip a string with single quotes, unquoted
foo="'a b c d'"
argv.py ${foo%d\'}
## stdout: ["'a", 'b', 'c']

#### Strip a string with single quotes, double quoted
foo="'a b c d'"
argv.py "${foo%d\'}"
## STDOUT:
["'a b c "]
## END

#### The string to strip is space sensitive
foo='a b c d'
argv.py "${foo%c d}" "${foo%c  d}"
## stdout: ['a b ', 'a b c d']

#### The string to strip can be single quoted, outer is unquoted
foo='a b c d'
argv.py ${foo%'c d'} ${foo%'c  d'}
## stdout: ['a', 'b', 'a', 'b', 'c', 'd']

#### # operator with single quoted arg (dash/ash and bash/mksh disagree, reported by Crestwave)
var=a
echo -${var#'a'}-
echo -"${var#'a'}"-
var="'a'"
echo -${var#'a'}-
echo -"${var#'a'}"-
## STDOUT:
--
--
-'a'-
-'a'-
## END
## OK ash STDOUT:
--
-a-
-'a'-
--
## END

#### Var substitution with newlines (#2492)
echo "${var-a \
b}"
echo "${var-a
b}"

echo "${var:-c \
d}"
echo "${var:-c
d}"

var=set
echo "${var:+e \
f}"
echo "${var:+e
f}"

## STDOUT:
a b
a
b
c d
c
d
e f
e
f
## END


#### Var substitution with \n in value
echo "${var-a\nb}"
echo "${var:-c\nd}"
var=val
echo "${var:+e\nf}"

## STDOUT:
a\nb
c\nd
e\nf
## END
## BUG dash/mksh STDOUT:
a
b
c
d
e
f
## END
