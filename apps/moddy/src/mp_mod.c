#include "mp_mod.h"

#include <string.h>

/* Protracker period table, finetune 0, octaves 1..3. */
static const uint16_t k_periods[MP_NOTE_COUNT] = {
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
    428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
    214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
};

static const char *k_note_names = "C-C#D-D#E-F-F#G-G#A-A#B-";

static void sanitize(char *dst, const uint8_t *src, int n)
{
    for (int i = 0; i < n; i++) {
        char c = (char)src[i];
        dst[i] = (c != 0 && (c < 32 || c > 126)) ? '?' : c;
    }
    dst[n] = '\0';
}

/* Channel count from the magic at offset 1080, mirroring pocketmod. */
static int channels_from_magic(const uint8_t *id)
{
    if (!memcmp(id, "M.K.", 4) || !memcmp(id, "M!K!", 4) ||
        !memcmp(id, "FLT4", 4))
        return 4;
    if (id[0] >= '1' && id[0] <= '9' && !memcmp(id + 1, "CHN", 3))
        return id[0] - '0';
    if (id[0] >= '1' && id[0] <= '9' && id[1] >= '0' && id[1] <= '9' &&
        !memcmp(id + 2, "CH", 2))
        return (id[0] - '0') * 10 + (id[1] - '0');
    return 0;
}

bool mp_mod_parse(mp_mod *m, const void *data, int size)
{
    const uint8_t *p = (const uint8_t *)data;
    memset(m, 0, sizeof(*m));
    if (!p || size < 1084) return false;

    m->data = p;
    m->size = size;
    m->num_channels = channels_from_magic(p + 1080);
    /* 15-sample MODs have no magic; pocketmod supports them, but they are
     * rare -- treat as 4ch with the short header. */
    int header_samples = m->num_channels ? 31 : 15;
    if (!m->num_channels) m->num_channels = 4;
    m->num_samples = header_samples;

    sanitize(m->title, p, 20);
    for (int i = 0; i < header_samples; i++)
        sanitize(m->sample_name[i], p + 20 + i * 30, 22);

    int tab = 20 + header_samples * 30; /* length byte, reset byte, order */
    m->order_len = p[tab];
    if (m->order_len < 1 || m->order_len > 128) m->order_len = 1;
    m->order = p + tab + 2;
    m->patterns = m->order + 128 + (header_samples == 31 ? 4 : 0);

    int maxpat = 0;
    for (int i = 0; i < 128; i++)
        if (m->order[i] > maxpat) maxpat = m->order[i];
    m->num_patterns = maxpat + 1;

    /* Cheap sanity: at least one full pattern must fit. */
    int pat_bytes = 64 * m->num_channels * 4;
    if ((m->patterns - p) + pat_bytes > size) return false;
    return true;
}

mp_cell mp_mod_cell(const mp_mod *m, int order_pos, int line, int ch)
{
    mp_cell c = {0, 0, 0, 0};
    if (!m->data || order_pos < 0 || order_pos >= m->order_len ||
        line < 0 || line > 63 || ch < 0 || ch >= m->num_channels)
        return c;
    int pat = m->order[order_pos];
    long off = ((long)pat * 64 + line) * m->num_channels * 4 + ch * 4;
    const uint8_t *b = m->patterns + off;
    if (b + 4 > m->data + m->size) return c;
    c.period = (uint16_t)(((b[0] & 0x0F) << 8) | b[1]);
    c.sample = (uint8_t)((b[0] & 0xF0) | (b[2] >> 4));
    c.effect = (uint8_t)(b[2] & 0x0F);
    c.param = b[3];
    return c;
}

int mp_note_index(int period)
{
    if (period <= 0) return -1;
    int best = 0, bestd = 0x7FFFFFFF;
    for (int i = 0; i < MP_NOTE_COUNT; i++) {
        int d = period - k_periods[i];
        if (d < 0) d = -d;
        if (d < bestd) { bestd = d; best = i; }
    }
    return best;
}

void mp_note_name(int note, char out[4])
{
    if (note < 0 || note >= MP_NOTE_COUNT) {
        memcpy(out, "---", 4);
        return;
    }
    out[0] = k_note_names[(note % 12) * 2];
    out[1] = k_note_names[(note % 12) * 2 + 1];
    out[2] = (char)('1' + note / 12);
    out[3] = '\0';
}
