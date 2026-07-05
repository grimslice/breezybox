#include "breezy_cmd.h"
#include "breezy_vfs.h"
#include <stdio.h>
#include <stdint.h>

// POSIX cksum CRC (polynomial 0x04C11DB7), computed bitwise to avoid a table.
static uint32_t crc_update(uint32_t crc, int byte)
{
    crc ^= (uint32_t)(byte & 0xff) << 24;
    for (int i = 0; i < 8; i++) {
        crc = (crc & 0x80000000u) ? (crc << 1) ^ 0x04C11DB7u : (crc << 1);
    }
    return crc;
}

static void cksum_stream(FILE *f, const char *name)
{
    uint32_t crc = 0;
    long n = 0;
    int c;
    while ((c = fgetc(f)) != EOF) { crc = crc_update(crc, c); n++; }
    // Feed the byte count, low-order first, then invert -- per POSIX cksum.
    for (long len = n; len; len >>= 8) crc = crc_update(crc, (int)(len & 0xff));
    crc = ~crc;
    if (name) printf("%lu %ld %s\n", (unsigned long)crc, n, name);
    else      printf("%lu %ld\n", (unsigned long)crc, n);
}

int cmd_cksum(int argc, char **argv)
{
    if (argc < 2) { cksum_stream(stdin, NULL); return 0; }

    int rc = 0;
    char resolved[BREEZYBOX_MAX_PATH * 2 + 2];
    for (int i = 1; i < argc; i++) {
        const char *path = argv[i];
        if (path[0] != '/') {
            if (!breezybox_resolve_path(path, resolved, sizeof(resolved))) {
                printf("cksum: %s: path too long\n", argv[i]);
                rc = 1;
                continue;
            }
            path = resolved;
        }
        FILE *f = fopen(path, "r");
        if (!f) {
            printf("cksum: %s: No such file\n", argv[i]);
            rc = 1;
            continue;
        }
        cksum_stream(f, argv[i]);
        fclose(f);
    }
    return rc;
}
