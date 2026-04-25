/*
 * toaddX-cli — CLI for toaddX Process Manager
 * Tarea 1 — Sistemas Operativos
 *
 * Command-line interface to communicate with the toaddX daemon via FIFOs.
 * Usage: toaddX-cli <command> [args]
 *
 * Compile: gcc -Wall -Wextra -std=c17 -o toaddX-cli toaddX-cli.c
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

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
    printf("    toaddX-cli start ./my_program\n");
    printf("    toaddX-cli ps\n");
    printf("    toaddX-cli status 2\n");
    printf("    toaddX-cli stop 2\n");
    printf("\n");
}

/* ───────────────────────── Check Daemon ──────────────────────── */

static int check_daemon_running(void) {
    /* Check if request FIFO exists */
    struct stat st;
    if (stat(FIFO_REQ, &st) != 0) {
        fprintf(stderr,
                "\033[38;5;196mERROR:\033[0m toaddX daemon is not running.\n"
                "       Start it with: ./toaddX\n");
        return 0;
    }
    return 1;
}

/* ───────────────────────── Send & Receive ────────────────────── */

static int send_request(const struct request *req, struct response *res) {
    /* Open request FIFO for writing */
    int req_fd = open(FIFO_REQ, O_WRONLY);
    if (req_fd < 0) {
        fprintf(stderr,
                "\033[38;5;196mERROR:\033[0m Cannot connect to toaddX daemon: %s\n"
                "       Is the daemon running? Start with: ./toaddX\n",
                strerror(errno));
        return -1;
    }

    /* Write request */
    ssize_t written = write(req_fd, req, sizeof(*req));
    close(req_fd);

    if (written != (ssize_t)sizeof(*req)) {
        fprintf(stderr, "\033[38;5;196mERROR:\033[0m Failed to send command to daemon.\n");
        return -1;
    }

    /* Open response FIFO for reading */
    int res_fd = open(FIFO_RES, O_RDONLY);
    if (res_fd < 0) {
        fprintf(stderr, "\033[38;5;196mERROR:\033[0m Cannot read response from daemon: %s\n",
                strerror(errno));
        return -1;
    }

    /* Read response */
    ssize_t n = read(res_fd, res, sizeof(*res));
    close(res_fd);

    if (n != (ssize_t)sizeof(*res)) {
        fprintf(stderr, "\033[38;5;196mERROR:\033[0m Incomplete response from daemon.\n");
        return -1;
    }

    return 0;
}

/* ───────────────────────── Parse & Execute ───────────────────── */

static int parse_iid(const char *str) {
    char *endptr;
    long val = strtol(str, &endptr, 10);
    if (*endptr != '\0' || val <= 0) {
        fprintf(stderr, "\033[38;5;196mERROR:\033[0m Invalid IID: '%s' (must be a positive integer)\n", str);
        return -1;
    }
    return (int)val;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    /* Check if daemon is running */
    if (!check_daemon_running()) {
        return 1;
    }

    struct request req;
    memset(&req, 0, sizeof(req));

    /* Parse command */
    if (strcmp(argv[1], "start") == 0) {
        if (argc < 3) {
            fprintf(stderr, "\033[38;5;196mUsage:\033[0m toaddX-cli start <bin_path>\n");
            return 1;
        }
        req.type = CMD_START;

        /* Resolve to absolute path if relative */
        char *resolved = realpath(argv[2], NULL);
        if (resolved) {
            strncpy(req.bin_path, resolved, MAX_PATH_LEN - 1);
            free(resolved);
        } else {
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
        fprintf(stderr, "\033[38;5;196mERROR:\033[0m Unknown command '%s'\n\n", argv[1]);
        print_usage();
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
