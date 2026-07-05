/*
 * tar.c - Minimal tar (+ gzip) for ESP32-BreezyBox
 *
 * Usage:
 *   tar xzf bundle.tgz [dir]   extract a .tar.gz / .tgz (into dir, default .)
 *   tar xf  bundle.tar [dir]   extract a plain .tar
 *   tar tzf bundle.tgz         list contents
 *   tar czf bundle.tgz f...    create a .tar.gz from files/dirs (recursive)
 *   tar cf  bundle.tar  f...    create a plain .tar
 *
 * The mode string is classic-tar style: one of c/t/x, optional z (gzip),
 * and a trailing f (the next argument is the archive). The main use case is
 * unpacking multi-file test bundles: `tar xzf bundle.tgz`.
 *
 * Built on microtar (rxi, MIT). Gzip streaming uses zlib's gzFile API, which
 * the firmware exports (gzseek included).
 *
 * ELF apps run on a small (~8KB) stack, so the big I/O buffers live in static
 * storage rather than on the stack.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#include "microtar.h"

/* ---- zlib gzFile API (resolved from firmware / system libz) ------------- */
typedef void *gzFile;
gzFile gzopen(const char *path, const char *mode);
int    gzread(gzFile file, void *buf, unsigned len);
int    gzwrite(gzFile file, const void *buf, unsigned len);
long   gzseek(gzFile file, long offset, int whence);
int    gzclose(gzFile file);

/* Shared file-copy buffer. Not reentrant, but we only ever process one file
 * at a time, so a single static buffer keeps it off the tiny ELF stack. */
static char iobuf[4096];

/* ---- microtar stream backed by a gzFile --------------------------------- */
static int gz_read(mtar_t *tar, void *data, unsigned size) {
    return gzread((gzFile)tar->stream, data, size) == (int)size
        ? MTAR_ESUCCESS : MTAR_EREADFAIL;
}
static int gz_write(mtar_t *tar, const void *data, unsigned size) {
    return gzwrite((gzFile)tar->stream, data, size) == (int)size
        ? MTAR_ESUCCESS : MTAR_EWRITEFAIL;
}
static int gz_seek(mtar_t *tar, unsigned offset) {
    return gzseek((gzFile)tar->stream, (long)offset, SEEK_SET) >= 0
        ? MTAR_ESUCCESS : MTAR_ESEEKFAIL;
}
static int gz_close(mtar_t *tar) {
    gzclose((gzFile)tar->stream);
    return MTAR_ESUCCESS;
}

/* Open a gzip-backed tar stream. mode is "r" or "w". */
static int gz_open(mtar_t *tar, const char *filename, const char *mode) {
    memset(tar, 0, sizeof(*tar));
    tar->read = gz_read;
    tar->write = gz_write;
    tar->seek = gz_seek;
    tar->close = gz_close;
    tar->stream = gzopen(filename, *mode == 'w' ? "wb" : "rb");
    return tar->stream ? MTAR_ESUCCESS : MTAR_EOPENFAIL;
}

/* ---- helpers ------------------------------------------------------------ */

/* Create every parent directory in a path (mkdir -p of the dirname). */
static void make_parents(char *path) {
    for (char *p = path + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(path, 0775);
            *p = '/';
        }
    }
}

/* ---- extract / list ----------------------------------------------------- */

