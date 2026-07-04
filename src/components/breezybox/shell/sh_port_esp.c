// Device (ESP-IDF) implementation of the shell platform interface. External
// commands go through the existing BreezyBox execution model: the ELF loader /
// esp_console registry (breezybox_run_argv).
//
// Only compiled when CONFIG_BREEZYBOX_SHELL_SCRIPTING is enabled.
#include "sh_port.h"
#include "breezy_exec.h"
#include "breezy_vfs.h"

int sh_port_run_external(int argc, char **argv, int *found)
{
    return breezybox_run_argv(argc, argv, found);
}

void sh_port_tmpfile(int which, char *buf, int bufsz)
{
    snprintf(buf, bufsz, "%s/.sh_pipe_%d", BREEZYBOX_MOUNT_POINT, which);
}

int sh_port_chdir(const char *path)
{
    return breezybox_set_cwd(path);
}

void sh_port_getcwd(char *buf, int bufsz)
{
    breezybox_get_cwd(buf, (size_t)bufsz);
}
