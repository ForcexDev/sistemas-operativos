/*
 * toaddX — Process Manager Daemon
 * Tarea 1 — Sistemas Operativos
 *
 * Daemon that manages child processes via named pipes (FIFOs).
 * Supports: start, stop, ps, status, kill, zombie commands.
 * Bonus: automatic restart on unexpected death, FAILED state.
 *
 * Compile: gcc -Wall -Wextra -std=c17 -o toaddX toaddX.c
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#include <time.h>

#include "common.h"

/* ───────────────────────── Globals ───────────────────────────── */

static struct proc_entry proc_table[MAX_PROCS];
static int proc_count = 0;
static int next_iid   = 2;  /* IID 1 reserved for toaddX itself */
static int max_restarts = DEFAULT_MAX_RESTARTS;

static volatile sig_atomic_t got_sigchld = 0;

/* ───────────────────────── Signal Handler ────────────────────── */

static void sigchld_handler(int sig) {
    (void)sig;
    got_sigchld = 1;
}

/* ───────────────────────── Logging ───────────────────────────── */

static FILE *logfp = NULL;

static void log_msg(const char *fmt, ...) {
    if (!logfp) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(logfp, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(logfp, fmt, ap);
    va_end(ap);
    fprintf(logfp, "\n");
    fflush(logfp);
}

/* ───────────────────────── Process Table Helpers ─────────────── */

static struct proc_entry *find_by_iid(int iid) {
    for (int i = 0; i < proc_count; i++) {
        if (proc_table[i].iid == iid)
            return &proc_table[i];
    }
    return NULL;
}

static struct proc_entry *find_by_pid(pid_t pid) {
    for (int i = 0; i < proc_count; i++) {
        if (proc_table[i].pid == pid)
            return &proc_table[i];
    }
    return NULL;
}

/* ───────────────────────── Format Uptime ─────────────────────── */

static void format_uptime(time_t start, char *buf, size_t len) {
    time_t now = time(NULL);
    long diff = (long)(now - start);
    if (diff < 0) diff = 0;
    int h = (int)(diff / 3600);
    int m = (int)((diff % 3600) / 60);
    int s = (int)(diff % 60);
    snprintf(buf, len, "%02d:%02d:%02d", h, m, s);
}

/* ───────────────────────── Launch a Process ──────────────────── */

static pid_t launch_process(const char *bin_path) {
    pid_t pid = fork();
    if (pid < 0) {
        log_msg("ERROR: fork failed for %s: %s", bin_path, strerror(errno));
        return -1;
    }
    if (pid == 0) {
        /* Child process */
        setpgid(0, 0);  /* New process group (for kill descendants) */

        /* Reset signal handlers */
        signal(SIGCHLD, SIG_DFL);

        /* Close log file descriptor */
        if (logfp) fclose(logfp);

        /* Execute the binary */
        execl(bin_path, bin_path, (char *)NULL);

        /* If execl returns, it failed */
        _exit(127);
    }
    /* Parent: also set pgid to avoid race condition */
    setpgid(pid, pid);
    return pid;
}

/* ───────────────────────── Command Handlers ──────────────────── */

static void handle_start(const struct request *req, struct response *res) {
    if (proc_count >= MAX_PROCS) {
        res->success = 0;
        snprintf(res->message, MAX_MSG_LEN, "ERROR: Maximum process limit (%d) reached.", MAX_PROCS);
        return;
    }

    /* Check if binary exists and is executable */
    if (access(req->bin_path, X_OK) != 0) {
        res->success = 0;
        snprintf(res->message, MAX_MSG_LEN, "ERROR: '%s' not found or not executable.", req->bin_path);
        return;
    }

    pid_t pid = launch_process(req->bin_path);
    if (pid < 0) {
        res->success = 0;
        snprintf(res->message, MAX_MSG_LEN, "ERROR: Failed to start process.");
        return;
    }

    /* Add to table */
    struct proc_entry *p = &proc_table[proc_count++];
    p->iid              = next_iid++;
    p->pid              = pid;
    strncpy(p->bin_path, req->bin_path, MAX_PATH_LEN - 1);
    p->bin_path[MAX_PATH_LEN - 1] = '\0';
    p->state            = STATE_RUNNING;
    p->start_time       = time(NULL);
    p->restart_count    = 0;
    p->was_explicit_stop = 0;

    log_msg("START iid=%d pid=%d bin=%s", p->iid, p->pid, p->bin_path);

    res->success = 1;
    snprintf(res->message, MAX_MSG_LEN, "IID: %d", p->iid);
}

static void handle_stop(const struct request *req, struct response *res) {
    struct proc_entry *p = find_by_iid(req->iid);
    if (!p) {
        res->success = 0;
        snprintf(res->message, MAX_MSG_LEN, "ERROR: IID %d not found.", req->iid);
        return;
    }
    if (p->state != STATE_RUNNING) {
        res->success = 0;
        snprintf(res->message, MAX_MSG_LEN, "ERROR: IID %d is not running (state: %s).", req->iid, state_str(p->state));
        return;
    }

    p->was_explicit_stop = 1;
    if (kill(p->pid, SIGTERM) != 0) {
        res->success = 0;
        snprintf(res->message, MAX_MSG_LEN, "ERROR: Failed to send SIGTERM to PID %d: %s", p->pid, strerror(errno));
        return;
    }

    log_msg("STOP iid=%d pid=%d", p->iid, p->pid);

    res->success = 1;
    snprintf(res->message, MAX_MSG_LEN, "Process IID %d (PID %d) sent SIGTERM.", p->iid, p->pid);
}

static void handle_ps(struct response *res) {
    char uptime_buf[16];
    int offset = 0;

    offset += snprintf(res->message + offset, MAX_MSG_LEN - offset,
                       "\033[38;5;82m%-6s %-8s %-10s %-10s %s\033[0m\n",
                       "IID", "PID", "STATE", "UPTIME", "BINARY");

    int found = 0;
    for (int i = 0; i < proc_count && offset < MAX_MSG_LEN - 100; i++) {
        struct proc_entry *p = &proc_table[i];
        format_uptime(p->start_time, uptime_buf, sizeof(uptime_buf));

        const char *color;
        switch (p->state) {
            case STATE_RUNNING: color = "\033[38;5;46m";  break; /* green  */
            case STATE_STOPPED: color = "\033[38;5;214m"; break; /* orange */
            case STATE_ZOMBIE:  color = "\033[38;5;196m"; break; /* red    */
            case STATE_FAILED:  color = "\033[38;5;160m"; break; /* dark red */
            default:            color = "\033[0m";        break;
        }

        offset += snprintf(res->message + offset, MAX_MSG_LEN - offset,
                           "%-6d %-8d %s%-10s\033[0m %-10s %s\n",
                           p->iid, p->pid, color, state_str(p->state),
                           uptime_buf, p->bin_path);
        found++;
    }

    if (found == 0) {
        offset += snprintf(res->message + offset, MAX_MSG_LEN - offset,
                           "\033[38;5;245mNo processes managed.\033[0m\n");
    }

    res->success = 1;
}

static void handle_status(const struct request *req, struct response *res) {
    struct proc_entry *p = find_by_iid(req->iid);
    if (!p) {
        res->success = 0;
        snprintf(res->message, MAX_MSG_LEN, "ERROR: IID %d not found.", req->iid);
        return;
    }

    char uptime_buf[16];
    format_uptime(p->start_time, uptime_buf, sizeof(uptime_buf));

    const char *color;
    switch (p->state) {
        case STATE_RUNNING: color = "\033[38;5;46m";  break;
        case STATE_STOPPED: color = "\033[38;5;214m"; break;
        case STATE_ZOMBIE:  color = "\033[38;5;196m"; break;
        case STATE_FAILED:  color = "\033[38;5;160m"; break;
        default:            color = "\033[0m";        break;
    }

    snprintf(res->message, MAX_MSG_LEN,
             "\033[38;5;82mIID:\033[0m      %d\n"
             "\033[38;5;82mPID:\033[0m      %d\n"
             "\033[38;5;82mBINARY:\033[0m   %s\n"
             "\033[38;5;82mSTATE:\033[0m    %s%s\033[0m\n"
             "\033[38;5;82mUPTIME:\033[0m   %s\n"
             "\033[38;5;82mRESTARTS:\033[0m %d",
             p->iid, p->pid, p->bin_path,
             color, state_str(p->state),
             uptime_buf, p->restart_count);

    res->success = 1;
}

static void handle_kill(const struct request *req, struct response *res) {
    struct proc_entry *p = find_by_iid(req->iid);
    if (!p) {
        res->success = 0;
        snprintf(res->message, MAX_MSG_LEN, "ERROR: IID %d not found.", req->iid);
        return;
    }
    if (p->state != STATE_RUNNING) {
        res->success = 0;
        snprintf(res->message, MAX_MSG_LEN, "ERROR: IID %d is not running (state: %s).", req->iid, state_str(p->state));
        return;
    }

    p->was_explicit_stop = 1;

    /* Kill entire process group to eliminate descendants */
    pid_t pgid = getpgid(p->pid);
    if (pgid > 0) {
        kill(-pgid, SIGKILL);
    } else {
        kill(p->pid, SIGKILL);
    }

    log_msg("KILL iid=%d pid=%d pgid=%d", p->iid, p->pid, pgid);

    res->success = 1;
    snprintf(res->message, MAX_MSG_LEN, "Process IID %d (PID %d) and descendants killed.", p->iid, p->pid);
}

static void handle_zombie(struct response *res) {
    char uptime_buf[16];
    int offset = 0;

    offset += snprintf(res->message + offset, MAX_MSG_LEN - offset,
                       "\033[38;5;196m%-6s %-8s %-10s %-10s %s\033[0m\n",
                       "IID", "PID", "STATE", "UPTIME", "BINARY");

    int found = 0;
    for (int i = 0; i < proc_count && offset < MAX_MSG_LEN - 100; i++) {
        struct proc_entry *p = &proc_table[i];
        if (p->state != STATE_ZOMBIE) continue;

        format_uptime(p->start_time, uptime_buf, sizeof(uptime_buf));
        offset += snprintf(res->message + offset, MAX_MSG_LEN - offset,
                           "%-6d %-8d \033[38;5;196m%-10s\033[0m %-10s %s\n",
                           p->iid, p->pid, state_str(p->state),
                           uptime_buf, p->bin_path);
        found++;
    }

    if (found == 0) {
        offset += snprintf(res->message + offset, MAX_MSG_LEN - offset,
                           "\033[38;5;245mNo zombie processes.\033[0m\n");
    }

    res->success = 1;
}

static void handle_command(const struct request *req, struct response *res) {
    memset(res, 0, sizeof(*res));

    switch (req->type) {
        case CMD_START:  handle_start(req, res);  break;
        case CMD_STOP:   handle_stop(req, res);   break;
        case CMD_PS:     handle_ps(res);          break;
        case CMD_STATUS: handle_status(req, res); break;
        case CMD_KILL:   handle_kill(req, res);   break;
        case CMD_ZOMBIE: handle_zombie(res);      break;
        default:
            res->success = 0;
            snprintf(res->message, MAX_MSG_LEN, "ERROR: Unknown command.");
            break;
    }
}

/* ───────────────────────── Reap Children ─────────────────────── */

static void reap_children(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        struct proc_entry *p = find_by_pid(pid);
        if (!p) continue;

        log_msg("REAP pid=%d iid=%d explicit=%d exit_status=%d",
                pid, p->iid, p->was_explicit_stop,
                WIFEXITED(status) ? WEXITSTATUS(status) : -1);

        if (p->was_explicit_stop) {
            /* Stopped via stop or kill command */
            p->state = STATE_STOPPED;
        } else {
            /* Unexpected death — try restart (bonus) */
            p->restart_count++;

            if (p->restart_count <= max_restarts) {
                log_msg("RESTART iid=%d attempt=%d/%d bin=%s",
                        p->iid, p->restart_count, max_restarts, p->bin_path);

                pid_t new_pid = launch_process(p->bin_path);
                if (new_pid > 0) {
                    p->pid = new_pid;
                    p->state = STATE_RUNNING;
                    p->start_time = time(NULL);
                    /* Keep same IID and restart_count */
                    continue;
                }
                /* If fork failed, fall through to FAILED */
                log_msg("RESTART FAILED iid=%d: fork error", p->iid);
            }

            if (p->restart_count > max_restarts) {
                p->state = STATE_FAILED;
                log_msg("FAILED iid=%d after %d restarts", p->iid, p->restart_count);
            } else {
                p->state = STATE_ZOMBIE;
                log_msg("ZOMBIE iid=%d pid=%d", p->iid, p->pid);
            }
        }
    }

    got_sigchld = 0;
}

