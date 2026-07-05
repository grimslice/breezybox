# 60_tools.sh - exercise the test-support commands themselves:
# printf/cksum (builtins) and diff/grep (eget-installed ELF apps).

d=$RT_TMP/tools
rm -rf "$d"; mkdir -p "$d"

# --- printf (builtin) ---
assert_eq "name=box val=42" "$(printf 'name=%s val=%d' box 42)" "printf %s %d"
assert_eq "a	b" "$(printf 'a\tb')" "printf \\t escape"
assert_eq "100%" "$(printf '%d%%' 100)" "printf %% literal"

# --- cksum (builtin): same bytes -> same checksum, different -> different ---
printf 'hello world' > "$d/h1"
printf 'hello world' > "$d/h2"
printf 'hello worlD' > "$d/h3"
assert_eq "$(cksum < "$d/h1")" "$(cksum < "$d/h2")" "cksum stable"
assert_fail test "$(cksum < "$d/h1")" = "$(cksum < "$d/h3")"

# --- diff (ELF app) ---
if have diff; then
    printf 'alpha\nbeta\ngamma\n' > "$d/a"
    printf 'alpha\nbeta\ngamma\n' > "$d/b"
    printf 'alpha\nBETA\ngamma\n' > "$d/c"
    assert_ok   diff "$d/a" "$d/b"
    assert_fail diff "$d/a" "$d/c"
    # Multi-line output check against a committed fixture.
    diff "$d/a" "$d/c" > "$d/diff.out"
    assert_diff "$RT_FIX/diff_expected.txt" "$d/diff.out"
else
    skip "diff tests (diff not installed)"
fi

# --- grep (ELF app) ---
if have grep; then
    printf 'apple\nBanana\ncherry\n' > "$d/fruit"
    assert_ok   grep apple "$d/fruit"
    assert_fail grep zzz   "$d/fruit"
    assert_num 1 "$(grep -c cherry "$d/fruit")" "grep -c"
    assert_eq Banana "$(grep -i banana "$d/fruit")" "grep -i (case-insensitive)"
    assert_num 2 "$(grep -v apple "$d/fruit" | wc -l)" "grep -v"
else
    skip "grep tests (grep not installed)"
fi

rm -rf "$d"
