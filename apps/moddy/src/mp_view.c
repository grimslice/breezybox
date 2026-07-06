#include "mp_view.h"

#include <stdio.h>
#include <string.h>

#include "tui_layout.h"

/* --- spectrum ------------------------------------------------------- */

/* 2-cell bars with 1-cell gaps, at least 1 bin. */
int mp_view_spectrum_bins(int width)
{
    int n = (width + 1) / 3;
    if (n < 1) n = 1;
    if (n > MP_MAX_BINS) n = MP_MAX_BINS;
    return n;
}

void mp_view_spectrum(tui_surface *s, tui_rect r, const mp_vis *vis)
{
    if (r.h < 1 || r.w < 1) return;
    /* Sub-cell resolution: 2 levels per row via ':' half vs '|' full. */
    int levels = r.h * 2;

    for (int b = 0; b < vis->nbins; b++) {
        int x0 = r.x + b * 3;
        if (x0 + 1 >= r.x + r.w) break;
        int lvl = vis->energy[b] * levels / MP_ENERGY_MAX;
        if (lvl > levels) lvl = levels;
        int plvl = vis->peak[b] * levels / MP_ENERGY_MAX;

        for (int row = 0; row < r.h; row++) {
            int y = r.y + r.h - 1 - row;          /* bottom-up */
            int cell_lvl = lvl - row * 2;         /* 2 levels per row */
            char ch;
            if (cell_lvl >= 2) ch = '|';
            else if (cell_lvl == 1) ch = ':';
            else ch = 0;

            /* Color by height: green -> yellow -> red (top quarter). */
            int fg = TUI_GREEN;
            if (row * 4 >= r.h * 3) fg = TUI_RED;
            else if (row * 2 >= r.h) fg = TUI_YELLOW;
            if (cell_lvl >= 2) fg |= TUI_BRIGHT;

            if (ch) {
                tui_put_char(s, r, x0, y, ch, TUI_ATTR(fg, TUI_BLACK));
                tui_put_char(s, r, x0 + 1, y, ch, TUI_ATTR(fg, TUI_BLACK));
            }
        }

        /* Peak-hold dot above the bar. */
        int prow = (plvl - 1) / 2;
        if (plvl > lvl && prow >= 0 && prow < r.h) {
            int y = r.y + r.h - 1 - prow;
            uint8_t pa = TUI_ATTR(TUI_WHITE | TUI_BRIGHT, TUI_BLACK);
            tui_put_char(s, r, x0, y, '.', pa);
            tui_put_char(s, r, x0 + 1, y, '.', pa);
        }
    }
}

/* --- separator / title line ----------------------------------------- */

void mp_view_title(tui_surface *s, tui_rect r, const mp_mod *m,
                   const mp_view_status *st, const mp_snap *snap)
{
    if (r.h < 1) return;
    uint8_t line_a = TUI_ATTR(TUI_CYAN, TUI_BLACK);
    uint8_t text_a = TUI_ATTR(TUI_WHITE | TUI_BRIGHT, TUI_BLACK);

    tui_hline(s, r, r.x, r.y, r.w, '-', line_a);

    char label[96];
    if (m->title[0])
        snprintf(label, sizeof(label), "[%d/%d %s: %s]", st->track,
                 st->ntracks, st->filename, m->title);
    else
        snprintf(label, sizeof(label), "[%d/%d %s]", st->track,
                 st->ntracks, st->filename);
    tui_put_str(s, r, r.x + 4, r.y, label, text_a);

    char pos[48];
    if (snap)
        snprintf(pos, sizeof(pos), "%sVol%d%% P%02d/%02d L%02d ",
                 st->paused ? "[PAUSED] " : "", st->vol_pct,
                 snap->pattern, m->order_len, snap->line);
    else
        snprintf(pos, sizeof(pos), "%sVol%d%% ",
                 st->paused ? "[PAUSED] " : "", st->vol_pct);
    int len = (int)strlen(pos);
    tui_put_str(s, r, r.x + r.w - len - 1, r.y, pos,
                st->paused ? TUI_ATTR(TUI_YELLOW | TUI_BRIGHT, TUI_BLACK)
                           : text_a);
}

/* --- track panel ----------------------------------------------------- */

/* Pattern position `delta` lines away from (pat, line), following the
 * order table across pattern boundaries. Returns false if off the song. */
static bool walk_pos(const mp_mod *m, int pat, int line, int delta,
                     int *out_pat, int *out_line)
{
    line += delta;
    while (line < 0) {
        if (--pat < 0) return false;
        line += 64;
    }
    while (line > 63) {
        if (++pat >= m->order_len) return false;
        line -= 64;
    }
    *out_pat = pat;
    *out_line = line;
    return true;
}

