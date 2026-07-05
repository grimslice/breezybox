# 50_gzip.sh - gzip / gunzip round-trip (both are eget-installed ELF apps).

if ! have gzip || ! have gunzip; then
    skip "gzip tests (gzip/gunzip not installed)"
    return 0
fi

d=$RT_TMP/gz
rm -rf "$d"; mkdir -p "$d"

# Repetitive content so it actually compresses.
: > "$d/orig.txt"
i=0
while [ "$i" -lt 20 ]; do
    echo "the quick brown fox jumps over the lazy dog" >> "$d/orig.txt"
    i=$(( i + 1 ))
done

# gzip -> gunzip and confirm the recovered file matches the original checksum.
assert_ok  gzip "$d/orig.txt" "$d/orig.txt.gz"
assert_file "$d/orig.txt.gz"
assert_ok  gunzip "$d/orig.txt.gz" "$d/restored.txt"
assert_cksum "$d/orig.txt" "$d/restored.txt"

rm -rf "$d"
