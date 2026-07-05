# 20_textutils.sh - cat / head / tail / wc against real files.

d=$RT_TMP/txt
rm -rf "$d"; mkdir -p "$d"

# A known 5-line file.
printf 'one\ntwo\nthree\nfour\nfive\n' > "$d/f.txt"

# wc counts.
assert_num 5 "$(wc -l < "$d/f.txt")" "wc -l"
assert_num 5 "$(wc -w < "$d/f.txt")" "wc -w"

# cat round-trips content (compare against the source via cksum).
cat "$d/f.txt" > "$d/cat.out"
assert_cksum "$d/f.txt" "$d/cat.out"

# head -n 2 / tail -n 2.
assert_eq "one" "$(head -n 1 "$d/f.txt")" "head -n 1"
assert_eq "$(printf 'four\nfive')" "$(tail -n 2 "$d/f.txt")" "tail -n 2"

head -n 2 "$d/f.txt" > "$d/head.out"
assert_num 2 "$(wc -l < "$d/head.out")" "head line count"

rm -rf "$d"
