# 30_redir_pipe.sh - I/O redirection and pipes writing to real files.

d=$RT_TMP/redir
rm -rf "$d"; mkdir -p "$d"

# > truncates, >> appends.
echo first  > "$d/r.txt"
echo second >> "$d/r.txt"
assert_num 2 "$(wc -l < "$d/r.txt")" "> then >>"
assert_eq first  "$(head -n 1 "$d/r.txt")" "truncate wrote first line"
assert_eq second "$(tail -n 1 "$d/r.txt")" "append wrote second line"

# > truncates an existing file back down.
echo only > "$d/r.txt"
assert_num 1 "$(wc -l < "$d/r.txt")" "> truncates"

# < input redirection.
assert_eq only "$(cat < "$d/r.txt")" "< input redirection"

# Pipeline into a file, preserving order.
printf 'c\na\nb\n' | cat > "$d/p.txt"
assert_eq "$(printf 'c\na\nb')" "$(cat "$d/p.txt")" "pipe preserves order"

# Multi-stage pipe.
assert_num 3 "$(printf 'x\ny\nz\n' | cat | wc -l)" "multi-stage pipe"

rm -rf "$d"
