# 15_glob.sh - pathname expansion (globbing) and how the core commands handle
# the multi-file argument lists it produces. Matches are sorted; an unmatched
# pattern stays literal (dash-style); quoting suppresses expansion.

d=$RT_TMP/glob
rm -rf "$d"; mkdir -p "$d" "$d/sub"

printf 'one\n'   > "$d/a1.txt"
printf 'two\n'   > "$d/a2.txt"
printf 'three\n' > "$d/b.txt"
printf 'junk\n'  > "$d/c.bak"
printf 'elf1\n'  > "$d/sub/s1.elf"
printf 'elf2\n'  > "$d/sub/s2.elf"

cd "$d"

# --- Expansion semantics -------------------------------------------------

assert_eq "a1.txt a2.txt b.txt" "$(echo *.txt)"      "glob expands sorted"
assert_eq "b.txt"               "$(echo ?.txt)"      "? matches one char"
assert_eq "*.zzz"               "$(echo *.zzz)"      "unmatched glob stays literal"
assert_eq "*.txt"               "$(echo '*.txt')"    "quoted glob not expanded"
assert_eq "sub/s1.elf sub/s2.elf" "$(echo sub/*.elf)" "glob with directory part"

# --- Commands fed a glob's multi-file argv -------------------------------

# ls: file operands (what a glob produces), including a dir-part pattern.
assert_ok ls *.txt
assert_ok ls sub/*.elf

# cat: concatenates all operands in glob (sorted) order.
assert_eq "$(printf 'one\ntwo\nthree')" "$(cat *.txt)" "cat glob concatenation"

# wc/cksum consume each operand.
assert_num 3 "$(cksum *.txt | wc -l)" "cksum glob: one line per file"

# cp glob into a directory.
mkdir "$d/dest"
assert_ok cp *.txt dest
assert_eq "dest/a1.txt dest/a2.txt dest/b.txt" "$(echo dest/*.txt)" "cp glob into dir"
assert_cksum "$d/a1.txt" "$d/dest/a1.txt"

# cp multiple sources to a non-directory must fail.
assert_fail cp *.txt "$d/a1.txt"

# mv glob into a directory: sources gone, targets present.
mkdir "$d/moved"
assert_ok mv sub/*.elf moved
assert_fail test -f "$d/sub/s1.elf"
assert_ok   test -f "$d/moved/s1.elf"
assert_ok   test -f "$d/moved/s2.elf"

# rm glob: removes all matches; the pattern then stays literal and rm fails.
assert_ok   rm *.bak
assert_fail test -f "$d/c.bak"
assert_fail rm *.bak

cd "$BASE"
rm -rf "$d"
