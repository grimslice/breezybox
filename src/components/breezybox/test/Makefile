# Host-native build of the BreezyBox shell core for spec testing.
#
#   make              # build ./breezysh (fast: -O2, no sanitizers)
#   make run          # build + run the pruned OSH/dash spec subset
#   make SANITIZE=1       # build with ASan/UBSan (-O0 -g) -- slow, ~5min run
#   make run SANITIZE=1   # sanitized build + run, for the rare memory-bug hunt
#   make clean
#
# ASan/UBSan instrument every memory access; that "memory tracing" is what makes
# a full run take minutes, so it is OFF by default. Turn it on (SANITIZE=1) for
# the occasional deep run that must catch heap/UB bugs. Sanitizer-detected
# crashes surface via run_tests.py's CRASH_MARKERS; plain segfaults are still
# caught by exit status either way.
#
# SHELL_TEST points at the lifted OSH/dash suite (default: ./shell-spec).

CORE_DIR := ../shell
SRCS := \
  $(CORE_DIR)/sh_state.c \
  $(CORE_DIR)/sh_lex.c \
  $(CORE_DIR)/sh_parse.c \
  $(CORE_DIR)/sh_expand.c \
  $(CORE_DIR)/sh_arith.c \
  $(CORE_DIR)/sh_glob.c \
  $(CORE_DIR)/sh_builtins.c \
  $(CORE_DIR)/sh_exec.c \
  $(CORE_DIR)/sh_redir.c \
  $(CORE_DIR)/sh_port_host.c \
  test_main.c

SANITIZE ?= 0
ifeq ($(SANITIZE),0)
  CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra
else
  CFLAGS ?= -std=c11 -O0 -g -Wall -Wextra -fsanitize=address,undefined
endif
CPPFLAGS := -I$(CORE_DIR)

SHELL_TEST ?= shell-spec

BIN := breezysh

$(BIN): $(SRCS) $(CORE_DIR)/sh.h $(CORE_DIR)/sh_port.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SRCS)

.PHONY: run clean
run: $(BIN)
	@python3 run_tests.py --shell $(abspath $(BIN)) --spec $(SHELL_TEST)

clean:
	rm -f $(BIN)
	rm -rf $(BIN).dSYM
