#include "breezy_cmd.h"
#include <stdio.h>
#include <stdlib.h>

// Minimal printf: supports %s %d/%i %c %%, and backslash escapes \n \t \r \\.
// The format is not recycled across extra args (kept lean); enough to build
// exact-byte test fixtures without echo's newline/escape quirks.
int cmd_printf(int argc, char **argv)
{
    if (argc < 2) return 0;
    int arg = 2;
    for (const char *p = argv[1]; *p; p++) {
        if (*p == '\\' && p[1]) {
            switch (*++p) {
                case 'n': putchar('\n'); break;
                case 't': putchar('\t'); break;
                case 'r': putchar('\r'); break;
                case '\\': putchar('\\'); break;
                default: putchar('\\'); putchar(*p); break;
            }
        } else if (*p == '%' && p[1]) {
            switch (*++p) {
                case 's': fputs(arg < argc ? argv[arg++] : "", stdout); break;
                case 'd': case 'i': printf("%d", arg < argc ? atoi(argv[arg++]) : 0); break;
                case 'c': { const char *s = arg < argc ? argv[arg++] : ""; putchar(s[0]); break; }
                case '%': putchar('%'); break;
                default: putchar('%'); putchar(*p); break;
            }
        } else {
            putchar(*p);
        }
    }
    return 0;
}
