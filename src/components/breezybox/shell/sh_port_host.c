// Host (Mac/Linux) implementation of the shell platform interface. Externals
// are run via fork/execvp against the real host so the OSH/dash spec subset
// has real coreutils to call. Test build only.
#include "sh_port.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int sh_port_run_external(int argc, char **argv, int *found)
{
    (void)argc;
    // Flush any buffered builtin output before handing the fd to the child.
    fflush(stdout);
    fflush(stderr);

    pid_t pid = fork();
    if (pid < 0) { *found = 1; return 1; }
    if (pid == 0) {
        // Child inherits the (possibly redirected) stdout/stdin fds. Our FILE*
        // swap changes the C stream, so make fd 0/1/2 follow it.
        int out_fd = fileno(stdout);
        int in_fd = fileno(stdin);
        int err_fd = fileno(stderr);
        if (out_fd != 1) dup2(out_fd, 1);
        if (in_fd != 0) dup2(in_fd, 0);
        if (err_fd != 2) dup2(err_fd, 2);
        execvp(argv[0], argv);
        _exit(127);
    }
    int wstatus = 0;
    waitpid(pid, &wstatus, 0);
    *found = 1;   // 127 from the child covers "not found"
    if (WIFEXITED(wstatus)) return WEXITSTATUS(wstatus);
    if (WIFSIGNALED(wstatus)) return 128 + WTERMSIG(wstatus);
    return 1;
}

void sh_port_tmpfile(int which, char *buf, int bufsz)
{
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";
    snprintf(buf, bufsz, "%s/.breezysh_pipe_%d_%ld", tmp, which, (long)getpid());
}

int sh_port_chdir(const char *path)
{
    return chdir(path);
}

void sh_port_getcwd(char *buf, int bufsz)
{
    if (!getcwd(buf, bufsz)) snprintf(buf, bufsz, "?");
}
