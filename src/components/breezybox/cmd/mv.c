/*
 * mv.c - Move/rename files
 *
 * Usage: mv <source> <dest>
 *        mv <source...> <dir>
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "breezy_vfs.h"

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

// Move one file; src_arg/dst_arg are the user-visible names for messages.
static int move_one(const char *src_arg, const char *src_path,
                    const char *dst_arg, const char *dst_path)
{
    struct stat st;
    if (stat(src_path, &st) != 0) {
        printf("mv: cannot stat '%s': No such file or directory\n", src_arg);
        return 1;
    }

    // Try rename (works if same filesystem)
    if (rename(src_path, dst_path) == 0) {
        return 0;
    }

    // If rename failed and source is a directory, we can't easily move it
    if (S_ISDIR(st.st_mode)) {
        printf("mv: cannot move directory '%s'\n", src_arg);
        return 1;
    }

    // For files, fall back to copy + delete
    FILE *src = fopen(src_path, "rb");
    if (!src) {
        printf("mv: cannot open '%s'\n", src_arg);
        return 1;
    }

    FILE *dst = fopen(dst_path, "wb");
    if (!dst) {
        printf("mv: cannot create '%s'\n", dst_arg);
        fclose(src);
        return 1;
    }

    char buf[512];
    size_t bytes_read;
    int error = 0;

    while ((bytes_read = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, bytes_read, dst) != bytes_read) {
            error = 1;
            break;
        }
    }

    fclose(src);
    fclose(dst);

    if (error) {
        printf("mv: write error\n");
        remove(dst_path);  // Clean up partial file
        return 1;
    }

    // Delete source
    if (remove(src_path) != 0) {
        printf("mv: warning: copied but could not remove source\n");
    }

    return 0;
}

int cmd_mv(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: mv <source...> <dest>\n");
        return 1;
    }

    char dst_path[256];
    breezybox_resolve_path(argv[argc - 1], dst_path, sizeof(dst_path));
    int dst_is_dir = is_dir(dst_path);

    if (argc > 3 && !dst_is_dir) {
        printf("mv: target '%s' is not a directory\n", argv[argc - 1]);
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
        if (move_one(argv[i], src_path, argv[argc - 1], dst_file)) ret = 1;
    }
    return ret;
}
