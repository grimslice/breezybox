idf_component_register(
    SRCS 
        "breezybox.c"
        "breezy_vfs.c"
        "breezy_wrap.c"
        "breezy_exec.c"
        "breezy_exports.c"
        "breezy_http.c"
        "cmd/ls.c"
        "cmd/cat.c"
        "cmd/mkdir.c"
        "cmd/wifi.c"
        "cmd/httpd.c"
        "cmd/head.c"
        "cmd/tail.c"
        "cmd/more.c"
        "cmd/wc.c"
        "cmd/cksum.c"
        "cmd/printf.c"
        "cmd/cp.c"
        "cmd/mv.c"
        "cmd/rm.c"
        "cmd/df.c"
        "cmd/du.c"
        "cmd/date.c"
        "cmd/eget.c"
    INCLUDE_DIRS "include"
    REQUIRES console littlefs nvs_flash esp_wifi esp_netif esp_http_server esp_http_client json vfs mbedtls elf_loader zlib breezy_term
)

# Rich shell scripting (CONFIG_BREEZYBOX_SHELL_SCRIPTING). The core is pure C
# with no ESP-IDF includes; only sh_port_esp.c touches the platform. Guarded so
# the default build gains zero code/SRAM.
if(CONFIG_BREEZYBOX_SHELL_SCRIPTING)
    target_sources(${COMPONENT_LIB} PRIVATE
        "shell/sh_state.c"
        "shell/sh_lex.c"
        "shell/sh_parse.c"
        "shell/sh_expand.c"
        "shell/sh_arith.c"
        "shell/sh_glob.c"
        "shell/sh_builtins.c"
        "shell/sh_exec.c"
        "shell/sh_redir.c"
        "shell/sh_port_esp.c"
    )
    target_include_directories(${COMPONENT_LIB} PRIVATE "shell")
endif()

# Propagate linker wrap options to any project using this component
target_link_options(${COMPONENT_LIB} INTERFACE
    "-Wl,-wrap,fopen"
    "-Wl,-wrap,open"
    "-Wl,-wrap,mkdir"
    "-Wl,-wrap,rename"
    "-Wl,-wrap,remove"
    "-Wl,-wrap,rmdir"
    "-Wl,-wrap,chdir"
    "-Wl,-wrap,getcwd"
    "-Wl,-wrap,opendir"
    "-Wl,-wrap,readdir"
    "-Wl,-wrap,closedir"
    "-Wl,-wrap,rewinddir"
    "-Wl,-wrap,stat"
    "-Wl,-wrap,realpath"
)