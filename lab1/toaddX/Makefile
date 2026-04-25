CC      = gcc
CFLAGS  = -Wall -Wextra -std=c17
LDFLAGS =

# ────────────────────────── Targets ──────────────────────────────

.PHONY: all tests clean

all: toaddX toaddX-cli tests

toaddX: toaddX.c common.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o toaddX toaddX.c

toaddX-cli: toaddX-cli.c common.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o toaddX-cli toaddX-cli.c

# ────────────────────────── Test Programs ────────────────────────

tests: tests/infinite_loop tests/short_task tests/crasher

tests/infinite_loop: tests/infinite_loop.c
	$(CC) $(CFLAGS) -o tests/infinite_loop tests/infinite_loop.c

tests/short_task: tests/short_task.c
	$(CC) $(CFLAGS) -o tests/short_task tests/short_task.c

tests/crasher: tests/crasher.c
	$(CC) $(CFLAGS) -o tests/crasher tests/crasher.c

# ────────────────────────── Clean ────────────────────────────────

clean:
	rm -f toaddX toaddX-cli
	rm -f tests/infinite_loop tests/short_task tests/crasher
	rm -f /tmp/toaddx_req /tmp/toaddx_res /tmp/toaddx.log
