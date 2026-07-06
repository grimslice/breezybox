/*
 * elf_extras.c - Project-level symbols exported to loadable ELF apps.
 *
 * The breezybox component already force-exports the libc/zlib/vterm surface
 * (breezy_exports.c). This file anchors the S3-demo-specific symbols that the
 * reference apps need but the firmware wouldn't otherwise link: nothing in
 * the firmware calls the snd_core mixer since cmd_soundkeys was removed, so
 * without these references --gc-sections drops it and the elf_loader customer
 * symbol table fails to link.
 *
 * After changing exports here, update the customer symbol table copy in
 * managed_components/espressif__elf_loader/src/esp_all_symbol.c (and its git
 * backup main/all_my_symbols.c).
 */

#include "snd_core.h"
#include "snd_port.h"

/* Volatile sink: volatile stores can't be optimized away, so the address-of
 * references below are emitted and the symbols are retained under -O2. */
static volatile const void *s_export_sink;

/*
 * Force the linker to keep the symbols above so they land in the firmware
 * ELF and, in turn, the customer symbol table. Call once at boot. Does
 * nothing useful at runtime.
 */
void breezy_s3_export_symbols(void)
{
    /* Anchor this project's snd_port so the linker pulls it from libmain
     * before resolving the breezy_sound component against it. */
    s_export_sink = (const void *)snd_port_init;
    /* snd_core mixer API (soundkeys and future sound apps). */
    s_export_sink = (const void *)snd_init;
    s_export_sink = (const void *)snd_note_on;
    s_export_sink = (const void *)snd_note_off;
    s_export_sink = (const void *)snd_all_off;
    s_export_sink = (const void *)snd_set_volume;
    s_export_sink = (const void *)snd_get_volume;
    /* PCM stream API (moddy and future streaming apps). */
    s_export_sink = (const void *)snd_stream_open;
    s_export_sink = (const void *)snd_stream_space;
    s_export_sink = (const void *)snd_stream_write;
    s_export_sink = (const void *)snd_stream_close;
}
