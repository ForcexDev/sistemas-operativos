/*
 * toaddX-cli — CLI for toaddX Process Manager
 * Tarea 1 — Sistemas Operativos
 *
 * Command-line interface to communicate with the toaddX daemon via FIFOs.
 * Auto-starts the daemon if not running.
 *
 * Usage: toaddX-cli <command> [args]
 *
 * Compile: gcc -Wall -Wextra -std=c17 -o toaddX-cli toaddX-cli.c
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#include "common.h"

/* ───────────────────────── Usage ──────────────────────────────── */

static void print_usage(void) {
    printf("%s", TOADDX_CLI_HEADER);
    printf("\n");
    printf("  \033[38;5;82mUsage:\033[0m toaddX-cli <command> [arguments]\n\n");
    printf("  \033[38;5;82mCommands:\033[0m\n");
    printf("    \033[38;5;45mstart\033[0m  <bin_path>   Start a process managed by toaddX\n");
    printf("    \033[38;5;45mstop\033[0m   <iid>        Stop a process (SIGTERM)\n");
    printf("    \033[38;5;45mps\033[0m                  List all managed processes\n");
    printf("    \033[38;5;45mstatus\033[0m <iid>        Show detailed process info\n");
    printf("    \033[38;5;45mkill\033[0m   <iid>        Kill process and all descendants\n");
    printf("    \033[38;5;45mzombie\033[0m              List zombie processes\n");
    printf("\n");
    printf("  \033[38;5;82mExamples:\033[0m\n");
    printf("    toaddX-cli start /home/user/my_program\n");
    printf("    toaddX-cli ps\n");
    printf("    toaddX-cli status 2\n");
    printf("    toaddX-cli stop 2\n");
    printf("\n");
}

/* ───────────────────────── Daemon Management ────────────────── */

/*
 * Check if the daemon is currently running by verifying the request
 * FIFO exists and can be opened for writing (non-blocking).
 */
static int is_daemon_running(void) {
    struct stat st;
    if (stat(FIFO_REQ, &st) != 0) return 0;
    if (!S_ISFIFO(st.st_mode))    return 0;

    /* Try a non-blocking open to verify daemon is actually listening */
    int fd = open(FIFO_REQ, O_WRONLY | O_NONBLOCK);
    if (fd < 0) return 0;
    close(fd);
    return 1;
}

/*
 * Attempt to start the daemon automatically.
 * Searches for the toaddX binary in PATH and common locations.
 * Returns 1 on success, 0 on failure.
 */
static int start_daemon(void) {
    fprintf(stderr,
            "\033[38;5;214m⚡\033[0m Daemon not running. Starting \033[38;5;82mtoaddX\033[0m...\n");

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "\033[38;5;196m✗\033[0m Failed to start daemon: %s\n", strerror(errno));
        return 0;
    }

    if (pid == 0) {
        /* Child: try to exec the daemon */
        /* Redirect stdout/stderr to /dev/null for the startup banner
         * (daemon will print it before daemonizing, but we don't need it here) */

        /* Try PATH first (installed via make install) */
        execlp("toaddX", "toaddX", (char *)NULL);

        /* Try current directory */
        execl("./toaddX", "./toaddX", (char *)NULL);

        /* Failed */
        _exit(127);
    }

    /* Parent: wait for the initial process to finish (it forks and exits) */
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
        fprintf(stderr,
                "\033[38;5;196m✗\033[0m Could not find toaddX binary.\n"
                "  Run \033[1msudo make install\033[0m or start manually: \033[1m./toaddX\033[0m\n");
        return 0;
    }

    /* Give the daemon a moment to create FIFOs */
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 500000000 }; /* 500ms */
    nanosleep(&ts, NULL);

    /* Verify it started */
    if (!is_daemon_running()) {
        /* Try once more with a longer wait */
        ts.tv_sec = 1;
        ts.tv_nsec = 0;
        nanosleep(&ts, NULL);

        if (!is_daemon_running()) {
            fprintf(stderr, "\033[38;5;196m✗\033[0m Daemon failed to start. Check /tmp/toaddx.log\n");
            return 0;
        }
    }

    fprintf(stderr, "\033[38;5;46m✓\033[0m Daemon started successfully.\n\n");
    return 1;
}

/*
 * Ensure the daemon is running, starting it if necessary.
 * Returns 1 if daemon is ready, 0 if it couldn't be started.
 */