/* ───────────────────────── Daemonize ─────────────────────────── */

static void daemonize(void) {
    pid_t pid;

    /* First fork */
    pid = fork();
    if (pid < 0) { perror("toaddX: fork"); exit(EXIT_FAILURE); }
    if (pid > 0) exit(EXIT_SUCCESS);  /* Parent exits */

    /* Create new session */
    if (setsid() < 0) { perror("toaddX: setsid"); exit(EXIT_FAILURE); }

    /* Ignore SIGHUP */
    signal(SIGHUP, SIG_IGN);

    /* Second fork (prevent re-acquiring terminal) */
    pid = fork();
    if (pid < 0) { perror("toaddX: fork2"); exit(EXIT_FAILURE); }
    if (pid > 0) exit(EXIT_SUCCESS);  /* First child exits */

    /* Change working directory */
    if (chdir("/") != 0) { perror("toaddX: chdir"); }

    /* Set file permissions mask */
    umask(0);

    /* Close standard file descriptors and redirect to /dev/null */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);  /* stdin  = fd 0 */
    open("/dev/null", O_WRONLY);  /* stdout = fd 1 */
    open("/dev/null", O_WRONLY);  /* stderr = fd 2 */
}

/* ───────────────────────── Create FIFOs ──────────────────────── */

