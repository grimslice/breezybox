# 10_fs.sh - filesystem commands against the real VFS: mkdir/cp/mv/rm/ls, nested
# dirs, and copy integrity via cksum.

d=$RT_TMP/fs
rm -rf "$d"; mkdir -p "$d"

# mkdir, including a nested path.
assert_ok  mkdir "$d/sub"
assert_ok  mkdir "$d/sub/deep"
assert_ok  test -d "$d/sub/deep"

# Create a file, copy it, verify the copy is byte-identical.
printf 'line one\nline two\nline three\n' > "$d/a.txt"
assert_file "$d/a.txt"
assert_ok   cp "$d/a.txt" "$d/b.txt"
assert_cksum "$d/a.txt" "$d/b.txt"

# Move (rename): source gone, dest present with same content.
assert_ok   mv "$d/b.txt" "$d/c.txt"
assert_fail test -f "$d/b.txt"
assert_cksum "$d/a.txt" "$d/c.txt"

# ls sees both remaining files (needs grep to scan the listing).
if have grep; then
    ls "$d" > "$d/ls.out"
    assert_ok grep a.txt "$d/ls.out"
    assert_ok grep c.txt "$d/ls.out"
else
    skip "ls-content check (grep not installed)"
fi

# rm a single file, then a directory tree with -r.
assert_ok   rm "$d/c.txt"
assert_fail test -f "$d/c.txt"
assert_ok   rm -r "$d/sub"
assert_fail test -d "$d/sub"

# Removing a nonexistent file should fail.
assert_fail rm "$d/does_not_exist"

rm -rf "$d"
