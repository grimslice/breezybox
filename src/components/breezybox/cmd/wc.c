#include "breezy_cmd.h"
#include "breezy_vfs.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

int cmd_wc(int argc, char **argv)
{
    int show_lines = 0, show_words = 0, show_chars = 0;
    const char *filename = NULL;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (int j = 1; argv[i][j]; j++) {
                switch (argv[i][j]) {
                    case 'l': show_lines = 1; break;
                    case 'w': show_words = 1; break;
                    case 'c': show_chars = 1; break;
                }
            }
        } else {
            filename = argv[i];
        }
    }
    
    // Default: show all
    if (!show_lines && !show_words && !show_chars) {
        show_lines = show_words = show_chars = 1;
    }
    
    FILE *f;
    char resolved[BREEZYBOX_MAX_PATH * 2 + 2];
    if (filename) {
        const char *path = filename;
        if (path[0] != '/') {
            if (!breezybox_resolve_path(path, resolved, sizeof(resolved))) {
                printf("wc: path too long\n");
                return 1;
            }
            path = resolved;
        }
        f = fopen(path, "r");
        if (!f) {
            printf("wc: %s: No such file\n", filename);
            return 1;
        }
    } else {
        // No file: read from stdin
        f = stdin;
    }

    long lines = 0, words = 0, chars = 0;
    int in_word = 0;
    int c;
    
    while ((c = fgetc(f)) != EOF) {
        chars++;
        
        if (c == '\n') {
            lines++;
        }
        
        if (isspace(c)) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            words++;
        }
    }
    
    if (f != stdin) {
        fclose(f);
    }

    // Print results, space-separated with no leading/trailing padding
    int first = 1;
    if (show_lines) { printf("%ld", lines); first = 0; }
    if (show_words) { printf("%s%ld", first ? "" : " ", words); first = 0; }
    if (show_chars) { printf("%s%ld", first ? "" : " ", chars); first = 0; }
    if (filename) {
        printf(" %s", filename);
    }
    printf("\n");
    
    return 0;
}
