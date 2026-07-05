/*
 * diff.c - Minimal line diff for ESP32-BreezyBox
 *
 * Usage: diff <a> <b>
 *
 * Compares two text files line by line. Exit 0 if identical, 1 if they differ
 * (like GNU diff), 2 on error. To stay lean it trims the common prefix and
 * suffix and reports the differing middle as a single normal-diff hunk -- enough
 * to drive test assertions ("actual == expected") and show what changed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 2000

static int read_lines(const char *path, char ***out)
{
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "diff: %s: cannot open\n", path); return -1; }
    char **lines = calloc(MAX_LINES, sizeof(char *));
    if (!lines) { fclose(f); return -1; }
    int n = 0;
    char buf[512];
    while (n < MAX_LINES && fgets(buf, sizeof(buf), f)) {
        size_t len = strlen(buf);
        if (len && buf[len - 1] == '\n') buf[--len] = '\0';
        lines[n] = malloc(len + 1);
        if (!lines[n]) break;
        memcpy(lines[n], buf, len + 1);
        n++;
    }
    fclose(f);
    *out = lines;
    return n;
}

int main(int argc, char **argv)
{
    if (argc < 3) { printf("Usage: diff <a> <b>\n"); return 2; }

    char **a, **b;
    int na = read_lines(argv[1], &a);
    if (na < 0) return 2;
    int nb = read_lines(argv[2], &b);
    if (nb < 0) return 2;

    // Common prefix.
    int p = 0;
    while (p < na && p < nb && strcmp(a[p], b[p]) == 0) p++;
    // Common suffix (not overlapping the prefix).
    int sa = na, sb = nb;
    while (sa > p && sb > p && strcmp(a[sa - 1], b[sb - 1]) == 0) { sa--; sb--; }

    int differ = (p < sa) || (p < sb);
    if (differ) {
        // Normal-diff header: <a-range>c<b-range> (1-based, inclusive).
        printf("%d", p + 1);
        if (sa - 1 > p) printf(",%d", sa);
        printf("c%d", p + 1);
        if (sb - 1 > p) printf(",%d", sb);
        printf("\n");
        for (int i = p; i < sa; i++) printf("< %s\n", a[i]);
        if (p < sa && p < sb) printf("---\n");
        for (int i = p; i < sb; i++) printf("> %s\n", b[i]);
    }
    return differ ? 1 : 0;
}
