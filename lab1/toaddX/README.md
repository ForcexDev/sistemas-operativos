# 🐸 toaddX — Process Manager

**toaddX** es un gestor de procesos tipo daemon para Linux. Ejecuta, monitorea y controla procesos desde la línea de comandos, con auto-restart automático ante crashes inesperados.

> **Tarea 1 — Sistemas Operativos**
> Procesos y Syscalls

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

## 🗑️ Desinstalación

```bash
sudo make uninstall    # Eliminar de /usr/local/bin
make clean             # Limpiar binarios y FIFOs
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
``

---

## 🧠 Decisiones de Diseño

| Decisión | Por qué |
|----------|---------|
| `poll()` en vez de `read()` bloqueante | Permite verificar `SIGCHLD` (hijos muertos) entre peticiones. Un `read()` bloqueante dejaría al daemon congelado sin poder recoger hijos |
| `waitpid(-1, ..., WNOHANG)` | Recoge hijos muertos sin bloquearse. `WNOHANG` retorna inmediatamente si no hay hijos que recoger, permitiendo volver al loop |
| `setpgid(0, 0)` en cada hijo | Aísla cada proceso en su propio grupo. Así `kill(-pgid, SIGKILL)` mata al proceso Y a todos sus descendientes sin afectar al daemon |
| Double fork para daemonizar | El primer `fork()` + `setsid()` crea una nueva sesión. El segundo `fork()` previene que el proceso pueda re-adquirir una terminal de control |
| FIFOs con structs de tamaño fijo | Evita problemas de parsing de texto. Se envía `sizeof(struct request)` bytes exactos y se lee esa misma cantidad |
| `volatile sig_atomic_t` para la bandera | Tipo garantizado por el estándar C como seguro para acceso desde un signal handler. Usar `int` normal podría causar bugs de optimización |
| `_exit(127)` en vez de `exit(127)` | `_exit` termina sin ejecutar handlers de `atexit` ni hacer flush de buffers. En un hijo post-fork esto es importante para no duplicar side effects del padre |
| `SA_RESTART` en `sigaction` | Hace que las syscalls interrumpidas por `SIGCHLD` (como `poll`) se reinicien automáticamente en vez de fallar con `EINTR` |
| `SA_NOCLDSTOP` en `sigaction` | Evita recibir `SIGCHLD` cuando un hijo es detenido con `SIGSTOP` (solo nos interesa cuando muere) |

---

## 📡 Diagrama de Comunicación IPC

```
              toaddX-cli                              toaddX (daemon)
           ┌──────────────┐                      ┌──────────────────┐
           │ 1. Parsear   │                      │                  │
           │    comando   │                      │  poll() esperando│
           │              │                      │  en FIFO_REQ     │
           │ 2. Escribir  │   struct request     │                  │
           │    request ──│─────── FIFO ────────►│ 3. Leer request  │
           │              │  /tmp/toaddx_req     │    y procesar    │
           │              │                      │    (handle_*)    │
           │ 4. Leer      │   struct response    │                  │
           │    response◄─│─────── FIFO ─────────│ 4. Escribir      │
           │              │  /tmp/toaddx_res     │    response      │
           │ 5. Imprimir  │                      │                  │
           └──────────────┘                      └──────────────────┘
```

**Nota:** La comunicación es sincrónica — el CLI se bloquea esperando la respuesta. Las escrituras son atómicas porque `sizeof(struct request)` < `PIPE_BUF` (4096 bytes en Linux).

---

## ❓ Preguntas Frecuentes

**¿Por qué no usan threads?**
> El modelo con `poll()` + señales es la forma clásica de manejar concurrencia en daemons Unix de un solo hilo.

**¿Qué pasa si el daemon muere?**
> Los FIFOs quedan huérfanos en `/tmp/`. El CLI detectará que no puede abrir el FIFO (el `open()` con `O_NONBLOCK` falla con `ENXIO`) y ofrecerá reiniciar el daemon automáticamente.

**¿Qué pasa si dos CLIs envían comandos simultáneamente?**
> Las escrituras menores a `PIPE_BUF` (4096 bytes) son atómicas en Linux, por lo que no hay corrupción de datos. Sin embargo, el daemon procesa secuencialmente, así que uno esperará al otro.

**¿Por qué `setpgid` se llama tanto en el hijo como en el padre?**
> Race condition: si el padre llama `kill(-pgid)` antes de que el hijo haya ejecutado su `setpgid`, el kill fallaría. Al llamarlo en ambos, se garantiza que el grupo está configurado antes de cualquier uso.

**¿Qué significa `_exit(127)` en vez de `exit(127)`?**
> `_exit` termina el proceso sin ejecutar handlers de `atexit()` ni hacer flush de buffers de `stdio`. En un hijo post-`fork()`, usar `exit()` podría duplicar la salida del padre si hay buffers pendientes.

**¿Por qué se usa `volatile sig_atomic_t` para `got_sigchld`?**
> El compilador podría optimizar la lectura de una variable global y no volver a leerla del memory. `volatile` fuerza a leerla cada vez. `sig_atomic_t` garantiza que la escritura desde el handler sea atómica.

**¿Cómo funciona el auto-restart (bonus)?**
> Cuando un hijo muere sin que el usuario haya ejecutado `stop` o `kill` (`was_explicit_stop == 0`), `handle_unexpected_death()` incrementa un contador y relanza el binario con `fork+exec`, conservando el mismo IID. Si supera N reintentos, lo marca como FAILED.

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

## 📝 Arquitectura

- **IPC:** Dos FIFOs en `/tmp/` (request y response) con structs de tamaño fijo
- **Daemonización:** Double fork + `setsid()` + redirigir fds a `/dev/null`
- **Señales:** `SIGCHLD` con handler async-signal-safe + `waitpid(WNOHANG)` en loop principal
- **Kill descendants:** Process groups (`setpgid` + `kill(-pgid, SIGKILL)`)
- **Bonus:** Auto-restart con mismo IID, contador de restarts, estado FAILED tras N muertes