static int ensure_daemon(void) {
    if (is_daemon_running()) return 1;
    return start_daemon();
}

/* ───────────────────────── Send & Receive ────────────────────── */

static int send_request(const struct request *req, struct response *res) {
    /* Open request FIFO for writing */
    int req_fd = open(FIFO_REQ, O_WRONLY);
    if (req_fd < 0) {
        fprintf(stderr, "\033[38;5;196m✗\033[0m Cannot connect to daemon: %s\n", strerror(errno));
        return -1;
    }

    /* Write request */
    ssize_t written = write(req_fd, req, sizeof(*req));
    close(req_fd);

    if (written != (ssize_t)sizeof(*req)) {
        fprintf(stderr, "\033[38;5;196m✗\033[0m Failed to send command.\n");
        return -1;
    }

    /* Open response FIFO for reading */
    int res_fd = open(FIFO_RES, O_RDONLY);
    if (res_fd < 0) {
        fprintf(stderr, "\033[38;5;196m✗\033[0m Cannot read response: %s\n", strerror(errno));
        return -1;
    }

    /* Read response */
    ssize_t n = read(res_fd, res, sizeof(*res));
    close(res_fd);

    if (n != (ssize_t)sizeof(*res)) {
        fprintf(stderr, "\033[38;5;196m✗\033[0m Incomplete response from daemon.\n");
        return -1;
    }

    return 0;
}

/* ───────────────────────── Parse & Execute ───────────────────── */

static int parse_iid(const char *str) {
    char *endptr;
    long val = strtol(str, &endptr, 10);
    if (*endptr != '\0' || val <= 0) {
        fprintf(stderr, "\033[38;5;196m✗\033[0m Invalid IID: '%s' (must be a positive integer)\n", str);
        return -1;
    }
    return (int)val;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    struct request req;
    memset(&req, 0, sizeof(req));

    /* Parse command first (before checking daemon, for --help etc.) */
    if (strcmp(argv[1], "start") == 0) {
        if (argc < 3) {
            fprintf(stderr, "\033[38;5;196mUsage:\033[0m toaddX-cli start <bin_path>\n");
            return 1;
        }
        req.type = CMD_START;

        /* Resolve to absolute path */
        char *resolved = realpath(argv[2], NULL);
        if (resolved) {
            strncpy(req.bin_path, resolved, MAX_PATH_LEN - 1);
            free(resolved);
        } else {
            /* If realpath fails, still try the path as-is */
            strncpy(req.bin_path, argv[2], MAX_PATH_LEN - 1);
        }
        req.bin_path[MAX_PATH_LEN - 1] = '\0';
    }
    else if (strcmp(argv[1], "stop") == 0) {
        if (argc < 3) {
            fprintf(stderr, "\033[38;5;196mUsage:\033[0m toaddX-cli stop <iid>\n");
            return 1;
        }
        req.type = CMD_STOP;
        req.iid = parse_iid(argv[2]);
        if (req.iid < 0) return 1;
    }
    else if (strcmp(argv[1], "ps") == 0) {
        req.type = CMD_PS;
    }
    else if (strcmp(argv[1], "status") == 0) {
        if (argc < 3) {
            fprintf(stderr, "\033[38;5;196mUsage:\033[0m toaddX-cli status <iid>\n");
            return 1;
        }
        req.type = CMD_STATUS;
        req.iid = parse_iid(argv[2]);
        if (req.iid < 0) return 1;
    }
    else if (strcmp(argv[1], "kill") == 0) {
        if (argc < 3) {
            fprintf(stderr, "\033[38;5;196mUsage:\033[0m toaddX-cli kill <iid>\n");
            return 1;
        }
        req.type = CMD_KILL;
        req.iid = parse_iid(argv[2]);
        if (req.iid < 0) return 1;
    }
    else if (strcmp(argv[1], "zombie") == 0) {
        req.type = CMD_ZOMBIE;
    }
    else {
        fprintf(stderr, "\033[38;5;196m✗\033[0m Unknown command '%s'\n\n", argv[1]);
        print_usage();
        return 1;
    }

    /* Ensure daemon is running (auto-start if needed) */
    if (!ensure_daemon()) {
        return 1;
    }

    /* Send to daemon and get response */
    struct response res;
    if (send_request(&req, &res) != 0) {
        return 1;
    }

    /* Print response */
    printf("%s\n", res.message);

    return res.success ? 0 : 1;
}
