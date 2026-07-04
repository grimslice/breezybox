## tags: interactive
## compare_shells: bash
## oils_failures_allowed: 9
## oils_cpp_failures_allowed: 7

#### history -d to delete 1 item

cd $TMP
HISTFILE=tmp
printf "cmd orig%s\n" {1..3} > tmp
history -c
history -r
history -d 1
history | grep orig1 > /dev/null
echo "status=$?"

## STDOUT:
status=1
## END


