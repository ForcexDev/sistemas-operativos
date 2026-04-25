CC      = gcc
CFLAGS  = -Wall -Wextra -std=c17
PREFIX ?= /usr/local
BINDIR  = $(PREFIX)/bin

# ────────────────────────── Colors ───────────────────────────────
GREEN  = \033[38;5;82m
CYAN   = \033[38;5;45m
DIM    = \033[38;5;245m
RESET  = \033[0m
BOLD   = \033[1m

# ────────────────────────── Main Targets ─────────────────────────

.PHONY: all tests clean install uninstall

all: toaddX toaddX-cli tests
	@echo ""
	@toilet -t -f mono12 "toaddX" -F metal 2>/dev/null || echo "  $(CYAN)═══  toaddX  ═══$(RESET)"
	@echo ""
	@echo "  $(DIM)Process Manager v1.0$(RESET)"
	@echo "  $(GREEN)Build successful$(RESET)"
	@echo ""
	@echo "  $(GREEN)✓$(RESET) toaddX        $(DIM)— Daemon$(RESET)"
	@echo "  $(GREEN)✓$(RESET) toaddX-cli    $(DIM)— CLI$(RESET)"
	@echo "  $(GREEN)✓$(RESET) test programs $(DIM)— infinite_loop, short_task, crasher$(RESET)"
	@echo ""
	@echo "  $(CYAN)Next steps:$(RESET)"
	@echo "    $(BOLD)sudo make install$(RESET)   $(DIM)Install system-wide$(RESET)"
	@echo "    $(BOLD)toaddX-cli ps$(RESET)       $(DIM)Start using$(RESET)"
	@echo ""

toaddX: toaddX.c common.h
	@echo "  $(CYAN)CC$(RESET)  toaddX.c"
	@$(CC) $(CFLAGS) -o toaddX toaddX.c

toaddX-cli: toaddX-cli.c common.h
	@echo "  $(CYAN)CC$(RESET)  toaddX-cli.c"
	@$(CC) $(CFLAGS) -o toaddX-cli toaddX-cli.c

# ────────────────────────── Test Programs ────────────────────────

tests: tests/infinite_loop tests/short_task tests/crasher
	@echo "  $(CYAN)CC$(RESET)  tests/*"

tests/infinite_loop: tests/infinite_loop.c
	@$(CC) $(CFLAGS) -o tests/infinite_loop tests/infinite_loop.c

tests/short_task: tests/short_task.c
	@$(CC) $(CFLAGS) -o tests/short_task tests/short_task.c

tests/crasher: tests/crasher.c
	@$(CC) $(CFLAGS) -o tests/crasher tests/crasher.c

# ────────────────────────── Install / Uninstall ──────────────────

install: all
	@echo "  $(CYAN)INSTALL$(RESET)  $(BINDIR)/toaddX"
	@install -d $(BINDIR)
	@install -m 755 toaddX $(BINDIR)/toaddX
	@echo "  $(CYAN)INSTALL$(RESET)  $(BINDIR)/toaddX-cli"
	@install -m 755 toaddX-cli $(BINDIR)/toaddX-cli
	@echo ""
	@echo "  $(GREEN)✓ Installed!$(RESET) You can now use $(BOLD)toaddX-cli$(RESET) from anywhere."
	@echo ""

uninstall:
	@echo "  $(CYAN)REMOVE$(RESET)  $(BINDIR)/toaddX"
	@rm -f $(BINDIR)/toaddX
	@echo "  $(CYAN)REMOVE$(RESET)  $(BINDIR)/toaddX-cli"
	@rm -f $(BINDIR)/toaddX-cli
	@echo "  $(GREEN)✓ Uninstalled.$(RESET)"

# ────────────────────────── Clean ────────────────────────────────

clean:
	@rm -f toaddX toaddX-cli
	@rm -f tests/infinite_loop tests/short_task tests/crasher
	@rm -f /tmp/toaddx_req /tmp/toaddx_res /tmp/toaddx.log
	@echo "  $(GREEN)✓ Clean.$(RESET)"
