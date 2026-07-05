/*
 * cp.c - Copy files
 *
 * Usage: cp <source> <dest>
 *        cp <source...> <dir>
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "breezy_vfs.h"

#define COPY_BUF_SIZE 512

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// Join dir + basename(src_arg) into buf.
static void dest_in_dir(const char *dir, const char *src_arg,
                        char *buf, size_t size)
{
    const char *filename = strrchr(src_arg, '/');
    filename = filename ? filename + 1 : src_arg;
    size_t len = strlen(dir);
    snprintf(buf, size, "%s%s%s", dir,
             (len > 0 && dir[len - 1] != '/') ? "/" : "", filename);
}

// Copy one file; src_arg/dst_arg are the user-visible names for messages.
static int copy_one(const char *src_arg, const char *src_path,
                    const char *dst_arg, const char *dst_path)
{
    struct stat st;
    if (stat(src_path, &st) != 0) {
        printf("cp: cannot stat '%s': No such file\n", src_arg);
        return 1;
    }
    if (S_ISDIR(st.st_mode)) {
        printf("cp: '%s' is a directory (not supported)\n", src_arg);
        return 1;
    }

    FILE *src = fopen(src_path, "rb");
    if (!src) {
        printf("cp: cannot open '%s'\n", src_arg);
        return 1;
    }

    FILE *dst = fopen(dst_path, "wb");
    if (!dst) {
        printf("cp: cannot create '%s'\n", dst_arg);
        fclose(src);
        return 1;
    }

    char buf[COPY_BUF_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, bytes_read, dst) != bytes_read) {
            printf("cp: write error\n");
            fclose(src);
            fclose(dst);
            return 1;
        }
    }

    fclose(src);
    fclose(dst);
    return 0;
}

int cmd_cp(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: cp <source...> <dest>\n");
        return 1;
    }

    char dst_path[256];
    breezybox_resolve_path(argv[argc - 1], dst_path, sizeof(dst_path));
    int dst_is_dir = is_dir(dst_path);

    if (argc > 3 && !dst_is_dir) {
        printf("cp: target '%s' is not a directory\n", argv[argc - 1]);
        return 1;
    }

    int ret = 0;
    for (int i = 1; i < argc - 1; i++) {
        char src_path[256], dst_file[256];
        breezybox_resolve_path(argv[i], src_path, sizeof(src_path));
        if (dst_is_dir) {
            dest_in_dir(dst_path, argv[i], dst_file, sizeof(dst_file));
        } else {
            strncpy(dst_file, dst_path, sizeof(dst_file) - 1);
            dst_file[sizeof(dst_file) - 1] = '\0';
        }
        if (copy_one(argv[i], src_path, argv[argc - 1], dst_file)) ret = 1;
    }
    return ret;
}
