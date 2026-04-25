# 🐸 toaddX — Process Manager

**toaddX** es un gestor de procesos tipo daemon para Linux. Ejecuta, monitorea y controla procesos desde la línea de comandos, con auto-restart automático ante crashes inesperados.

> **Tarea 1 — Sistemas Operativos**
> Procesos y Syscalls | C17 | Sin threads | IPC via FIFOs

---

## 📦 Instalación

```bash
# Clonar el repositorio
git clone https://github.com/ForcexDev/sistemas-operativos.git

# Entrar a la carpeta del proyecto
cd sistemas-operativos/lab1/toaddX

# Compilar
make

# Instalar en el sistema (permite usar toaddX-cli desde cualquier lugar)
sudo make install
```

> **Requisitos:** `gcc` con soporte C17, Linux.
> **Opcional:** `toilet` para el banner ASCII estilizado (`sudo apt install toilet`).

---

## 🚀 Uso Rápido

```bash
# El daemon se inicia automáticamente al ejecutar cualquier comando
toaddX-cli start /path/to/binary    # Iniciar un proceso
toaddX-cli ps                        # Listar procesos
toaddX-cli status <iid>              # Info detallada
toaddX-cli stop <iid>                # Detener (SIGTERM)
toaddX-cli kill <iid>                # Matar + descendientes (SIGKILL)
toaddX-cli zombie                    # Listar procesos zombie
```

---

## 📖 Comandos

### `toaddX-cli start <bin_path>`

Ejecuta el binario indicado y lo pone bajo administración de toaddX.
Retorna el **IID** (Internal ID) asignado al proceso.

```bash
$ toaddX-cli start /home/user/mi_programa
IID: 2
```

### `toaddX-cli stop <iid>`

Envía **SIGTERM** al proceso, permitiéndole terminar de forma limpia.
El proceso queda en estado **STOPPED**.

```bash
$ toaddX-cli stop 2
Process IID 2 (PID 14203) sent SIGTERM.
```

### `toaddX-cli ps`

Lista todos los procesos administrados con su IID, PID, estado, tiempo activo y binario.

```bash
$ toaddX-cli ps
IID    PID      STATE      UPTIME     BINARY
2      14203    RUNNING    00:02:14   /home/user/servidor
3      14891    STOPPED    00:00:43   /home/user/worker
4      15002    FAILED     00:00:12   /home/user/crasher
```

### `toaddX-cli status <iid>`

Muestra información detallada de un proceso específico.

```bash
$ toaddX-cli status 2
IID:      2
PID:      14203
BINARY:   /home/user/servidor
STATE:    RUNNING
UPTIME:   00:02:14
RESTARTS: 0
```

### `toaddX-cli kill <iid>`

Envía **SIGKILL** al proceso y a **todos sus descendientes** (hijos, nietos, etc).
No deja procesos huérfanos. El proceso queda en estado **STOPPED**.

```bash
$ toaddX-cli kill 3
Process IID 3 (PID 14891) and descendants killed.
```

### `toaddX-cli zombie`

Lista únicamente los procesos en estado **ZOMBIE** (terminaron pero su estado de salida aún no fue recogido).

```bash
$ toaddX-cli zombie
IID    PID      STATE      UPTIME     BINARY
5      16001    ZOMBIE     00:00:03   /home/user/tarea_corta
```

---

## 🔄 Estados de un Proceso

| Estado | Significado |
|--------|-------------|
| **RUNNING** | El proceso está en ejecución |
| **STOPPED** | Fue detenido explícitamente con `stop` o `kill` |
| **ZOMBIE** | Terminó por sí solo pero su estado de salida no fue recogido |
| **FAILED** | Murió inesperadamente más de N veces consecutivas (bonus) |

```
                fork+exec
               ┌──────────┐
               │          ▼
           ┌───────────────────┐
           │     RUNNING       │
           └───────────────────┘
              │          │
    stop/kill │          │ Muere solo
              ▼          ▼
      ┌──────────┐   ┌────────────────────┐
      │ STOPPED  │   │ Auto-restart       │
      └──────────┘   │ (mismo IID)        │
                     │ ¿Éxito? → RUNNING  │
                     │ ¿>N muertes? ↓     │
                     └────────┬───────────┘
                              ▼
                       ┌──────────┐
                       │  FAILED  │
                       └──────────┘
```

---

## 🧪 Ejemplo de Flujo Completo

A continuación se muestra un flujo de uso completo con los resultados esperados.

### 1. Compilar e instalar

```bash
$ cd sistemas-operativos/lab1/toaddX
$ make
  CC  toaddX.c
  CC  toaddX-cli.c
  CC  tests/*

  ████████╗ ██████╗  █████╗ ██████╗ ██████╗ ██╗  ██╗
  (banner metálico de toilet)

  Process Manager v1.0
  Build successful

  ✓ toaddX        — Daemon
  ✓ toaddX-cli    — CLI
  ✓ test programs — infinite_loop, short_task, crasher

$ sudo make install
  INSTALL  /usr/local/bin/toaddX
  INSTALL  /usr/local/bin/toaddX-cli
  ✓ Installed! You can now use toaddX-cli from anywhere.
```

### 2. Iniciar un proceso (el daemon se auto-inicia)

```bash
$ toaddX-cli start tests/infinite_loop
⚡ Daemon not running. Starting toaddX...
  (banner de toaddX)
✓ Daemon started successfully.

IID: 2
```

### 3. Verificar que está corriendo

```bash
$ toaddX-cli ps
IID    PID      STATE      UPTIME     BINARY
2      14203    RUNNING    00:00:05   /home/user/.../tests/infinite_loop
```

