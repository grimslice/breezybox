#pragma once

#include "esp_err.h"
#include "esp_console.h"
#include <stdint.h>
#include <stddef.h>

#ifdef ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT
/**
 * @brief Start BreezyBox with USB Serial JTAG console
 * 
 * Sets up USB Serial JTAG as stdin/stdout and starts the REPL.
 * Use this for boards with native USB (ESP32-S3, etc.)
 *
 * @param stack_size  Task stack size in bytes (8192 for simple use; 16384+
 *                    when CONFIG_BREEZYBOX_SHELL_SCRIPTING is enabled --
 *                    nested command substitution/pipelines are stack-hungry)
 * @param priority    Task priority
 * @return ESP_OK on success
 */
esp_err_t breezybox_start_usb(size_t stack_size, uint32_t priority);

#endif // ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT

/**
 * @brief Start BreezyBox using existing stdin/stdout
 * 
 * Uses whatever stdin/stdout is already configured (UART, custom VFS, etc.)
 * Works with default UART setup, or after you've redirected stdio to 
 * your own device (LCD + BT keyboard, etc.)
 *
 * @param stack_size  Task stack size in bytes (8192 for simple use; 16384+
 *                    when CONFIG_BREEZYBOX_SHELL_SCRIPTING is enabled --
 *                    nested command substitution/pipelines are stack-hungry)
 * @param priority    Task priority
 * @return ESP_OK on success
 */
esp_err_t breezybox_start_stdio(size_t stack_size, uint32_t priority);

/**
 * @brief Like breezybox_start_stdio(), but registers extra_cmds before the
 * init script runs, so init.sh can reference them (e.g. an optional
 * component's `sshd`). Pass NULL/0 for no extras.
 */
esp_err_t breezybox_start_stdio_ex(size_t stack_size, uint32_t priority,
                                   const esp_console_cmd_t *extra_cmds,
                                   size_t extra_count);

/**
 * @brief Just register BreezyBox commands (if you manage REPL yourself)
 */
esp_err_t breezybox_register_commands(void);

/**
 * @brief Execute one interactive command line.
 *
 * With CONFIG_BREEZYBOX_SHELL_SCRIPTING, runs through the shell interpreter
 * (globbing, quoting, variables, &&/||, pipelines) with state persisting
 * across calls; otherwise falls back to the lightweight breezybox_exec().
 * Use this from custom REPLs (SSH, etc.) instead of breezybox_exec().
 *
 * @return Command exit status
 */
int breezybox_exec_line(const char *line);

/**
 * @brief Force-export symbols for ELF runtime linking
 * 
 * Called automatically during init. Add functions to breezy_exports.c
 * when ELF binaries fail to load with "symbol not found".
 */
void breezybox_export_symbols(void);

// Backwards compatibility alias
#define breezybox_start breezybox_start_usb

/**
 * @brief Download a file from HTTP(S) URL
 *
 * Simple wrapper for ELF apps - avoids struct layout issues.
 *
 * @param url       Full URL (http:// or https://)
 * @param dest_path Destination file path
 * @return 0 on success, -1 on error, -2 if no network
 */
int breezy_http_download(const char *url, const char *dest_path);