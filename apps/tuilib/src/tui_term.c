#include "tui_term.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* The BreezyBox elf_loader firmware exports a fixed libc-ish symbol table.
 * poll(), ioctl() and signal()/raise() are NOT in it, so on the ESP32 targets
 * we avoid them: timed input uses non-blocking read()+usleep(), the screen
 * size comes from the ANSI ESC[18t query, and terminal restore relies on
 * tui_shutdown()/atexit() instead of a SIGINT/SIGTERM handler. */
#if defined(__XTENSA__) || defined(__riscv)
#define TUI_ESP 1
#include <fcntl.h>
#else
#define TUI_ESP 0
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#endif

static struct termios g_saved_termios;
static int g_raw = 0;
static tui_surface g_screen;
static tui_cell *g_cells = NULL; /* back + front, one malloc in tui_init */

/* Output buffer: small on purpose (static BSS ships in every app). Normal
 * frames fit; a full redraw just triggers a few transparent flushes in
 * out_buffered. */
#define OUTBUF_SIZE (8 * 1024)
static char g_outbuf[OUTBUF_SIZE];
static size_t g_outlen = 0;

static void out_buffered(void *ctx, const char *bytes, size_t n) {
    (void)ctx;
    if (g_outlen + n > OUTBUF_SIZE) {
        write(STDOUT_FILENO, g_outbuf, g_outlen);
        g_outlen = 0;
        if (n > OUTBUF_SIZE) {
            write(STDOUT_FILENO, bytes, n);
            return;
        }
    }
    memcpy(g_outbuf + g_outlen, bytes, n);
    g_outlen += n;
}

static void out_flush(void) {
    if (g_outlen) {
        write(STDOUT_FILENO, g_outbuf, g_outlen);
        g_outlen = 0;
    }
}

static void restore_terminal(void) {
    if (!g_raw)
        return;
    g_raw = 0;
    /* Reset colors, clear, home, show cursor, leave alt screen. The explicit
     * 2J+H clear is what actually wipes the screen on the ESP32 breezy_term
     * vterm, which ignores the ?1049 alt-screen sequences (it only handles
     * ?25h/l), so leaving alt screen there would otherwise leave our frame
     * behind. On a real xterm the ?1049l restores the prior screen and the
     * clear is harmless. */
    const char *s = "\x1b[0m\x1b[2J\x1b[H\x1b[?25h\x1b[?1049l";
    write(STDOUT_FILENO, s, strlen(s));
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved_termios);
}

#if !TUI_ESP
static void on_signal(int sig) {
    restore_terminal();
    signal(sig, SIG_DFL);
    raise(sig);
}
#endif

/* Set raw mode from the saved termios; enter alt screen, clear, hide
 * cursor. Shared by tui_init() and tui_resume(). */
static int enter_raw(void) {
    struct termios raw = g_saved_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~OPOST;
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
        return -1;
#if TUI_ESP
    /* No poll() on the firmware: make stdin non-blocking so read() returns
     * immediately and tui_read_byte() can implement its own timeout. */
    int fl = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (fl != -1)
        fcntl(STDIN_FILENO, F_SETFL, fl | O_NONBLOCK);
#endif
    g_raw = 1;
    const char *s = "\x1b[?1049h\x1b[2J\x1b[?25l";
    write(STDOUT_FILENO, s, strlen(s));
    return 0;
}

#if TUI_ESP
/* No poll() in the firmware symbol table: stdin is non-blocking (see
 * enter_raw), so spin on read() with a short usleep between tries until a
 * byte arrives or the timeout elapses. timeout_ms < 0 blocks indefinitely. */
int tui_read_byte(int timeout_ms) {
    const int step_us = 2000; /* 2 ms poll interval */
    int waited_us = 0;
    for (;;) {
        unsigned char c;
        if (read(STDIN_FILENO, &c, 1) == 1)
            return c;
        if (timeout_ms == 0)
            return -1;
        usleep(step_us);
        if (timeout_ms > 0) {
            waited_us += step_us;
            if (waited_us >= timeout_ms * 1000)
                return -1;
        }
    }
}
#else
int tui_read_byte(int timeout_ms) {
    struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
    int r = poll(&pfd, 1, timeout_ms);
    if (r <= 0)
        return -1;
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1)
        return -1;
    return c;
}
#endif

/* On ESP the firmware exports vterm_get_size(); the breezy_term console has no
 * ioctl() and does not reliably answer escape-sequence size queries, so use it
 * directly (this is what app-drafts/vi does). On POSIX, use TIOCGWINSZ. */
#if TUI_ESP
void vterm_get_size(int *rows, int *cols);
#endif

static int detect_size(int *cols, int *rows) {
#if TUI_ESP
    int r = 0, c = 0;
    vterm_get_size(&r, &c);
    if (r > 0 && c > 0) {
        *cols = c;
        *rows = r;
        return 0;
    }
    return -1;
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 &&
        ws.ws_row > 0) {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
    return -1;
#endif
}

int tui_init(void) {
    if (g_raw)
        return 0;
    if (tcgetattr(STDIN_FILENO, &g_saved_termios) != 0)
        return -1;

    if (enter_raw() != 0)
        return -1;

    int cols = 0, rows = 0;
    if (detect_size(&cols, &rows) != 0) {
        cols = 80;
        rows = 24;
    }

    if (!g_cells) {
        g_cells = malloc(2u * (size_t)cols * (size_t)rows * sizeof(tui_cell));
        if (!g_cells) {
            restore_terminal();
            return -1;
        }
    }
    tui_surface_setup(&g_screen, g_cells, g_cells + cols * rows, cols, rows);

    atexit(restore_terminal);
#if !TUI_ESP
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
#endif
    return 0;
}

void tui_suspend(void) {
    out_flush();
    restore_terminal();
}

int tui_resume(void) {
    if (g_raw || !g_cells)
        return g_cells ? 0 : -1;
    if (enter_raw() != 0)
        return -1;

    /* invalidate front buffer: next present redraws every cell */
    int w = g_screen.w, h = g_screen.h;
    tui_surface_setup(&g_screen, g_cells, g_cells + w * h, w, h);
    return 0;
}

void tui_shutdown(void) {
    restore_terminal();
    free(g_cells);
    g_cells = NULL;
}

tui_surface *tui_screen(void) {
    return &g_screen;
}

void tui_present(void) {
    tui_surface_present(&g_screen, out_buffered, NULL);
    out_flush();
}
