#include "mp_vis.h"

void mp_snap_push(mp_snap_ring *r, const mp_snap *s)
{
    r->ring[r->head] = *s;
    r->head = (r->head + 1) % MP_SNAP_RING;
    if (r->count < MP_SNAP_RING) r->count++;
}

const mp_snap *mp_snap_at(const mp_snap_ring *r, uint32_t play_pos)
{
    const mp_snap *best = 0;
    for (int i = 0; i < r->count; i++) {
        int idx = (r->head - 1 - i + MP_SNAP_RING) % MP_SNAP_RING;
        const mp_snap *s = &r->ring[idx];
        if (s->pos <= play_pos) { best = s; break; } /* newest-first scan */
        best = s; /* everything is in the future: fall back to the oldest */
    }
    return best;
}

void mp_vis_init(mp_vis *v, int nbins)
{
    *v = (mp_vis){0};
    if (nbins < 1) nbins = 1;
    if (nbins > MP_MAX_BINS) nbins = MP_MAX_BINS;
    v->nbins = nbins;
    v->last_pattern = -1;
    v->last_line = -1;
}

static int note_bin(const mp_vis *v, int period)
{
    int note = mp_note_index(period);
    if (note < 0) return -1;
    return note * (v->nbins - 1) / (MP_NOTE_COUNT - 1);
}

static void inject(mp_vis *v, int bin, int amount)
{
    /* Center hit plus decreasing bleed into the neighbors. */
    static const int shift[5] = {2, 1, 0, 1, 2};
    for (int i = -2; i <= 2; i++) {
        int b = bin + i;
        if (b < 0 || b >= v->nbins) continue;
        int e = v->energy[b] + (amount >> shift[i + 2]);
        v->energy[b] = e > MP_ENERGY_MAX ? MP_ENERGY_MAX : e;
    }
}

void mp_vis_step(mp_vis *v, const mp_mod *m, const mp_snap *snap)
{
    /* Decay: bins fall fast, peaks hold then slide, VU sags slower. */
    for (int b = 0; b < v->nbins; b++) {
        v->energy[b] -= (v->energy[b] >> 3) + 1;
        if (v->energy[b] < 0) v->energy[b] = 0;
        if (v->peak[b] > 0 && ++v->peak_age[b] > 8) {
            v->peak[b] -= MP_ENERGY_MAX >> 5;
            if (v->peak[b] < 0) v->peak[b] = 0;
        }
    }
    for (int c = 0; c < MP_VIS_CHANNELS; c++) {
        v->vu[c] -= (v->vu[c] >> 4) + 1;
        if (v->vu[c] < 0) v->vu[c] = 0;
    }

    if (snap) {
        int nch = m->num_channels < MP_VIS_CHANNELS ? m->num_channels
                                                    : MP_VIS_CHANNELS;
        bool new_line = snap->pattern != v->last_pattern ||
                        snap->line != v->last_line;

        for (int c = 0; c < nch; c++) {
            int vol = snap->volume[c]; /* 0..64 */

            /* Sustain: playing channels keep their bin warm. */
            int bin = note_bin(v, snap->period[c]);
            if (bin >= 0 && vol > 0) {
                int floor_e = vol << 6; /* 64 -> 4096, ~1/4 height */
                if (v->energy[bin] < floor_e) v->energy[bin] = floor_e;
            }

            /* Attack: a fresh note on this pattern line kicks its bin and
             * the VU to (near) full, scaled by channel volume. */
            if (new_line && snap->pattern >= 0) {
                mp_cell cell = mp_mod_cell(m, snap->pattern, snap->line, c);
                if (cell.period) {
                    int hit_bin = note_bin(v, cell.period);
                    int amount = (vol ? vol : 64) << 8; /* 64 -> 16384 */
                    if (hit_bin >= 0) inject(v, hit_bin, amount);
                    if (v->vu[c] < amount) v->vu[c] = amount;
                }
            }

            /* VU floor follows the channel's running volume. */
            int vu_floor = vol << 7; /* 64 -> 8192, half scale */
            if (v->vu[c] < vu_floor) v->vu[c] = vu_floor;
        }
        v->last_pattern = snap->pattern;
        v->last_line = snap->line;
    }

    /* Peak-hold trails the bins. */
    for (int b = 0; b < v->nbins; b++) {
        if (v->energy[b] >= v->peak[b]) {
            v->peak[b] = v->energy[b];
            v->peak_age[b] = 0;
        }
    }
}
