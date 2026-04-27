/*
 * toaddX-cli — Cliente CLI para el Gestor de Procesos toaddX
 * Tarea 1 — Sistemas Operativos
 *
 * Herramienta de línea de comandos que se comunica con el daemon toaddX
 * mediante named pipes (FIFOs). Si el daemon no está corriendo, lo
 * inicia automáticamente.
 *
 * Flujo general:
 *   1. Parsear el comando del usuario (start, stop, ps, status, kill, zombie)
 *   2. Verificar que el daemon esté vivo (auto-iniciarlo si no)
 *   3. Enviar un struct request por FIFO_REQ
 *   4. Esperar y leer un struct response por FIFO_RES
 *   5. Imprimir el resultado
 *
 * Uso: toaddX-cli <command> [args]
 * Compilar: gcc -Wall -Wextra -std=c17 -o toaddX-cli toaddX-cli.c
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

/* ================================================================ */
/*                       INTERFAZ DE AYUDA                         */
/* ================================================================ */

static void print_usage(void) {
  printf("%s", TOADDX_CLI_HEADER);
  printf("\n");
  printf("  \033[38;5;82mUsage:\033[0m toaddX-cli <command> [arguments]\n\n");
  printf("  \033[38;5;82mCommands:\033[0m\n");
  printf("    \033[38;5;45mstart\033[0m  <bin_path>   Start a process managed "
         "by toaddX\n");
  printf(
      "    \033[38;5;45mstop\033[0m   <iid>        Stop a process (SIGTERM)\n");
  printf("    \033[38;5;45mps\033[0m                  List all managed "
         "processes\n");
  printf("    \033[38;5;45mstatus\033[0m <iid>        Show detailed process "
         "info\n");
  printf("    \033[38;5;45mkill\033[0m   <iid>        Kill process and all "
         "descendants\n");
  printf("    \033[38;5;45mzombie\033[0m              List zombie processes\n");
  printf("\n");
  printf("  \033[38;5;82mExamples:\033[0m\n");
  printf("    toaddX-cli start /home/user/my_program\n");
  printf("    toaddX-cli ps\n");
  printf("    toaddX-cli status 2\n");
  printf("    toaddX-cli stop 2\n");
  printf("\n");
}

/* ================================================================ */
/*              DETECCIÓN Y AUTO-INICIO DEL DAEMON                 */
/* ================================================================ */

/*
 * Verifica si el daemon está corriendo.
 *
 * Estrategia: intentar abrir el FIFO de peticiones en modo escritura
 * con O_NONBLOCK. Si el daemon está escuchando (tiene el FIFO abierto
 * para lectura), el open() tendrá éxito. Si no hay nadie leyendo,
 * open() falla con ENXIO → el daemon no está vivo.
 */
static int is_daemon_running(void) {
  struct stat st;
  if (stat(FIFO_REQ, &st) != 0)
    return 0;
  if (!S_ISFIFO(st.st_mode))
    return 0;

  /* Try a non-blocking open to verify daemon is actually listening */
  int fd = open(FIFO_REQ, O_WRONLY | O_NONBLOCK);
  if (fd < 0)
    return 0;
  close(fd);
  return 1;
}

/*
 * Inicia el daemon automáticamente si no estaba corriendo.
 *
 * Hace fork() para crear un hijo que ejecuta el binario del daemon.
 * El daemon a su vez se daemoniza internamente (double fork + setsid),
 * por lo que el proceso hijo que creamos aquí terminará rápido.
 * Esperamos con waitpid() a que ese hijo termine, y luego verificamos
 * que el daemon haya creado los FIFOs correctamente.
 */
static int start_daemon(void) {
  fprintf(stderr, "\033[38;5;214m⚡\033[0m Daemon not running. Starting "
                  "\033[38;5;82mtoaddX\033[0m...\n");

  pid_t pid = fork();
  if (pid < 0) {
    fprintf(stderr, "\033[38;5;196m✗\033[0m Failed to start daemon: %s\n",
            strerror(errno));
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
    fprintf(stderr, "\033[38;5;196m✗\033[0m Could not find toaddX binary.\n"
                    "  Run \033[1msudo make install\033[0m or start manually: "
                    "\033[1m./toaddX\033[0m\n");
    return 0;
  }

  /* Give the daemon a moment to create FIFOs */
  struct timespec ts = {.tv_sec = 0, .tv_nsec = 500000000}; /* 500ms */
  nanosleep(&ts, NULL);

  /* Verify it started */
  if (!is_daemon_running()) {
    /* Try once more with a longer wait */
    ts.tv_sec = 1;
    ts.tv_nsec = 0;
    nanosleep(&ts, NULL);

    if (!is_daemon_running()) {
      fprintf(stderr, "\033[38;5;196m✗\033[0m Daemon failed to start. Check "
                      "/tmp/toaddx.log\n");
      return 0;
    }
  }

  fprintf(stderr, "\033[38;5;46m✓\033[0m Daemon started successfully.\n\n");
  return 1;
}

