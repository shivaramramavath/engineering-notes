# Architecture

## Concept
nginx uses an **event-driven, asynchronous, non-blocking** design: a small, fixed number of worker processes each handle thousands of connections. This is why nginx uses little memory per connection and scales well, and it explains many of its configuration rules.

**Prerequisites:** [config-structure.md](../01-fundamentals/config-structure.md)

## Process Model

```
                  ┌──────────────────┐
                  │  master process  │   runs as root
                  │ (config, signals,│   binds ports 80/443
                  │  worker control) │
                  └────────┬─────────┘
          ┌────────────────┼─────────────────┐
          ▼                ▼                 ▼
   ┌────────────┐   ┌────────────┐    ┌────────────┐
   │  worker 1  │   │  worker 2  │ …  │  worker N  │   run as unprivileged user
   │ event loop │   │ event loop │    │ event loop │   (www-data / nginx)
   └────────────┘   └────────────┘    └────────────┘
          cache manager / cache loader (only if proxy_cache is used)
```

| Process | Role |
|---|---|
| **Master** | Reads and validates config, opens listening sockets, starts/stops workers, handles signals. Does not serve requests |
| **Worker** | Accepts connections and processes requests inside an event loop. Runs as the `user` set in `nginx.conf` |
| **Cache manager** | Periodically removes cache entries to enforce `max_size` and `inactive` |
| **Cache loader** | Runs at startup to load cache metadata into the shared memory zone |

```bash
ps -o pid,ppid,user,cmd -C nginx      # one master (root) + N workers
```

## The Event Loop
Each worker is single-threaded. Instead of one thread per connection, it asks the OS which sockets are ready (via `epoll` on Linux, `kqueue` on BSD/macOS) and does a small piece of work for each, then moves on. Waiting on a slow client or a slow backend costs almost nothing, because the worker is free to serve other connections in the meantime.

Consequences:
- Idle keepalive connections are cheap (a few KB each).
- **Anything that blocks a worker blocks all its connections.** Slow disk reads, blocking third-party modules, or heavy Lua/njs code stall every request on that worker.
- Per-connection overhead is low, so nginx handles slow clients (the "C10K problem") far better than process-per-connection servers.

## Key Directives

```nginx
worker_processes auto;            # one worker per CPU core (recommended)
worker_rlimit_nofile 65535;       # open-file limit per worker

events {
    worker_connections 4096;      # max simultaneous connections per worker
    multi_accept off;
    # use epoll;                  # auto-detected, rarely needs setting
}
```

### Connection capacity
```
max clients ≈ worker_processes × worker_connections        (serving static files)
max clients ≈ worker_processes × worker_connections ÷ 2    (reverse proxy)
```
A proxied request uses two connections: client to nginx, and nginx to backend. The real ceiling is also limited by the OS file-descriptor limit, so `worker_rlimit_nofile` should be at least `worker_connections` (in practice, higher, since each connection can also need file handles for cached or static files).

## How Workers Share Work
- The master opens the listening sockets; all workers inherit them.
- By default, the kernel and nginx decide which worker accepts a new connection (`accept_mutex` is **off** by default since nginx 1.11.3).
- `listen 80 reuseport;` creates a separate listening socket per worker, letting the kernel distribute connections. This can improve balance and throughput on busy servers.
- Workers share state through **shared memory zones**: `limit_req_zone`, `limit_conn_zone`, `proxy_cache_path keys_zone`, `upstream` state, and `ssl_session_cache shared:`. This is why rate limits and cache keys are consistent across all workers.

## Blocking Work and Thread Pools
Disk I/O can block a worker when files are not in the OS page cache. For large files or slow storage:
```nginx
aio threads;              # offload file reads to a thread pool
sendfile on;              # kernel copies file to socket, no userspace copy
directio 8m;              # bypass page cache for files larger than 8 MB (optional)
```
Thread pools apply only to file I/O. Request handling itself remains single-threaded per worker.

## Reload and Signals
nginx is controlled by signals sent to the master.

| Signal | `nginx -s` | Effect |
|---|---|---|
| `HUP` | `reload` | Validate new config; if valid, start new workers and gracefully stop old ones |
| `QUIT` | `quit` | Graceful shutdown: finish in-flight requests, then exit |
| `TERM` / `INT` | `stop` | Fast shutdown |
| `USR1` | `reopen` | Reopen log files (used after log rotation) |
| `USR2` | n/a | Start a new master with a new binary (on-the-fly upgrade) |
| `WINCH` | n/a | Gracefully stop workers of the old master (during upgrade) |

### What a reload really does
1. Master checks the new config. On error, it keeps the **old config running** and logs the problem.
2. On success, master starts new workers with the new config.
3. Old workers stop accepting new connections and exit after their current requests finish.

So a reload is zero-downtime, but long-lived connections (WebSockets, streams, large downloads) keep old workers alive until they close. Multiple "worker process is shutting down" entries in `ps` are normal during that period. Use `worker_shutdown_timeout 30s;` to force old workers to exit after a limit.

## Memory Characteristics
- Per-connection memory is small, but buffers add up: `proxy_buffers`, `client_body_buffer_size`, `large_client_header_buffers`, and `gzip_buffers` are per connection or request.
- Shared zones are allocated once at startup and sized by config.
- A reload temporarily runs old and new workers together, roughly doubling worker memory for a moment.

## Common Mistakes
- Setting `worker_processes` far above CPU cores. It gives no gain and more context switching.
- Raising `worker_connections` without raising OS or `worker_rlimit_nofile` limits, which causes `Too many open files` errors.
- Forgetting the 2x connection use when sizing a reverse proxy.
- Blocking operations in modules or scripts that stall the worker.
- Expecting config changes to apply without `reload`.
- Assuming a reload kills long connections (it doesn't; they drain slowly).

## Debugging
```bash
ps -ef | grep nginx                        # master and workers; "shutting down" = draining
sudo ss -s                                 # socket totals
sudo cat /proc/$(pgrep -o nginx)/limits | grep "open files"
sudo grep -i "worker_connections\|too many open files" /var/log/nginx/error.log
curl http://localhost/nginx_status         # if stub_status is enabled; see operations.md
```

## Related / Next
- [request-lifecycle.md](request-lifecycle.md): what a worker does with each request
- [performance-tuning](../05-security-performance/performance-tuning.md): applying these settings
- [operations](../06-production/operations.md): reload, upgrade, and log rotation in practice
