# Module 04: Internals & Architecture

## Overview

Module 04 of the NGINX curriculum dives into the low-level operating system interactions, process hierarchy, memory management, and asynchronous event-driven mechanics that make NGINX one of the world's most performant reverse proxies.

This module covers the master-worker concurrency model, non-blocking I/O multiplexing (`epoll`/`kqueue`), the 11 HTTP phase handlers, configuration compilation, zero-downtime binary upgrades, slab memory allocation, shared memory dictionaries, dynamic C modules, and source code navigation with `gdb`.

---

## Architectural Systems Layout

```
                                  MASTER PROCESS
                        (Root privileges, Signals, Cycle)
                                       │
        ┌──────────────────────────────┼──────────────────────────────┐
        ▼                              ▼                              ▼
  WORKER PROCESS 0               WORKER PROCESS 1               WORKER PROCESS 2
  (CPU 0 Affinity)               (CPU 1 Affinity)               (CPU 2 Affinity)
  - Non-blocking epoll loop      - Non-blocking epoll loop      - Non-blocking epoll loop
  - Pre-allocated connections    - Pre-allocated connections    - Pre-allocated connections
  - Region pools (ngx_pool_t)    - Region pools (ngx_pool_t)    - Region pools (ngx_pool_t)
        │                              │                              │
        └──────────────────────────────┼──────────────────────────────┘
                                       │
                                       ▼
                       SHARED MEMORY SEGMENTS (mmap)
         ┌────────────────────────────────────────────────────────────┐
         │ - ngx_slab_pool_t (Buddy allocator + slot lists)           │
         │ - Rate limiting zones (limit_req / limit_conn rbtrees)     │
         │ - Proxy cache metadata hash tables                         │
         │ - Atomic spinlocks (ngx_shmtx_t)                           │
         └────────────────────────────────────────────────────────────┘
```

---

## Module Notes Catalog

| # | Note | Key Topics Covered |
| :--- | :--- | :--- |
| **01** | [Architecture: Master-Worker & Event Loop](01-architecture-master-worker-and-event-loop.md) | Single master, multi-worker process model, asynchronous event loop, `epoll`/`kqueue`/`eventport`, socket sharding (`SO_REUSEPORT`), CPU core affinity pinning. |
| **02** | [Request Lifecycle & Phases](02-request-lifecycle-and-phases.md) | Deep breakdown of the 11 HTTP processing phases (Post-Read through Log), internal redirects, phase checkers, phase return codes (`NGX_OK`, `NGX_AGAIN`, `NGX_DONE`). |
| **03** | [Configuration Parsing & Reload Mechanics](03-config-parsing-and-reload-mechanics.md) | `ngx_init_cycle()`, context merging (`http`, `server`, `location`), memory inheritance, zero-downtime reloads (`kill -HUP`), binary upgrades (`kill -USR2`). |
| **04** | [Memory Buffers & Shared Zones](04-memory-buffers-and-shared-zones.md) | Arena memory management (`ngx_pool_t`), large allocations, shared memory (`shm`), slab allocation algorithms, atomic spinlocks, debugging leaks. |
| **05** | [Modules & Dynamic Modules](05-modules-and-dynamic-modules.md) | NGINX module architecture (`ngx_module_t`), static vs dynamic compilation (`load_module`), binary compatibility rules, module context callbacks. |
| **06** | [Reading NGINX Source Code](06-reading-nginx-source-code.md) | Navigating `src/core`, `src/event`, `src/http`, core structs (`ngx_str_t`, `ngx_http_request_t`, `ngx_connection_t`), `gdb` debugging, and tracing phase handlers. |

---

## Core Mental Models & Principles

1. **Process Isolation Over Threading**: Workers do not share stack or heap memory. Crashes in one worker cannot corrupt the memory space of other workers.
2. **Zero-Copy & Non-Blocking Invariant**: No worker process may ever execute a blocking system call. File transfers leverage kernel `sendfile(2)`, and disk I/O offloads to Linux AIO thread pools.
3. **Phase-Driven Processing**: Directives execute in phase order, not textual order. A `rewrite` directive always executes before an `access` check, regardless of where it is written in a `location` block.
4. **Arena Allocation**: Requests allocate memory from an arena pool (`ngx_pool_t`) that is destroyed in a single operation upon request completion, preventing memory leaks and heap fragmentation.

---

[← Previous Module: 03-traffic-management](../03-traffic-management/README.md) | [Back to Track Index](../README.md) | [Next Module: 05-security-performance →](../05-security-performance/README.md)
