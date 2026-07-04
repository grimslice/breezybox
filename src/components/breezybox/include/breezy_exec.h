#pragma once

/**
 * @brief Initialize the exec subsystem (call once at startup)
 */
void breezybox_exec_init(void);

/**
 * @brief Execute a command with redirect support
 * 
 * Supports:
 *   cmd > file      Output redirect (overwrite)
 *   cmd >> file     Output redirect (append)
 *   cmd < file      Input redirect
 *   cmd1 | cmd2     Pipe (serialized via temp file)
 * 
 * @param cmdline Command line to execute
 * @return Command return code, or -1 on redirect error
 */
int breezybox_exec(const char *cmdline);

/**
 * @brief Run a pre-parsed argv as an external ELF or esp_console builtin.
 *
 * Used by the shell-scripting core (sh_port_esp.c). Tries an external ELF at
 * argv[0] first, then falls back to the esp_console command registry.
 *
 * @param argc   Argument count
 * @param argv   NULL-terminated argument vector
 * @param found  Set to 1 if the command was located, 0 otherwise
 * @return Command exit status
 */
int breezybox_run_argv(int argc, char **argv, int *found);
