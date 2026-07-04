## oils_failures_allowed: 1
## compare_shells: bash mksh zsh ash

#### read line from here doc

# NOTE: there are TABS below
read x <<EOF
A		B C D E
FG
EOF
echo "[$x]"
## stdout: [A		B C D E]
## status: 0

#### read from empty file
echo -n '' > $TMP/empty.txt
read x < $TMP/empty.txt
argv.py "status=$?" "$x"

# No variable name, behaves the same
read < $TMP/empty.txt
argv.py "status=$?" "$REPLY"

## STDOUT:
['status=1', '']
['status=1', '']
## END
## OK dash STDOUT:
['status=1', '']
['status=2', '']
## END
## status: 0

#### read /dev/null
read -n 1 </dev/null
echo $?
## STDOUT:
1
## END
## OK dash stdout: 2

#### read with zero args
echo | read
echo status=$?
## STDOUT:
status=0
## END
## BUG dash STDOUT:
status=2
## END

#### read builtin with no newline returns status 1

# This is odd because the variable is populated successfully.  OSH/YSH might
# need a separate put reading feature that doesn't use IFS.

echo -n ZZZ | { read x; echo status=$?; echo $x; }

## STDOUT:
status=1
ZZZ
## END
## status: 0

#### read builtin splits value across multiple vars
# NOTE: there are TABS below
read x y z <<EOF
A		B C D E 
FG
EOF
echo "[$x/$y/$z]"
## stdout: [A/B/C D E]
## status: 0

#### read builtin with too few variables
set -o errexit
set -o nounset  # hm this doesn't change it
read x y z <<EOF
A B
EOF
echo /$x/$y/$z/
## stdout: /A/B//
## status: 0

#### read -n with invalid arg
read -n not_a_number
echo status=$?
## stdout: status=2
## OK bash stdout: status=1
## OK zsh stdout-json: ""

#### read -r ignores backslashes
echo 'one\ two' > $TMP/readr.txt
read escaped < $TMP/readr.txt
read -r raw < $TMP/readr.txt
argv.py "$escaped" "$raw"
## stdout: ['one two', 'one\\ two']

#### read -r with other backslash escapes
echo 'one\ two\x65three' > $TMP/readr.txt
read escaped < $TMP/readr.txt
read -r raw < $TMP/readr.txt
argv.py "$escaped" "$raw"
# mksh respects the hex escapes here, but other shells don't!
## stdout: ['one twox65three', 'one\\ two\\x65three']
## BUG mksh/zsh stdout: ['one twoethree', 'one\\ twoethree']

#### read multiple vars spanning many lines
read x y << 'EOF'
one-\
two three-\
four five-\
six
EOF
argv.py "$x" "$y" "$z"
## stdout: ['one-two', 'three-four five-six', '']

#### read -r with \n
echo '\nline' > $TMP/readr.txt
read escaped < $TMP/readr.txt
read -r raw < $TMP/readr.txt
argv.py "$escaped" "$raw"
# dash/mksh/zsh are bugs because at least the raw mode should let you read a
# literal \n.
## stdout: ['nline', '\\nline']
## BUG dash/mksh/zsh stdout: ['', '']

#### read multiple lines with IFS=:
# The leading spaces are stripped if they appear in IFS.
# IFS chars are escaped with :.
tmp=$TMP/$(basename $SH)-read-ifs.txt
IFS=:
cat >$tmp <<'EOF'
  \\a :b\: c:d\
  e
EOF
read a b c d < $tmp
# Use printf because echo in dash/mksh interprets escapes, while it doesn't in
# bash.
printf "%s\n" "[$a|$b|$c|$d]"
## stdout: [  \a |b: c|d  e|]

#### read with IFS=''
IFS=''
read x y <<EOF
  a b c d
EOF
echo "[$x|$y]"
## stdout: [  a b c d|]

#### read does not respect C backslash escapes

# bash doesn't respect these, but other shells do.  Gah!  I think bash
# behavior makes more sense.  It only escapes IFS.
echo '\a \b \c \d \e \f \g \h \x65 \145 \i' > $TMP/read-c.txt
read line < $TMP/read-c.txt
echo $line
## STDOUT:
a b c d e f g h x65 145 i
## END
## BUG ash STDOUT:
abcdefghx65 145 i
## END
## BUG dash/zsh stdout-json: "\u0007 \u0008\n"
## BUG mksh stdout-json: "\u0007 \u0008 d \u001b \u000c g h e 145 i\n"

#### dynamic scope used to set vars
f() {
  read head << EOF
ref: refs/heads/dev/andy
EOF
}
f
echo $head
## STDOUT:
ref: refs/heads/dev/andy
## END

#### read -t -0.5 is invalid
# bash appears to just take the absolute value?

read -t -0.5 < /dev/null
echo $?

## STDOUT:
2
## END
## BUG bash STDOUT:
1
## END
## BUG zsh stdout-json: ""
## BUG zsh status: 1

#### read -u syntax error
read -u -3
echo status=$?
## STDOUT:
status=2
## END
## OK bash/zsh STDOUT:
status=1
## END

#### read usage
read -n -1
echo status=$?
## STDOUT:
status=2
## END
## OK bash stdout: status=1
## BUG mksh stdout-json: ""
# zsh gives a fatal error?  seems inconsistent
## BUG zsh stdout-json: ""
## BUG zsh status: 1

#### read from redirected directory is non-fatal error

