## compare_shells: dash bash mksh zsh ash

#### command -v executable, builtin

#command -v grep ls

command -v grep | egrep -o '/[^/]+$'
command -v ls | egrep -o '/[^/]+$'
echo

command -v true
command -v eval

## STDOUT:
/grep
/ls

true
eval
## END


#### command skips function lookup
seq() {
  echo "$@"
}
command  # no-op
seq 3
command seq 3
# subshell shouldn't fork another process (but we don't have a good way of
# testing it)
( command seq 3 )
## STDOUT:
3
1
2
3
1
2
3
## END

#### command command seq 3
command command seq 3
## STDOUT:
1
2
3
## END
## OK zsh stdout-json: ""
## OK zsh status: 127

#### command -p (hide tool in custom path)
mkdir -p $TMP/bin
echo "echo hello" > $TMP/bin/hello
chmod +x $TMP/bin/hello
export PATH=$TMP/bin
command -p hello
## status: 127 

#### $(command type ls)
type() { echo FUNCTION; }
type
s=$(command type echo)
echo $s | grep builtin > /dev/null
echo status=$?
## STDOUT:
FUNCTION
status=0
## END
## OK zsh STDOUT:
FUNCTION
status=1
## END
## OK mksh STDOUT:
status=1
## END

