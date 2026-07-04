// Host test driver: reads a whole script from stdin and runs it through the
// shell core, exactly like `dash` would for the OSH/dash spec harness. This
// lets specrun.py drive the BreezyBox shell with `--shell ./breezysh`.
#include "sh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char **environ;

// dash (like any POSIX sh) seeds its shell variables from the inherited
// environment and keeps them exported. The device build has no process
// environment, but the host test build does, and several spec cases rely on
// it (e.g. `HOSTNAME=x $SH -c 'echo $HOSTNAME'`, `UID=xx $SH -c ...`).
static void import_environ(sh_state *st)
{
    for (char **e = environ; e && *e; e++) {
        const char *eq = strchr(*e, '=');
        if (!eq || eq == *e) continue;
        size_t nlen = (size_t)(eq - *e);
        char name[128];
        if (nlen >= sizeof(name)) continue;
        memcpy(name, *e, nlen);
        name[nlen] = '\0';
        sh_set(st, name, eq + 1);
        sh_export(st, name);
    }
}

int main(int argc, char **argv)
{
    // `-c CMD [name [args...]]`: run CMD from the argument, like dash. A single
    // leading `--` after -c is tolerated (options terminator).
    int argi = 1;
    if (argi < argc && strcmp(argv[argi], "-i") == 0) argi++;  // interactive: accepted, ignored
    if (argi < argc && strcmp(argv[argi], "--") == 0 &&
        !(argi + 1 < argc && strcmp(argv[argi + 1], "-c") == 0)) {
        argi++;   // bare `--` before a script/args: skip the terminator
    }
    if (argi < argc && strcmp(argv[argi], "-c") == 0) {
        argi++;
        if (argi < argc && strcmp(argv[argi], "--") == 0) argi++;
        if (argi >= argc) { fprintf(stderr, "-c: option requires an argument\n"); return 2; }
        char *cmd = argv[argi++];
        // Remaining args: first is $0, rest are $1..$N.
        int sargc = argc - argi;
        char **sargv = (sargc > 0) ? argv + argi : NULL;
        sh_state st;
        sh_state_init(&st);
        import_environ(&st);
        st.arg0 = strdup(argv[0]);   // default $0; -c name args override below
        int status = sh_run_string_args(&st, cmd, sargc, sargv);
        sh_state_free(&st);
        fflush(stdout);
        return status & 0xff;
    }

    // Read the script: from a file argument, or stdin.
    char *src = NULL;
    size_t len = 0, cap = 0;

    FILE *in = stdin;
    if (argi < argc) {
        in = fopen(argv[argi], "r");
        if (!in) { perror(argv[argi]); return 1; }
    }

    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), in)) > 0) {
        if (len + n + 1 > cap) { cap = (len + n + 1) * 2; src = realloc(src, cap); }
        memcpy(src + len, chunk, n);
        len += n;
    }
    if (in != stdin) fclose(in);
    if (!src) { src = malloc(1); len = 0; }
    src[len] = '\0';

    sh_state st;
    sh_state_init(&st);
    import_environ(&st);
    st.arg0 = strdup(argv[0]);   // stdin script: $0 is the shell itself (dash-style)
    // Forward args: argv[argi] (script path) becomes $0, the rest become $1..$N.
    int sargc = argc - argi;
    char **sargv = (sargc > 0) ? argv + argi : NULL;
    int status = sh_run_string_args(&st, src, sargc, sargv);
    sh_state_free(&st);

    free(src);
    fflush(stdout);
    return status & 0xff;
}
