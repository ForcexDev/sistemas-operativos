# 🐸 toaddX — Process Manager

**toaddX** is a lightweight process manager for Linux. It manages, monitors, and controls processes from the command line.

## Quick Install

```bash
git clone <URL_DEL_REPOSITORIO>
cd sistemas-operativos/lab1/toaddX
make
sudo make install
```

After installing, you can use `toaddX-cli` from anywhere in your system.

## Usage

```bash
# Start a process
toaddX-cli start /path/to/binary

# List all managed processes
toaddX-cli ps

# Detailed process info
toaddX-cli status <iid>

# Stop a process (SIGTERM)
toaddX-cli stop <iid>

# Kill a process and all its descendants (SIGKILL)
toaddX-cli kill <iid>

# List zombie processes
toaddX-cli zombie
```

> **Note:** The daemon starts automatically when you run any command.
> You can also start it manually with `toaddX`.

## Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `TOADDX_MAX_RESTARTS` | `5` | Max consecutive auto-restarts before marking a process as FAILED |

```bash
export TOADDX_MAX_RESTARTS=10
toaddX   # restart daemon to apply
```

## Process States

| State | Meaning |
|-------|---------|
| `RUNNING` | Process is executing |
| `STOPPED` | Process was explicitly stopped via `stop` or `kill` |
| `ZOMBIE` | Process terminated but exit status not yet collected |
| `FAILED` | Process crashed more than N consecutive times (bonus) |

## Logs

```bash
tail -f /tmp/toaddx.log
```

## Uninstall

```bash
sudo make uninstall
make clean
```
