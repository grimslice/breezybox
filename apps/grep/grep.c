/*
 * grep.c - Minimal fixed-string grep for ESP32-BreezyBox
 *
 * Usage: grep [-i] [-v] [-c] [-n] PATTERN [file...]
 *
 * Prints lines containing PATTERN (a literal substring, like grep -F -- no
 * regex, kept lean on purpose). Reads stdin when no file is given.
 *   -i  case-insensitive   -v  invert match
 *   -c  print match count   -n  prefix line numbers
 * Exit 0 if any line matched, 1 if none, 2 on error (GNU convention).
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

static int ci_match(const char *hay, const char *needle)
{
    if (!*needle) return 1;
    for (; *hay; hay++) {
        const char *h = hay, *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) { h++; n++; }
        if (!*n) return 1;
    }
    return 0;
}

static int grep_stream(FILE *f, const char *pat, const char *name, int show_name,
                       int icase, int invert, int count, int numbers)
{
    char buf[512];
    int line = 0, matches = 0;
    while (fgets(buf, sizeof(buf), f)) {
        line++;
        int hit = icase ? ci_match(buf, pat) : (strstr(buf, pat) != NULL);
        if (hit == invert) continue;
        matches++;
        if (count) continue;
        if (show_name) printf("%s:", name);
        if (numbers) printf("%d:", line);
        fputs(buf, stdout);
        if (buf[strlen(buf) - 1] != '\n') putchar('\n');
    }
    if (count) {
        if (show_name) printf("%s:", name);
        printf("%d\n", matches);
    }
    return matches;
}

int main(int argc, char **argv)
{
    int icase = 0, invert = 0, count = 0, numbers = 0;
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        for (int j = 1; argv[i][j]; j++) {
            switch (argv[i][j]) {
                case 'i': icase = 1; break;
                case 'v': invert = 1; break;
                case 'c': count = 1; break;
                case 'n': numbers = 1; break;
                default: fprintf(stderr, "grep: unknown option -%c\n", argv[i][j]); return 2;
            }
        }
    }
    if (i >= argc) { printf("Usage: grep [-ivcn] PATTERN [file...]\n"); return 2; }

    const char *pat = argv[i++];
    int total = 0, rc = 0;

    if (i >= argc) {
        total = grep_stream(stdin, pat, "(stdin)", 0, icase, invert, count, numbers);
    } else {
        int show_name = (argc - i) > 1;
        for (; i < argc; i++) {
            FILE *f = fopen(argv[i], "r");
            if (!f) { fprintf(stderr, "grep: %s: cannot open\n", argv[i]); rc = 2; continue; }
            total += grep_stream(f, pat, argv[i], show_name, icase, invert, count, numbers);
            fclose(f);
        }
    }
    if (rc) return rc;
    return total > 0 ? 0 : 1;
}
