#include "breezy_cmd.h"
#include "breezy_vfs.h"
#include <stdio.h>
#include <string.h>

static int cat_one(const char *arg)
{
    char resolved[BREEZYBOX_MAX_PATH * 2 + 2];
    const char *path = arg;

    if (path[0] != '/') {
        if (!breezybox_resolve_path(path, resolved, sizeof(resolved))) {
            printf("cat: %s: path too long\n", arg);
            return 1;
        }
        path = resolved;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("cat: %s: No such file\n", arg);
        return 1;
    }

    char buf[128];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        fwrite(buf, 1, n, stdout);
    }
    fclose(f);
    return 0;
}

int cmd_cat(int argc, char **argv)
{
    if (argc < 2) {
        // No file argument: copy stdin to stdout (POSIX cat), so `cat < file`
        // and `... | cat` work under the shell's stream-swapping redirection.
        char buf[128];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0) {
            fwrite(buf, 1, n, stdout);
        }
        fflush(stdout);
        return 0;
    }

    int ret = 0;
    for (int i = 1; i < argc; i++) {
        if (cat_one(argv[i])) ret = 1;
    }
    fflush(stdout);
    return ret;
}