static void create_fifos(void) {
    unlink(FIFO_REQ);
    unlink(FIFO_RES);

    if (mkfifo(FIFO_REQ, 0666) != 0 && errno != EEXIST) {
        log_msg("ERROR: mkfifo(%s): %s", FIFO_REQ, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (mkfifo(FIFO_RES, 0666) != 0 && errno != EEXIST) {
        log_msg("ERROR: mkfifo(%s): %s", FIFO_RES, strerror(errno));
        exit(EXIT_FAILURE);
    }

    log_msg("FIFOs created: %s, %s", FIFO_REQ, FIFO_RES);
}

/* ───────────────────────── Setup Signals ─────────────────────── */

static void setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    /* Ignore SIGPIPE (broken pipe when CLI disconnects) */
    signal(SIGPIPE, SIG_IGN);
}

/* ───────────────────────── Main ──────────────────────────────── */

int main(void) {
    /* Print banner before daemonizing */
    printf("%s", TOADDX_ASCII);
    printf("  \033[38;5;245mStarting daemon...\033[0m\n\n");
    fflush(stdout);

    /* Read max restarts from environment */
    const char *env_restarts = getenv("TOADDX_MAX_RESTARTS");
    if (env_restarts) {
        int val = atoi(env_restarts);
        if (val > 0) max_restarts = val;
    }

    /* Daemonize */
    daemonize();

    /* Open log file */
    logfp = fopen("/tmp/toaddx.log", "a");
    log_msg("═══════════════════════════════════════════");
    log_msg("toaddX daemon started (PID %d, max_restarts=%d)", getpid(), max_restarts);

    /* Setup */
    setup_signals();
    create_fifos();

    /* ─── Main Loop ─── */
    int req_fd = -1;

    while (1) {
        /* Open request FIFO if needed */
        if (req_fd < 0) {
            req_fd = open(FIFO_REQ, O_RDONLY | O_NONBLOCK);
            if (req_fd < 0) {
                log_msg("ERROR: open(%s): %s", FIFO_REQ, strerror(errno));
                sleep(1);
                continue;
            }
        }

        /* Poll for incoming requests */
        struct pollfd pfd = { .fd = req_fd, .events = POLLIN };
        int ret = poll(&pfd, 1, 1000);  /* 1 second timeout */

        /* Check for dead children */
        if (got_sigchld) {
            reap_children();
        }

        if (ret < 0) {
            if (errno == EINTR) continue;  /* Interrupted by signal */
            log_msg("ERROR: poll: %s", strerror(errno));
            continue;
        }

        if (ret > 0 && (pfd.revents & POLLIN)) {
            struct request req;
            ssize_t n = read(req_fd, &req, sizeof(req));

            if (n == (ssize_t)sizeof(req)) {
                log_msg("CMD received: type=%d iid=%d path=%s",
                        req.type, req.iid, req.bin_path);

                struct response res;
                handle_command(&req, &res);

                /* Send response */
                int res_fd = open(FIFO_RES, O_WRONLY);
                if (res_fd >= 0) {
                    write(res_fd, &res, sizeof(res));
                    close(res_fd);
                } else {
                    log_msg("ERROR: open(%s) for response: %s", FIFO_RES, strerror(errno));
                }
            } else if (n == 0) {
                /* All writers closed — reopen FIFO */
                close(req_fd);
                req_fd = -1;
            } else if (n > 0) {
                log_msg("WARNING: partial read (%zd bytes, expected %zu)", n, sizeof(req));
            }
        }

        /* Handle POLLHUP: all writers disconnected */
        if (ret > 0 && (pfd.revents & POLLHUP) && !(pfd.revents & POLLIN)) {
            close(req_fd);
            req_fd = -1;
        }
    }

    /* Cleanup (unreachable, but good practice) */
    if (logfp) fclose(logfp);
    unlink(FIFO_REQ);
    unlink(FIFO_RES);
    return 0;
}
