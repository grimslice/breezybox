# 40_tar.sh - tar create / list / extract round-trip (tar is an eget-installed
# ELF app; the whole file skips if it is absent).

if ! have tar; then
    skip "tar tests (tar not installed -- eget valdanylchuk/breezybox)"
    return 0 2>/dev/null || exit 0
fi

d=$RT_TMP/tar
rm -rf "$d"; mkdir -p "$d/src" "$d/out"

# A small tree to archive.
printf 'alpha\n'      > "$d/src/a.txt"
printf 'beta\ngamma\n' > "$d/src/b.txt"
mkdir -p "$d/src/nested"
printf 'deep\n'      > "$d/src/nested/c.txt"

# Create. The interpreter's ( ) does not isolate cwd, so save and restore it.
owd=$(pwd); cd "$d"; tar czf bundle.tgz src; cd "$owd"
assert_file "$d/bundle.tgz"

# List should mention the member files.
tar tzf "$d/bundle.tgz" > "$d/list.txt"
if have grep; then
    assert_ok grep a.txt "$d/list.txt"
    assert_ok grep nested "$d/list.txt"
else
    assert_file "$d/list.txt"
fi

# Extract into a fresh dir and verify byte-identical round-trip ($d is absolute).
owd=$(pwd); cd "$d/out"; tar xzf "$d/bundle.tgz"; cd "$owd"
assert_cksum "$d/src/a.txt"        "$d/out/src/a.txt"
assert_cksum "$d/src/b.txt"        "$d/out/src/b.txt"
assert_cksum "$d/src/nested/c.txt" "$d/out/src/nested/c.txt"

rm -rf "$d"
