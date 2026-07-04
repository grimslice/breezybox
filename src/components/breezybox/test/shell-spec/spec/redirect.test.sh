## oils_failures_allowed: 2
## compare_shells: bash dash mksh

#### >& and <& are the same

echo one 1>&2

echo two 1<&2

## STDERR:
one
two
## END


#### 2>&1 with no command
( exit 42 )  # status is reset after this
echo status=$?
2>&1
echo status=$?
## STDOUT:
status=42
status=0
## END
## stderr-json: ""


#### Nonexistent file
cat <$TMP/nonexistent.txt
echo status=$?
## stdout: status=1
## OK dash stdout: status=2

#### Descriptor redirect with spaces
# Hm this seems like a failure of lookahead!  The second thing should look to a
# file-like thing.
# I think this is a posix issue.
# tag: posix-issue
echo one 1>&2
echo two 1 >&2
echo three 1>& 2
## STDERR:
one
two 1
three
## END

#### Filename redirect with spaces
# This time 1 *is* a descriptor, not a word.  If you add a space between 1 and
# >, it doesn't work.
echo two 1> $TMP/file-redir1.txt
cat $TMP/file-redir1.txt
## stdout: two

#### Quoted filename redirect with spaces
# POSIX makes node of this
echo two \1 > $TMP/file-redir2.txt
cat $TMP/file-redir2.txt
## stdout: two 1

#### Redirect echo to stderr, and then redirect all of stdout somewhere.
{ echo foo52 1>&2; echo 012345789; } > $TMP/block-stdout.txt
cat $TMP/block-stdout.txt |  wc -c 
## stderr: foo52
## stdout: 10

#### : 3>&3 (OSH regression)

# mksh started being flaky on the continuous build and during release.  We
# don't care!  Related to issue #330.
case $SH in mksh) exit ;; esac

: 3>&3
echo hello
## stdout: hello
## BUG mksh stdout-json: ""
## BUG mksh status: 0

#### Redirect to empty string
f=''
echo s > "$f"
echo "result=$?"
set -o errexit
echo s > "$f"
echo DONE
## stdout: result=1
## status: 1
## OK dash stdout: result=2
## OK dash status: 2

#### exec redirect then various builtins
exec 5>$TMP/log.txt
echo hi >&5
set -o >&5
echo done
## STDOUT:
done
## END

#### Parsing of x=1> and related cases

echo x=1>/dev/stdout
echo x=1 >/dev/stdout
echo x= 1>/dev/stdout

echo +1>/dev/stdout
echo +1 >/dev/stdout
echo + 1>/dev/stdout

echo a1>/dev/stdout

## STDOUT:
x=1
x=1
x=
+1
+1
+
a1
## END

