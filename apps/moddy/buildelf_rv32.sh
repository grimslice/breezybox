#!/bin/sh
exec ../buildelf_rv32.sh src/moddy.c \
  src/mp_mod.c src/mp_view.c src/mp_vis.c \
  ../tuilib/src/tui_core.c ../tuilib/src/tui_input.c \
  ../tuilib/src/tui_layout.c ../tuilib/src/tui_term.c \
  -Isrc -I../tuilib/src -I../tuilib/local_include
