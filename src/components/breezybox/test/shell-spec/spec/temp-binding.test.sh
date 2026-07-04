## compare_shells: dash bash zsh mksh ash yash
## oils_failures_allowed: 0

# forked from spec/ble-idioms
# the IFS= eval 'local x' bug

#### FOO=bar $unset - temp binding, then empty argv from unquoted unset var (#2411)
foo=alive! $unset
echo $foo
## STDOUT:
alive!
## END
