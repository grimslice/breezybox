#!/bin/sh
# BreezyBox shell scripting demo -- self-contained, copy & run.

OK=demo_ok.tmp
NO=demo_no.tmp
: > $OK
: > $NO

echo "Testing BreezyBox shell scripting:"

name=BreezyBox; greeting="hello, $name"
if [ "$greeting" = "hello, BreezyBox" ]; then r=OK; echo . >> $OK; else r=FAIL; echo . >> $NO; fi
echo "    variables... $r"

lit='$name'; exp="$name"
if [ "$lit" = '$name' ] && [ "$exp" = BreezyBox ]; then r=OK; echo . >> $OK; else r=FAIL; echo . >> $NO; fi
echo "    quoting... $r"

n=2
if [ "$n" = 1 ]; then r=one; elif [ "$n" = 2 ]; then r=two; else r=many; fi
if [ "$r" = two ]; then r=OK; echo . >> $OK; else r=FAIL; echo . >> $NO; fi
echo "    conditionals... $r"

if [ 5 -gt 3 ] && [ 2 -le 2 ] && [ 7 -ne 8 ]; then r=OK; echo . >> $OK; else r=FAIL; echo . >> $NO; fi
echo "    numeric tests... $r"

acc=
for w in a b c; do acc=$acc$w; done
if [ "$acc" = abc ]; then r=OK; echo . >> $OK; else r=FAIL; echo . >> $NO; fi
echo "    for loops... $r"

s=
while [ "$s" != xxx ]; do s=x$s; done
if [ "$s" = xxx ]; then r=OK; echo . >> $OK; else r=FAIL; echo . >> $NO; fi
echo "    while loops... $r"

flag=0; false || flag=1; true && flag=2
if [ "$flag" = 2 ]; then r=OK; echo . >> $OK; else r=FAIL; echo . >> $NO; fi
echo "    logical && ||... $r"

false; a=$?; true; b=$?
if [ "$a" = 1 ] && [ "$b" = 0 ]; then r=OK; echo . >> $OK; else r=FAIL; echo . >> $NO; fi
echo "    exit status \$?... $r"

who=$(echo world)
if [ "$who" = world ]; then r=OK; echo . >> $OK; else r=FAIL; echo . >> $NO; fi
echo "    command subst... $r"

count=$(echo abcde | wc -l)
if [ "$count" -eq 1 ]; then r=OK; echo . >> $OK; else r=FAIL; echo . >> $NO; fi
echo "    pipes... $r"

echo persisted > demo_data.tmp
data=$(cat demo_data.tmp)
if [ "$data" = persisted ]; then r=OK; echo . >> $OK; else r=FAIL; echo . >> $NO; fi
echo "    redirection... $r"

good=$(wc -l < $OK)
bad=$(wc -l < $NO)
echo ""
echo Total: $good OK, $bad FAIL

rm -f $OK $NO demo_data.tmp