static void draw_track_row(tui_surface *s, tui_rect clip, int x, int y,
                           const mp_mod *m, int pat, int line, int ch,
                           bool current)
{
    mp_cell cell = mp_mod_cell(m, pat, line, ch);
    char note[4];
    mp_note_name(mp_note_index(cell.period), note);

    uint8_t bg = current ? TUI_CYAN : TUI_BLACK;
    uint8_t note_a, smp_a, fx_a;
    if (current) {
        note_a = smp_a = fx_a = TUI_ATTR(TUI_BLACK, bg);
    } else {
        bool empty = !cell.period && !cell.sample && !cell.effect &&
                     !cell.param;
        note_a = TUI_ATTR(empty ? TUI_BLACK | TUI_BRIGHT
                                : TUI_WHITE | TUI_BRIGHT, bg);
        smp_a = TUI_ATTR(empty ? TUI_BLACK | TUI_BRIGHT : TUI_CYAN, bg);
        fx_a = TUI_ATTR(empty ? TUI_BLACK | TUI_BRIGHT : TUI_MAGENTA, bg);
    }

    char buf[8];
    tui_put_str(s, clip, x, y, note, note_a);
    if (cell.sample)
        snprintf(buf, sizeof(buf), "%02d", cell.sample);
    else
        memcpy(buf, "..", 3);
    tui_put_str(s, clip, x + 4, y, buf, smp_a);
    if (cell.effect || cell.param)
        snprintf(buf, sizeof(buf), "%X%02X", cell.effect, cell.param);
    else
        memcpy(buf, "...", 4);
    tui_put_str(s, clip, x + 7, y, buf, fx_a);
}

void mp_view_tracks(tui_surface *s, tui_rect r, const mp_mod *m,
                    const mp_vis *vis, const mp_snap *snap)
{
    if (r.h < 2 || r.w < 8) return;
    int ncols = m->num_channels < MP_TRACK_COLS ? m->num_channels
                                                : MP_TRACK_COLS;
    if (ncols < 1) return;

    tui_rect cols[MP_TRACK_COLS];
    tui_constraint cons[MP_TRACK_COLS];
    for (int i = 0; i < ncols; i++) cons[i] = TUI_FILL;
    tui_layout_split(r, TUI_DIR_HORIZ, cons, ncols, cols);

    int pattern_rows = r.h - 1; /* last row = VU + sample name */
    int center = pattern_rows / 2;

    for (int c = 0; c < ncols; c++) {
        tui_rect col = cols[c];
        uint8_t dim = TUI_ATTR(TUI_BLACK | TUI_BRIGHT, TUI_BLACK);

        /* Column separator on the left edge (skip the first column). */
        int tx = col.x;
        if (c > 0) {
            tui_vline(s, col, col.x, col.y, col.h, '|', dim);
            tx = col.x + 2;
        }

        if (snap) {
            for (int row = 0; row < pattern_rows; row++) {
                int pat, line;
                if (!walk_pos(m, snap->pattern, snap->line, row - center,
                              &pat, &line))
                    continue;
                bool cur = row == center;
                if (cur) /* highlight bar across the text width */
                    tui_hline(s, col, tx, col.y + row,
                              col.x + col.w - tx, ' ',
                              TUI_ATTR(TUI_BLACK, TUI_CYAN));
                draw_track_row(s, col, tx, col.y + row, m, pat, line, c,
                               cur);
            }
        }

        /* Bottom row: VU bar + current sample name. */
        int by = col.y + col.h - 1;
        int avail = col.x + col.w - tx;
        int bar_w = avail / 3;
        if (bar_w > 8) bar_w = 8;
        if (bar_w >= 2) {
            int fill = vis->vu[c] * bar_w / MP_ENERGY_MAX;
            if (fill > bar_w) fill = bar_w;
            for (int i = 0; i < bar_w; i++) {
                bool on = i < fill;
                int fg = i >= bar_w - 2 ? TUI_RED : TUI_GREEN;
                tui_put_char(s, col, tx + i, by, on ? '=' : '.',
                             on ? TUI_ATTR(fg | TUI_BRIGHT, TUI_BLACK)
                                : dim);
            }
        }
        int smp = snap ? snap->sample[c] : 0;
        if (smp >= 1 && smp <= m->num_samples &&
            m->sample_name[smp - 1][0]) {
            tui_put_str(s, col, tx + bar_w + 1, by,
                        m->sample_name[smp - 1],
                        TUI_ATTR(TUI_YELLOW, TUI_BLACK));
        }
    }
}

/* --- whole screen ----------------------------------------------------- */

void mp_view_draw(tui_surface *s, tui_rect r, const mp_mod *m,
                  const mp_view_status *st, const mp_vis *vis,
                  const mp_snap *snap)
{
    tui_rect rows[3];
    tui_constraint cons[3] = {TUI_FILL, TUI_LEN(1), TUI_LEN(MP_TRACK_ROWS)};
    tui_layout_split(r, TUI_DIR_VERT, cons, 3, rows);

    mp_view_spectrum(s, rows[0], vis);
    mp_view_title(s, rows[1], m, st, snap);
    mp_view_tracks(s, rows[2], m, vis, snap);
}