### 4. Ver status detallado

```bash
$ toaddX-cli status 2
IID:      2
PID:      14203
BINARY:   /home/user/.../tests/infinite_loop
STATE:    RUNNING
UPTIME:   00:00:12
RESTARTS: 0
```

### 5. Detener el proceso (SIGTERM)

```bash
$ toaddX-cli stop 2
Process IID 2 (PID 14203) sent SIGTERM.

$ toaddX-cli ps
IID    PID      STATE      UPTIME     BINARY
2      14203    STOPPED    00:00:30   /home/user/.../tests/infinite_loop
```

### 6. Iniciar un segundo proceso

```bash
$ toaddX-cli start tests/infinite_loop
IID: 3

$ toaddX-cli ps
IID    PID      STATE      UPTIME     BINARY
2      14203    STOPPED    00:01:00   /home/user/.../tests/infinite_loop
3      15002    RUNNING    00:00:03   /home/user/.../tests/infinite_loop
```

> **Nota:** Los IIDs son secuenciales y nunca se reutilizan. IID 1 está reservado para toaddX.

### 7. Matar proceso + descendientes (SIGKILL)

```bash
$ toaddX-cli kill 3
Process IID 3 (PID 15002) and descendants killed.

$ toaddX-cli ps
IID    PID      STATE      UPTIME     BINARY
2      14203    STOPPED    00:01:30   /home/user/.../tests/infinite_loop
3      15002    STOPPED    00:00:15   /home/user/.../tests/infinite_loop
```

### 8. Probar auto-restart (Bonus)

El programa `crasher` se termina inesperadamente cada 1 segundo. toaddX lo reinicia automáticamente hasta 5 veces:

```bash
$ toaddX-cli start tests/crasher
IID: 4

# Después de ~3 segundos:
$ toaddX-cli status 4
IID:      4
PID:      15500
BINARY:   /home/user/.../tests/crasher
STATE:    RUNNING
UPTIME:   00:00:01
RESTARTS: 2

# Después de ~10 segundos (superó los 5 restarts):
$ toaddX-cli status 4
IID:      4
PID:      15800
BINARY:   /home/user/.../tests/crasher
STATE:    FAILED
UPTIME:   00:00:01
RESTARTS: 6
```

### 9. Verificar que el daemon sobrevive al cierre del terminal

```bash
# Cerrar la terminal completamente, abrir una nueva:
$ toaddX-cli ps
IID    PID      STATE      UPTIME     BINARY
2      14203    STOPPED    00:05:00   /home/user/.../tests/infinite_loop
3      15002    STOPPED    00:04:00   /home/user/.../tests/infinite_loop
4      15800    FAILED     00:03:30   /home/user/.../tests/crasher
```

> El daemon sigue corriendo. Esto es porque se daemoniza con double fork + `setsid()`.

### 10. Ver los logs del daemon

```bash
$ tail /tmp/toaddx.log
[2026-04-24 22:00:00] toaddX daemon started (PID 13500, max_restarts=5)
[2026-04-24 22:00:00] FIFOs created: /tmp/toaddx_req, /tmp/toaddx_res
[2026-04-24 22:00:01] START iid=2 pid=14203 bin=/.../infinite_loop
[2026-04-24 22:00:30] STOP iid=2 pid=14203
[2026-04-24 22:00:30] REAP pid=14203 iid=2 explicit=1 exit_status=0
[2026-04-24 22:01:00] START iid=4 pid=15300 bin=/.../crasher
[2026-04-24 22:01:01] REAP pid=15300 iid=4 explicit=0 exit_status=-1
[2026-04-24 22:01:01] RESTART iid=4 attempt=1/5 bin=/.../crasher
[2026-04-24 22:01:02] RESTART iid=4 attempt=2/5 bin=/.../crasher
...
[2026-04-24 22:01:06] FAILED iid=4 after 6 restarts
```

---

## ⚙️ Configuración

| Variable | Default | Descripción |
|----------|---------|-------------|
| `TOADDX_MAX_RESTARTS` | `5` | Máximo de auto-restarts consecutivos antes de marcar como FAILED |

```bash
export TOADDX_MAX_RESTARTS=10
pkill toaddX           # Detener daemon actual
toaddX                 # Reiniciar con nueva configuración
```

---

## 📁 Estructura del Proyecto

```
lab1/toaddX/
├── common.h          # Protocolo IPC, constantes, structs compartidas
├── toaddX.c          # Daemon gestor de procesos
├── toaddX-cli.c      # Cliente CLI
├── Makefile          # Build, install, clean
├── README.md         # Este archivo
└── tests/
    ├── infinite_loop.c   # Loop infinito (probar ps, stop, kill)
    ├── short_task.c      # Termina en 2s (probar auto-restart)
    └── crasher.c         # Crash cada 1s (probar FAILED)
```

---

## 🗑️ Desinstalación

```bash
sudo make uninstall    # Eliminar de /usr/local/bin
make clean             # Limpiar binarios y FIFOs
```

---

## 📝 Arquitectura

- **IPC:** Dos FIFOs en `/tmp/` (request y response) con structs de tamaño fijo
- **Daemonización:** Double fork + `setsid()` + redirigir fds a `/dev/null`
- **Señales:** `SIGCHLD` con handler async-signal-safe + `waitpid(WNOHANG)` en loop principal
- **Kill descendants:** Process groups (`setpgid` + `kill(-pgid, SIGKILL)`)
- **Bonus:** Auto-restart con mismo IID, contador de restarts, estado FAILED tras N muertes
