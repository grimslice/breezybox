# 00_smoke.sh - shell-semantics sanity. Deep coverage lives in the host spec
# suite; this is just a "the interpreter on this device works" tripwire.

name=BreezyBox; greeting="hello, $name"
assert_eq "hello, BreezyBox" "$greeting" "variable expansion"

lit='$name'
assert_eq '$name' "$lit" "single-quote is literal"

n=2
if [ "$n" = 1 ]; then r=one; elif [ "$n" = 2 ]; then r=two; else r=many; fi
assert_eq two "$r" "if/elif/else"

assert_ok   test 5 -gt 3
assert_fail test 5 -lt 3

acc=
for w in a b c; do acc=$acc$w; done
assert_eq abc "$acc" "for loop"

s=
while [ "$s" != xxx ]; do s=x$s; done
assert_eq xxx "$s" "while loop"

flag=0; false || flag=1; true && flag=2
assert_eq 2 "$flag" "&& / ||"

false; a=$?; true; b=$?
assert_eq "1 0" "$a $b" "exit status \$?"

who=$(echo world)
assert_eq world "$who" "command substitution"

assert_eq 4 "$(( 2 + 2 ))" "arithmetic"

double() { echo $(( $1 * 2 )); }
assert_eq 10 "$(double 5)" "function + arg"

count=$(printf 'a\nb\nc\n' | wc -l)
assert_num 3 "$count" "pipe into wc"