static int do_extract(mtar_t *tar, const char *destdir, int list_only) {
    static mtar_header_t h;
    static char path[512];
    int err;

    while ((err = mtar_read_header(tar, &h)) == MTAR_ESUCCESS) {
        if (destdir)
            snprintf(path, sizeof(path), "%s/%s", destdir, h.name);
        else
            snprintf(path, sizeof(path), "%s", h.name);

        if (list_only) {
            printf("%s\n", h.name);
        } else if (h.type == MTAR_TDIR) {
            make_parents(path);
            mkdir(path, 0775);
        } else {
            make_parents(path);
            FILE *out = fopen(path, "wb");
            if (!out) {
                printf("tar: cannot create %s\n", path);
                return 1;
            }
            unsigned left = h.size;
            while (left > 0) {
                unsigned n = left < sizeof(iobuf) ? left : sizeof(iobuf);
                if (mtar_read_data(tar, iobuf, n) != MTAR_ESUCCESS) {
                    printf("tar: read error on %s\n", h.name);
                    fclose(out);
                    return 1;
                }
                fwrite(iobuf, 1, n, out);
                left -= n;
            }
            fclose(out);
            printf("x %s\n", h.name);
        }
        mtar_next(tar);
    }
    if (err != MTAR_ENULLRECORD) {
        printf("tar: %s\n", mtar_strerror(err));
        return 1;
    }
    return 0;
}

/* ---- create ------------------------------------------------------------- */

/* Add one regular file's header + data to the archive. */
static int add_regular(mtar_t *tar, const char *name, unsigned size) {
    FILE *in = fopen(name, "rb");
    if (!in) {
        printf("tar: cannot open %s\n", name);
        return 1;
    }
    mtar_write_file_header(tar, name, size);
    size_t n;
    while ((n = fread(iobuf, 1, sizeof(iobuf), in)) > 0)
        mtar_write_data(tar, iobuf, n);
    fclose(in);
    printf("a %s\n", name);
    return 0;
}

/* Recursively add a file or directory tree. */
static int add_path(mtar_t *tar, const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        printf("tar: cannot stat %s\n", path);
        return 1;
    }
    /* microtar's name field is 100 bytes; skip anything that won't fit. */
    if (strlen(path) >= 100) {
        printf("tar: path too long, skipping %s\n", path);
        return 0;
    }

    if (!S_ISDIR(st.st_mode))
        return add_regular(tar, path, (unsigned)st.st_size);

    mtar_write_dir_header(tar, path);
    printf("a %s/\n", path);

    DIR *d = opendir(path);
    if (!d) {
        printf("tar: cannot open dir %s\n", path);
        return 1;
    }
    int rc = 0;
    struct dirent *e;
    while (rc == 0 && (e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        char child[256];
        snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
        rc = add_path(tar, child);
    }
    closedir(d);
    return rc;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: tar {c|t|x}[z]f <archive> [files/dir...]\n");
        return 1;
    }

    const char *mode = argv[1];
    const char *archive = argv[2];
    int op = 0, gz = 0;
    for (const char *p = mode; *p; p++) {
        if (*p == 'c' || *p == 't' || *p == 'x') op = *p;
        else if (*p == 'z') gz = 1;
        else if (*p == 'f') /* archive follows */;
        else { printf("tar: unknown flag '%c'\n", *p); return 1; }
    }
    if (!op) { printf("tar: specify c, t, or x\n"); return 1; }

    mtar_t tar;
    int err;

    if (op == 'c') {
        err = gz ? gz_open(&tar, archive, "w") : mtar_open(&tar, archive, "w");
        if (err != MTAR_ESUCCESS) {
            printf("tar: cannot create %s: %s\n", archive, mtar_strerror(err));
            return 1;
        }
        int rc = 0;
        for (int i = 3; i < argc && rc == 0; i++)
            rc = add_path(&tar, argv[i]);
        mtar_finalize(&tar);
        mtar_close(&tar);
        return rc;
    }

    /* extract or list */
    if (gz)
        err = gz_open(&tar, archive, "r");
    else
        err = mtar_open(&tar, archive, "r");
    if (err != MTAR_ESUCCESS) {
        printf("tar: cannot open %s: %s\n", archive, mtar_strerror(err));
        return 1;
    }
    const char *destdir = (op == 'x' && argc > 3) ? argv[3] : NULL;
    if (destdir) mkdir(destdir, 0775);
    int rc = do_extract(&tar, destdir, op == 't');
    mtar_close(&tar);
    return rc;
}
