#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <time.h>

/* ───────────────────────── FIFO Paths ───────────────────────── */
#define FIFO_REQ "/tmp/toaddx_req"
#define FIFO_RES "/tmp/toaddx_res"

/* ───────────────────────── Limits ────────────────────────────── */
#define MAX_PROCS        256
#define MAX_PATH_LEN     512
#define MAX_MSG_LEN      4096
#define DEFAULT_MAX_RESTARTS 5

/* ───────────────────────── Command Types ─────────────────────── */
enum cmd_type {
    CMD_START,
    CMD_STOP,
    CMD_PS,
    CMD_STATUS,
    CMD_KILL,
    CMD_ZOMBIE
};

/* ───────────────────────── Process States ────────────────────── */
enum proc_state {
    STATE_RUNNING,
    STATE_STOPPED,
    STATE_ZOMBIE,
    STATE_FAILED
};

/* ───────────────────────── IPC Messages ──────────────────────── */

/* CLI → Daemon */
struct request {
    enum cmd_type type;
    int iid;                        /* For stop/status/kill    */
    char bin_path[MAX_PATH_LEN];    /* For start               */
};

/* Daemon → CLI */
struct response {
    int success;
    char message[MAX_MSG_LEN];
};

/* ───────────────────────── Process Table Entry ───────────────── */
struct proc_entry {
    int    iid;
    pid_t  pid;
    char   bin_path[MAX_PATH_LEN];
    enum proc_state state;
    time_t start_time;
    int    restart_count;
    int    was_explicit_stop;       /* 1 if stopped via stop/kill */
};

/* ───────────────────────── State Helpers ─────────────────────── */
static inline const char *state_str(enum proc_state s) {
    switch (s) {
        case STATE_RUNNING: return "RUNNING";
        case STATE_STOPPED: return "STOPPED";
        case STATE_ZOMBIE:  return "ZOMBIE";
        case STATE_FAILED:  return "FAILED";
        default:            return "UNKNOWN";
    }
}

/* ───────────────────────── ASCII Art ─────────────────────────── */
#define TOADDX_ASCII \
    "\033[38;5;34m" \
    "  ╔══════════════════════════════════════════╗\n" \
    "  ║                                          ║\n" \
    "  ║       ┌─────────┐                        ║\n" \
    "  ║       │  ◉   ◉  │   \033[38;5;82mtoaddX\033[38;5;34m               ║\n" \
    "  ║       │    ▼    │   \033[38;5;245mProcess Manager\033[38;5;34m      ║\n" \
    "  ║       │  ╰───╯  │   \033[38;5;245mv1.0\033[38;5;34m               ║\n" \
    "  ║       └─┬─────┬─┘                        ║\n" \
    "  ║        ╱│     │╲                          ║\n" \
    "  ║       ╱ │     │ ╲                         ║\n" \
    "  ║      ╱  └──┬──┘  ╲                        ║\n" \
    "  ║     🌿    🌿    🌿                       ║\n" \
    "  ║                                          ║\n" \
    "  ╚══════════════════════════════════════════╝\n" \
    "\033[0m"

#define TOADDX_CLI_HEADER \
    "\033[38;5;34m" \
    "  ┌─ \033[38;5;82mtoaddX-cli\033[38;5;34m ─────────────────────────┐\n" \
    "  │  \033[38;5;245mProcess Manager CLI v1.0\033[38;5;34m             │\n" \
    "  └──────────────────────────────────────┘\n" \
    "\033[0m"

#endif /* COMMON_H */
