/* mp_vis: playback-state snapshot ring (A/V sync) and the animated
 * visualization state -- synthesized spectrum bins and per-channel VU --
 * updated per displayed frame. Integer math only; no audio, no tty.
 *
 * Sync model: the audio loop pushes one mp_snap per rendered chunk, tagged
 * with the stream position (total frames written so far). The draw side
 * asks for the snapshot at `frames_written - frames_queued`, i.e. what the
 * speaker is playing right now, and feeds it to mp_vis_step once per
 * displayed frame. */
#ifndef MP_VIS_H
#define MP_VIS_H

#include <stdbool.h>
#include <stdint.h>

#include "mp_mod.h"

#define MP_VIS_CHANNELS 8   /* tracked channels (display uses first 4) */
#define MP_SNAP_RING    64
#define MP_MAX_BINS     64
#define MP_ENERGY_MAX   (1 << 14)

typedef struct {
    uint32_t pos;      /* stream frames written before this chunk */
    int8_t pattern;    /* order position */
    int8_t line;
    uint16_t period[MP_VIS_CHANNELS];
    uint8_t volume[MP_VIS_CHANNELS]; /* real_volume 0..64 */
    uint8_t sample[MP_VIS_CHANNELS]; /* current sample 0..31 */
} mp_snap;

typedef struct {
    mp_snap ring[MP_SNAP_RING];
    int head, count;
} mp_snap_ring;

void mp_snap_push(mp_snap_ring *r, const mp_snap *s);
/* Latest snapshot with pos <= play_pos (or the oldest one available).
 * NULL when empty. */
const mp_snap *mp_snap_at(const mp_snap_ring *r, uint32_t play_pos);

typedef struct {
    int nbins;
    int energy[MP_MAX_BINS];  /* 0..MP_ENERGY_MAX */
    int peak[MP_MAX_BINS];    /* peak-hold, same scale */
    int peak_age[MP_MAX_BINS];
    int vu[MP_VIS_CHANNELS];  /* 0..MP_ENERGY_MAX */
    int last_pattern, last_line; /* line-change detection */
} mp_vis;

void mp_vis_init(mp_vis *v, int nbins);

/* Advance one displayed frame (~30 fps): decay bins/peaks/VU, feed sustain
 * from the snapshot's channel volumes, and on a pattern-line change inject
 * note attacks looked up from the MOD pattern data. `snap` may be NULL
 * (e.g. drained while paused): everything just decays. */
void mp_vis_step(mp_vis *v, const mp_mod *m, const mp_snap *snap);

#endif /* MP_VIS_H */
