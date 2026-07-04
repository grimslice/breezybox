## oils_failures_allowed: 0
## compare_shells: bash dash mksh zsh ash yash

#### true is not special; prefix assignments don't persist, it can be redefined
foo=bar true
echo foo=$foo

true() {
  echo true func
}
foo=bar true
echo foo=$foo

## STDOUT:
foo=
true func
foo=
## END

## BUG mksh STDOUT:
foo=
true func
foo=bar
## END

# POSIX rule about special builtins pointed at:
#
# https://www.reddit.com/r/oilshell/comments/5ykpi3/oildev_is_alive/

#### Non-special builtins CAN be redefined as functions
test -n "$BASH_VERSION" && set -o posix
true() {
  echo 'true func'
}
true hi
echo status=$?
## STDOUT:
true func
status=0
## END