# This tickles an infinite loop bug in our version of mksh!  TODO: upgrade the
# version and enable this
case $SH in mksh) return ;; esac

cd $TMP
mkdir -p dir
read x < ./dir
echo status=$?

## STDOUT:
status=1
## END
# OK mksh stdout: status=2
## OK mksh stdout-json: ""

#### read -n 0
case $SH in zsh) exit 99;; esac  # read -n not implemented

echo 'a\b\c\d\e\f' | (read -n 0; argv.py "$REPLY")

## STDOUT:
['']
## END
# ash appears to treat 0 as unspecified
## OK ash STDOUT:
['abcdef']
## END
## OK zsh status: 99
## OK zsh STDOUT:
## END

#### IFS='x ' read a b: trailing spaces (with max_split)
echo 'hello world  test   ' | (read a b; argv.py "$a" "$b")
echo '-- IFS=x --'
IFS='x '
echo 'a ax  x  '     | (read a b; argv.py "$a" "$b")
echo 'a ax  x  x'    | (read a b; argv.py "$a" "$b")
echo 'a ax  x  x  '  | (read a b; argv.py "$a" "$b")
echo 'a ax  x  x  a' | (read a b; argv.py "$a" "$b")
## STDOUT:
['hello', 'world  test']
-- IFS=x --
['a', 'ax  x']
['a', 'ax  x  x']
['a', 'ax  x  x']
['a', 'ax  x  x  a']
## END

#### IFS='\ ' and backslash escaping
IFS='\ '
echo "hello\ world  test" | (read a b; argv.py "$a" "$b")
IFS='\'
echo "hello\ world  test" | (read a b; argv.py "$a" "$b")
## STDOUT:
['hello world', 'test']
['hello world  test', '']
## END
# In mksh/zsh, IFS='\' is stronger than backslash escaping
## OK mksh/zsh STDOUT:
['hello', 'world  test']
['hello', ' world  test']
## END

#### max_split and backslash escaping
echo 'Aa b \ a\ b' | (read a b; argv.py "$a" "$b")
echo 'Aa b \ a\ b' | (read a b c; argv.py "$a" "$b" "$c")
echo 'Aa b \ a\ b' | (read a b c d; argv.py "$a" "$b" "$c" "$d")
## STDOUT:
['Aa', 'b  a b']
['Aa', 'b', ' a b']
['Aa', 'b', ' a b', '']
## END

#### IFS=x read a b <<< xxxxxx
IFS='x '
echo x     | (read a b; argv.py "$a" "$b")
echo xx    | (read a b; argv.py "$a" "$b")
echo xxx   | (read a b; argv.py "$a" "$b")
echo xxxx  | (read a b; argv.py "$a" "$b")
echo xxxxx | (read a b; argv.py "$a" "$b")
echo '-- spaces --'
echo 'x    ' | (read a b; argv.py "$a" "$b")
echo 'xx   ' | (read a b; argv.py "$a" "$b")
echo 'xxx  ' | (read a b; argv.py "$a" "$b")
echo 'xxxx ' | (read a b; argv.py "$a" "$b")
echo 'xxxxx' | (read a b; argv.py "$a" "$b")
echo '-- with char --'
echo 'xa    ' | (read a b; argv.py "$a" "$b")
echo 'xax   ' | (read a b; argv.py "$a" "$b")
echo 'xaxx  ' | (read a b; argv.py "$a" "$b")
echo 'xaxxx ' | (read a b; argv.py "$a" "$b")
echo 'xaxxxx' | (read a b; argv.py "$a" "$b")
## STDOUT:
['', '']
['', '']
['', 'xx']
['', 'xxx']
['', 'xxxx']
-- spaces --
['', '']
['', '']
['', 'xx']
['', 'xxx']
['', 'xxxx']
-- with char --
['', 'a']
['', 'a']
['', 'axx']
['', 'axxx']
['', 'axxxx']
## END
## OK-2 zsh STDOUT:
['', '']
['', 'x']
['', 'xx']
['', 'xxx']
['', 'xxxx']
-- spaces --
['', '']
['', 'x']
['', 'xx']
['', 'xxx']
['', 'xxxx']
-- with char --
['', 'a']
['', 'ax']
['', 'axx']
['', 'axxx']
['', 'axxxx']
## END

#### read and "\ "

IFS='x '
check() { echo "$1" | (read a b; argv.py "$a" "$b"); }

echo '-- xs... --'
check 'x '
check 'x \ '
check 'x \ \ '
check 'x \ \ \ '
echo '-- xe... --'
check 'x\ '
check 'x\ \ '
check 'x\ \ \ '
check 'x\  '
check 'x\  '
check 'x\    '

# check 'xx\ '
# check 'xx\ '

## STDOUT:
-- xs... --
['', '']
['', ' ']
['', '  ']
['', '   ']
-- xe... --
['', ' ']
['', '  ']
['', '   ']
['', ' ']
['', ' ']
['', ' ']
## END
## BUG mksh STDOUT:
-- xs... --
['', '']
['', '']
['', ' ']
['', '  ']
-- xe... --
['', '']
['', ' ']
['', '  ']
['', '']
['', '']
['', '']
## END
## OK-2 zsh/ash STDOUT:
-- xs... --
['', '']
['', '']
['', '']
['', '']
-- xe... --
['', '']
['', '']
['', '']
['', '']
['', '']
['', '']
## END

