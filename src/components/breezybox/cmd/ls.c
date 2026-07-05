#include "breezy_cmd.h"
#include "breezy_vfs.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

static void print_entry(const char *name, const struct stat *st)
{
    if (st && S_ISDIR(st->st_mode)) {
        printf("%-20s  <DIR>\n", name);
    } else if (st) {
        printf("%-20s  %7ld\n", name, st->st_size);
    } else {
        printf("%-20s\n", name);
    }
}

// List one operand: a directory lists its entries (with a `name:` header when
// there are several operands, ls-style), a file prints its own line.
static int ls_one(const char *arg, int multiple)
{
    char resolved[BREEZYBOX_MAX_PATH * 2 + 2];
    const char *path = arg;

    if (path[0] != '/') {
        if (!breezybox_resolve_path(path, resolved, sizeof(resolved))) {
            printf("ls: %s: path too long\n", arg);
            return 1;
        }
        path = resolved;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        printf("ls: cannot access '%s'\n", arg);
        return 1;
    }

    if (!S_ISDIR(st.st_mode)) {
        print_entry(arg, &st);
        return 0;
    }

    DIR *dir = opendir(path);
    if (!dir) {
        printf("ls: cannot access '%s'\n", arg);
        return 1;
    }

    if (multiple) printf("%s:\n", arg);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char entry_path[BREEZYBOX_MAX_PATH * 2 + 258];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", path, entry->d_name);

        if (stat(entry_path, &st) == 0) {
            print_entry(entry->d_name, &st);
        } else {
            print_entry(entry->d_name, NULL);
        }
    }
    closedir(dir);
    return 0;
}

int cmd_ls(int argc, char **argv)
{
    if (argc < 2) {
        return ls_one(breezybox_cwd(), 0);
    }
    int ret = 0;
    for (int i = 1; i < argc; i++) {
        if (ls_one(argv[i], argc > 2)) ret = 1;
    }
    return ret;
}
