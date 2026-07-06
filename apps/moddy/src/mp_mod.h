/* mp_mod: read-only view of a loaded MOD file for display purposes --
 * title, sample names, pattern order, and per-cell decode (note name,
 * sample number, effect). Pure data, no audio, no tty: snapshot-testable.
 *
 * Works alongside pocketmod (which does the playing); this module parses
 * the same buffer independently so the view code never touches
 * pocketmod_context internals. */
#ifndef MP_MOD_H
#define MP_MOD_H

#include <stdbool.h>
#include <stdint.h>

#define MP_MAX_SAMPLES 31
#define MP_NOTE_COUNT  36 /* C-1..B-3, protracker 3 octaves */

typedef struct {
    const uint8_t *data;   /* whole file */
    int size;
    char title[21];        /* sanitized, NUL-terminated */
    char sample_name[MP_MAX_SAMPLES][23];
    int num_samples;       /* 15 or 31 */
    int num_channels;      /* 1..8 typical */
    int order_len;         /* song length in the order table (1..128) */
    const uint8_t *order;  /* order table (128 bytes) */
    const uint8_t *patterns; /* start of pattern data */
    int num_patterns;
} mp_mod;

/* One decoded pattern cell. */
typedef struct {
    uint16_t period;  /* 0 = no note */
    uint8_t sample;   /* 0 = none, 1..31 */
    uint8_t effect;   /* 0x0..0xF */
    uint8_t param;
} mp_cell;

/* Parse header/metadata from an in-memory MOD file. Returns false if the
 * buffer is too small / obviously not a MOD. Keeps a pointer to `data`. */
bool mp_mod_parse(mp_mod *m, const void *data, int size);

/* Decode the cell at (order position, line 0..63, channel). Out-of-range
 * positions yield an all-zero cell. */
mp_cell mp_mod_cell(const mp_mod *m, int order_pos, int line, int ch);

/* Nearest note index 0..35 for a period (113..856), or -1 for period 0. */
int mp_note_index(int period);

/* "C-3", "A#2", ... for note index 0..35; "---" otherwise. `out` >= 4. */
void mp_note_name(int note, char out[4]);

#endif /* MP_MOD_H */