/*
 * Punto de entrada para asegurar que el daemon esté vivo.
 * Si ya está corriendo retorna 1 directo, si no intenta iniciarlo.
 */
static int ensure_daemon(void) {
  if (is_daemon_running())
    return 1;
  return start_daemon();
}

/* ================================================================ */
/*         COMUNICACIÓN IPC (envío y recepción por FIFOs)           */
/* ================================================================ */

/*
 * Envía una petición al daemon y espera su respuesta.
 *
 * Protocolo IPC (sincrónico):
 *   CLI abre FIFO_REQ → escribe struct request → cierra
 *   CLI abre FIFO_RES → lee struct response (bloqueante) → cierra
 *
 * Se usan structs de tamaño fijo para evitar problemas de parsing.
 * Las escrituras menores a PIPE_BUF (4096 bytes) son atómicas en Linux.
 */

static int send_request(const struct request *req, struct response *res) {
  /* Paso 1: Abrir el FIFO de peticiones y enviar el comando */
  int req_fd = open(FIFO_REQ, O_WRONLY);
  if (req_fd < 0) {
    fprintf(stderr, "\033[38;5;196m✗\033[0m Cannot connect to daemon: %s\n",
            strerror(errno));
    return -1;
  }

  /* Escribir la petición completa de una sola vez (escritura atómica) */
  ssize_t written = write(req_fd, req, sizeof(*req));
  close(req_fd);

  if (written != (ssize_t)sizeof(*req)) {
    fprintf(stderr, "\033[38;5;196m✗\033[0m Failed to send command.\n");
    return -1;
  }

  /* Paso 2: Abrir el FIFO de respuestas y esperar contestación */
  int res_fd = open(FIFO_RES, O_RDONLY);
  if (res_fd < 0) {
    fprintf(stderr, "\033[38;5;196m✗\033[0m Cannot read response: %s\n",
            strerror(errno));
    return -1;
  }

  /* Leer la respuesta completa del daemon */
  ssize_t n = read(res_fd, res, sizeof(*res));
  close(res_fd);

  if (n != (ssize_t)sizeof(*res)) {
    fprintf(stderr,
            "\033[38;5;196m✗\033[0m Incomplete response from daemon.\n");
    return -1;
  }

  return 0;
}

/* ================================================================ */
/*          MAIN — parseo de comandos y flujo principal             */
/* ================================================================ */

/* Convierte un string a IID (Internal ID), validando que sea positivo */

static int parse_iid(const char *str) {
  char *endptr;
  long val = strtol(str, &endptr, 10);
  if (*endptr != '\0' || val <= 0) {
    fprintf(stderr,
            "\033[38;5;196m✗\033[0m Invalid IID: '%s' (must be a positive "
            "integer)\n",
            str);
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

  /* ── Paso 1: Parsear el comando pedido por el usuario ── */
  if (strcmp(argv[1], "start") == 0) {
    if (argc < 3) {
      fprintf(stderr,
              "\033[38;5;196mUsage:\033[0m toaddX-cli start <bin_path>\n");
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
  } else if (strcmp(argv[1], "stop") == 0) {
    if (argc < 3) {
      fprintf(stderr, "\033[38;5;196mUsage:\033[0m toaddX-cli stop <iid>\n");
      return 1;
    }
    req.type = CMD_STOP;
    req.iid = parse_iid(argv[2]);
    if (req.iid < 0)
      return 1;
  } else if (strcmp(argv[1], "ps") == 0) {
    req.type = CMD_PS;
  } else if (strcmp(argv[1], "status") == 0) {
    if (argc < 3) {
      fprintf(stderr, "\033[38;5;196mUsage:\033[0m toaddX-cli status <iid>\n");
      return 1;
    }
    req.type = CMD_STATUS;
    req.iid = parse_iid(argv[2]);
    if (req.iid < 0)
      return 1;
  } else if (strcmp(argv[1], "kill") == 0) {
    if (argc < 3) {
      fprintf(stderr, "\033[38;5;196mUsage:\033[0m toaddX-cli kill <iid>\n");
      return 1;
    }
    req.type = CMD_KILL;
    req.iid = parse_iid(argv[2]);
    if (req.iid < 0)
      return 1;
  } else if (strcmp(argv[1], "zombie") == 0) {
    req.type = CMD_ZOMBIE;
  } else {
    fprintf(stderr, "\033[38;5;196m✗\033[0m Unknown command '%s'\n\n", argv[1]);
    print_usage();
    return 1;
  }

  /* ── Paso 2: Asegurar que el daemon receptor esté vivo ── */
  if (!ensure_daemon()) {
    return 1;
  }

  /* ── Paso 3: Enviar la petición y capturar la respuesta ── */
  struct response res;
  if (send_request(&req, &res) != 0) {
    return 1;
  }

  /* ── Paso 4: Mostrar el resultado al usuario ── */
  printf("%s\n", res.message);

  return res.success ? 0 : 1;
}
