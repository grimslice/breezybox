#include "breezy_cmd.h"
#include "breezy_vfs.h"
#include <stdio.h>
#include <string.h>

int cmd_cat(int argc, char **argv)
{
    char buf[128];
    size_t n;

    if (argc < 2) {
        // No file argument: copy stdin to stdout (POSIX cat), so `cat < file`
        // and `... | cat` work under the shell's stream-swapping redirection.
        while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0) {
            fwrite(buf, 1, n, stdout);
        }
        fflush(stdout);
        return 0;
    }

    char resolved[BREEZYBOX_MAX_PATH * 2 + 2];
    const char *path = argv[1];

    if (path[0] != '/') {
        if (!breezybox_resolve_path(path, resolved, sizeof(resolved))) {
            printf("cat: path too long\n");
            return 1;
        }
        path = resolved;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("cat: %s: No such file\n", argv[1]);
        return 1;
    }

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        fwrite(buf, 1, n, stdout);
    }
    fclose(f);
    fflush(stdout);

    return 0;
}