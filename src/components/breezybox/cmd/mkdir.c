#include "breezy_cmd.h"
#include "breezy_vfs.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

// Create one directory, treating "already a directory" as success (errno is not
// reliable across the VFS wrappers, so confirm with stat instead).
static int ensure_dir(const char *path)
{
    if (mkdir(path, 0755) == 0) return 0;
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return 0;
    return -1;
}

// mkdir -p: create every missing parent of `path` (modified in place), then the
// leaf itself. Existing components are fine.
static int mkdir_parents(char *path)
{
    for (char *s = path + 1; *s; s++) {
        if (*s == '/') {
            *s = '\0';
            int rc = ensure_dir(path);
            *s = '/';
            if (rc != 0) return -1;
        }
    }
    return ensure_dir(path);
}

int cmd_mkdir(int argc, char **argv)
{
    int parents = 0;
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        for (int j = 1; argv[i][j]; j++) {
            if (argv[i][j] == 'p') parents = 1;
            else { printf("mkdir: invalid option -- '%c'\n", argv[i][j]); return 1; }
        }
    }
    if (i >= argc) {
        printf("Usage: mkdir [-p] <dir>...\n");
        return 1;
    }

    int errors = 0;
    char resolved[BREEZYBOX_MAX_PATH * 2 + 2];
    for (; i < argc; i++) {
        const char *path = argv[i];
        if (path[0] != '/') {
            if (!breezybox_resolve_path(argv[i], resolved, sizeof(resolved))) {
                printf("mkdir: %s: path too long\n", argv[i]);
                errors++;
                continue;
            }
            path = resolved;
        }
        int rc;
        if (parents) {
            char buf[BREEZYBOX_MAX_PATH * 2 + 2];
            snprintf(buf, sizeof(buf), "%s", path);
            rc = mkdir_parents(buf);
        } else {
            rc = mkdir(path, 0755);
        }
        if (rc != 0) {
            printf("mkdir: cannot create '%s'\n", argv[i]);
            errors++;
        }
    }
    return errors > 0 ? 1 : 0;
}
