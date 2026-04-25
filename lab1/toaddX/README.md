# 🚀 Guía de Instalación y Uso (Linux)

Sigue estos pasos para instalar y ejecutar **toaddX** en tu entorno Linux.

## 1. Instalación desde GitHub

Desde tu terminal de Linux, ejecuta los siguientes comandos:

```bash
# Clonar el repositorio
git clone [URL_DEL_REPOSITORIO]

# Entrar a la carpeta del proyecto
cd sistemas-operativos/lab1/toaddX

# Compilar el sistema completo (Daemon, CLI y Tests)
make
```

## 2. Iniciar el Gestor (Daemon)

El daemon debe iniciarse una sola vez. Se ejecutará en segundo plano y sobrevivirá incluso si cierras la terminal.

```bash
./toaddX
```
*Verás el logo ASCII del sapo 🐸 confirmando que el proceso se ha iniciado y se ha movido al background.*

## 3. Comandos Principales (CLI)

Ahora puedes interactuar con el gestor usando `toaddX-cli`.

| Acción | Comando |
| :--- | :--- |
| **Iniciar un programa** | `./toaddX-cli start ./tests/infinite_loop` |
| **Listar procesos** | `./toaddX-cli ps` |
| **Ver estado detallado** | `./toaddX-cli status [IID]` |
| **Detener proceso (SIGTERM)** | `./toaddX-cli stop [IID]` |
| **Matar proceso y descendientes** | `./toaddX-cli kill [IID]` |
| **Ver procesos Zombie** | `./toaddX-cli zombie` |

## 4. Pruebas de Funcionamiento

### Probar Auto-Restart (Bonus)
El sistema reiniciará automáticamente cualquier proceso que muera de forma inesperada (hasta 5 veces por defecto).
```bash
# Iniciar el programa que crashea
./toaddX-cli start ./tests/crasher

# Observar cómo aumentan los RESTARTS
./toaddX-cli status [IID]
```

### Monitorear Logs
Si quieres ver qué está pasando "bajo el capó", puedes revisar el archivo de log del daemon:
```bash
tail -f /tmp/toaddx.log
```

## 5. Mantenimiento y Limpieza

- **Detener el Daemon:** `pkill toaddX`
- **Limpiar archivos compilados:** `make clean`
- **Configurar intentos de reinicio:** `export TOADDX_MAX_RESTARTS=10` (antes de iniciar `./toaddX`)
