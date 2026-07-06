/* mp_view: all screen drawing for the moddy TUI. Stateless: takes the
 * surface, a rect, the MOD metadata, and the current mp_vis / mp_snap.
 * Layout (mp_view_draw): spectrum fills the top, one separator/title line,
 * then MP_TRACK_ROWS of track panel at the bottom. Testable headlessly. */
#ifndef MP_VIEW_H
#define MP_VIEW_H

#include "mp_mod.h"
#include "mp_vis.h"
#include "tui_core.h"

#define MP_TRACK_ROWS 12  /* pattern rows + VU/sample-name row */
#define MP_TRACK_COLS 4   /* channels shown */

/* Player status shown in the title line. */
typedef struct {
    const char *filename; /* basename of the current file */
    int track, ntracks;   /* 1-based playlist position */
    int vol_pct;          /* 0..200 */
    bool paused;
} mp_view_status;

/* Whole screen. `snap` may be NULL (nothing played yet). */
void mp_view_draw(tui_surface *s, tui_rect r, const mp_mod *m,
                  const mp_view_status *st, const mp_vis *vis,
                  const mp_snap *snap);

/* Pieces (exposed for snapshot tests). */
void mp_view_spectrum(tui_surface *s, tui_rect r, const mp_vis *vis);
void mp_view_title(tui_surface *s, tui_rect r, const mp_mod *m,
                   const mp_view_status *st, const mp_snap *snap);
void mp_view_tracks(tui_surface *s, tui_rect r, const mp_mod *m,
                    const mp_vis *vis, const mp_snap *snap);

/* Bars the spectrum will use for a given width (for mp_vis_init). */
int mp_view_spectrum_bins(int width);

#endif /* MP_VIEW_H */
