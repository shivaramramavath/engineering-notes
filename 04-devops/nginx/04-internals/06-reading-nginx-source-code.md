---
title: "Reading NGINX Source Code & Core Data Structures"
description: "Comprehensive guide to navigating, understanding, and debugging the C source code of NGINX Core, including core structs, memory layouts, gdb inspection, and event loops."
module: "04-internals"
topic: "06-reading-nginx-source-code"
status: "completed"
tags: ["nginx", "c", "source-code", "internals", "gdb", "debugging", "data-structures", "core"]
prerequisites: ["04-internals/01-architecture-master-worker-and-event-loop.md", "04-internals/04-memory-buffers-and-shared-zones.md"]
estimated_read_time: "35 min"
---

# Reading NGINX Source Code & Core Data Structures

```
+-----------------------------------------------------------------------------+
| NGINX C Source Tree Architecture & Core Data Structures                     |
|                                                                             |
|  src/                                                                       |
|  |-- core/      --> ngx_cycle.h, ngx_palloc.h, ngx_string.h, ngx_array.h    |
|  |-- event/     --> ngx_event.h, ngx_epoll_module.c, ngx_event_posted.h     |
|  |-- http/      --> ngx_http_request.h, ngx_http_core_module.c, modules/    |
|  |-- mail/      --> Mail proxy subsystems (POP3, IMAP, SMTP)                |
|  |-- stream/    --> Layer 4 TCP/UDP proxy subsystem                         |
|  `-- os/unix/   --> ngx_process.c, ngx_files.c, ngx_posix_init.c             |
|                                                                             |
|  Core Object Graph:                                                         |
|  [ngx_cycle_t]                                                              |
|        |                                                                    |
|        v                                                                    |
|  [ngx_connection_t] <---> [ngx_event_t (read)] & [ngx_event_t (write)]      |
|        |                                                                    |
|        v                                                                    |
|  [ngx_http_request_t]                                                       |
|        |--> pool (ngx_pool_t)                                               |
|        |--> headers_in / headers_out                                        |
|        `--> uri, args, exten (ngx_str_t)                                    |
+-----------------------------------------------------------------------------+
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts & Source Tree Layout](#2-core-concepts--source-tree-layout)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Key C Structs & API Reference](#4-key-c-structs--api-reference)
5. [Practical Implementation & Debugging Examples](#5-practical-implementation--debugging-examples)
6. [Production Best Practices & Safe Code Auditing](#6-production-best-practices--safe-code-auditing)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance, Memory & Security Implications](#8-performance-memory--security-implications)
9. [Architectural Comparison: NGINX vs Envoy vs Node.js Source Layout](#9-architectural-comparison-nginx-vs-envoy-vs-nodejs-source-layout)
10. [Technical Interview Questions & Scenarios](#10-technical-interview-questions--scenarios)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading & Source Pointers](#13-further-reading--source-pointers)

---

## 1. What & Why

Reading NGINX source code is an essential skill for senior infrastructure engineers, performance architects, and systems developers:
- **Disambiguating Undocumented Edge Cases**: Official documentation describes directive parameters, but only source code reveals precise execution ordering, default fallback branches, and system call error handling.
- **Root-Cause Analysis of Production Panics**: When NGINX worker processes dump core (SIGSEGV/SIGBUS) or experience silent memory growth, inspecting core dumps with `gdb` requires understanding NGINX's internal structs (`ngx_http_request_t`, `ngx_cycle_t`, `ngx_pool_t`).
- **Custom Module Development**: Building native C or dynamic modules requires deep familiarity with NGINX's idiomatic C style, memory pools, non-blocking asynchronous hooks, and string representations.
- **Auditing Security Advisories**: Evaluating whether a CVE affects a customized deployment requires reading patches across upstream releases to evaluate vulnerable code paths.

---

## 2. Core Concepts & Source Tree Layout

The NGINX source codebase adheres to strict stylistic and architectural conventions:
- **Zero Third-Party Dependency in Core**: Except for system libc, zlib, PCRE2, and OpenSSL, NGINX implements all fundamental data structures (arrays, strings, hash tables, red-black trees, memory pools) from scratch.
- **Idiomatic Types**: Primitive types are wrapped in system-agnostic typedefs (`ngx_int_t`, `ngx_uint_t`, `ngx_str_t`, `ngx_msec_t`).
- **Directory Hierarchy**:

| Directory | Purpose | Key Header / Source Files |
| :--- | :--- | :--- |
| `src/core/` | Fundamental types, string routines, memory pools, logging, cycle configuration | `ngx_cycle.h`, `ngx_palloc.h`, `ngx_string.h`, `ngx_array.h`, `ngx_rbtree.h` |
| `src/event/` | Event-driven architecture, timer rbtree, epoll/kqueue abstraction, connection management | `ngx_event.h`, `ngx_event.c`, `ngx_epoll_module.c`, `ngx_event_timer.c` |
| `src/event/modules/` | OS-specific multiplexers | `ngx_epoll_module.c`, `ngx_kqueue_module.c`, `ngx_devpoll_module.c`, `ngx_select_module.c` |
| `src/http/` | HTTP engine, request parsing, phase engines, upstream handling | `ngx_http.h`, `ngx_http_request.h`, `ngx_http_core_module.c`, `ngx_http_upstream.h` |
| `src/http/modules/` | Standard HTTP modules (proxy, gzip, fastcgi, ssl, rewrite) | `ngx_http_proxy_module.c`, `ngx_http_ssl_module.c`, `ngx_http_rewrite_module.c` |
| `src/http/v2/` & `v3/` | HTTP/2 and HTTP/3 QUIC protocol parsers and frame processors | `ngx_http_v2.c`, `ngx_http_v3.c`, `ngx_http_v3_filter_module.c` |
| `src/stream/` | Layer 4 TCP/UDP proxy engine | `ngx_stream.h`, `ngx_stream_proxy_module.c`, `ngx_stream_upstream.h` |
| `src/os/unix/` | UNIX process spawning, signals, daemonization, sendfile, shared memory wrappers | `ngx_process_cycle.c`, `ngx_files.c`, `ngx_shmem.c`, `ngx_alloc.c` |

---

## 3. How It Works Internally

### Connection and Request Object Relationships

```
                     +---------------------------+
                     |        ngx_cycle_t        |
                     |  - pool: ngx_pool_t*      |
                     |  - conf_ctx: void****     |
                     |  - files: ngx_file_t**    |
                     +-------------+-------------+
                                   |
                +------------------+------------------+
                |                                     |
                v                                     v
     +--------------------+                +--------------------+
     | ngx_connection_t   |                | ngx_connection_t   |
     | [Listening Socket] |                | [Client Socket]    |
     | fd: 6              |                | fd: 12             |
     | read: ngx_event_t  |                | read: ngx_event_t  |
     | write: ngx_event_t |                | write: ngx_event_t |
     +--------------------+                +----------+---------+
                                                      |
                                                      | data
                                                      v
                                           +--------------------+
                                           | ngx_http_request_t |
                                           | - pool: ngx_pool_t |
                                           | - uri: ngx_str_t   |
                                           | - headers_in: ...  |
                                           | - headers_out: ... |
                                           | - phase_handler: 7 |
                                           +--------------------+
```

1. **Cycle Initialization**: When NGINX starts or reloads, `ngx_init_cycle()` allocates memory pools, parses configuration, and allocates pre-sized connection tables (`cycle->connections`).
2. **Accept Phase**: On an incoming TCP handshake, `ngx_epoll_module.c` dispatches a read event on the listening connection, calling `ngx_event_accept()`.
3. **HTTP Binding**: The newly accepted connection assigns its `c->data` pointer to an initialized `ngx_http_request_t` instance created from its own dedicated memory pool.
4. **Phase Engine Progression**: The request executes sequentially across HTTP phase handlers recorded in `cycle->conf_ctx` until completing or delegating to upstream sockets.

---

## 4. Key C Structs & API Reference

### 1. `ngx_str_t` (The Counted String)
Unlike standard C null-terminated strings (`char*`), NGINX strings are strictly length-prefixed:
```c
typedef struct {
    size_t      len;
    u_char     *data;
} ngx_str_t;
```
*Key invariant*: `data` is NOT guaranteed to be null-terminated (`\0`). Passing `str.data` directly to `printf("%s")` or `open()` causes illegal memory reads. Use `ngx_snprintf` or allocate a null-terminated copy.

### 2. `ngx_pool_t` (Memory Allocation Arena)
Defined in `src/core/ngx_palloc.h`:
```c
struct ngx_pool_s {
    ngx_pool_data_t       d;
    size_t                max;
    ngx_pool_t           *current;
    ngx_chain_t          *chain;
    ngx_pool_large_t     *large;
    ngx_pool_cleanup_t   *cleanup;
    ngx_log_t            *log;
};
```
- Allocations below `pool->max` are carved from contiguous blocks without individual `free()` tracking.
- Allocations exceeding `pool->max` go to `pool->large` via standard `malloc()`.
- Destroying the pool (`ngx_destroy_pool()`) invokes all `cleanup` callbacks and reclaims all memory in bulk.

### 3. `ngx_connection_t` (Network Sockets)
Defined in `src/core/ngx_connection.h`:
```c
struct ngx_connection_s {
    void               *data;         /* Associated protocol context (e.g., ngx_http_request_t) */
    ngx_event_t        *read;         /* Read event hook */
    ngx_event_t        *write;        /* Write event hook */
    ngx_socket_t        fd;           /* Operating system file descriptor */
    ngx_recv_pt         recv;         /* OS-specific non-blocking receive function pointer */
    ngx_send_pt         send;         /* OS-specific non-blocking transmit function pointer */
    ngx_pool_t         *pool;         /* Connection-lifetime pool */
    struct sockaddr    *sockaddr;     /* Client IP/port socket structure */
    socklen_t           socklen;
    ngx_str_t           addr_text;    /* Human-readable IP string */
    unsigned            timedout:1;
    unsigned            close:1;
};
```

### 4. `ngx_http_request_t` (HTTP Transaction)
Defined in `src/http/ngx_http_request.h`:
```c
struct ngx_http_request_s {
    uint32_t                          signature;         /* 'HTTP' identifier */
    ngx_connection_t                 *connection;
    void                            **ctx;               /* Array of module-specific contexts */
    void                            **main_conf;
    void                            **srv_conf;
    void                            **loc_conf;
    ngx_http_headers_in_t             headers_in;
    ngx_http_headers_out_t            headers_out;
    ngx_http_request_body_t          *request_body;
    ngx_str_t                         uri;
    ngx_str_t                         args;
    ngx_str_t                         exten;
    ngx_str_t                         unparsed_uri;
    ngx_str_t                         method_name;
    ngx_uint_t                        method;
    ngx_uint_t                        http_version;
    ngx_pool_t                       *pool;
    ngx_chain_t                      *out;
    ngx_http_upstream_t              *upstream;
};
```

---

## 5. Practical Implementation & Debugging Examples

### Example 1: Compiling NGINX for Source Debugging with GDB
To step through source code with line symbols, compile NGINX with `-O0 -g`:

```bash
# Clone mainline source
git clone https://github.com/nginx/nginx.git
cd nginx

# Configure with debug flags and core modules
./auto/configure \
    --prefix=/opt/nginx-debug \
    --with-debug \
    --with-http_ssl_module \
    --with-http_v2_module \
    --with-http_realip_module \
    --with-cc-opt="-O0 -g -fno-inline"

make -j$(nproc)
sudo make install
```

### Example 2: Configuring NGINX for Single-Process Debugging
Running multiple worker processes under `gdb` causes complex process attach issues. Configure single-process mode in `nginx.conf`:

```nginx
# /opt/nginx-debug/conf/nginx.conf
daemon off;
master_process off;
worker_processes 1;
error_log /opt/nginx-debug/logs/error.log debug;

events {
    worker_connections 1024;
}

http {
    server {
        listen 8080;
        server_name localhost;

        location /debug-test {
            return 200 "Debugging NGINX Internals\n";
        }
    }
}
```

### Example 3: GDB Session to Inspect `ngx_http_request_t`
Launch `gdb` against the compiled binary:

```bash
sudo gdb /opt/nginx-debug/sbin/nginx
```

Inside GDB:
```gdb
(gdb) break ngx_http_core_content_phase
Breakpoint 1 at 0x48b210: file src/http/ngx_http_core_module.c, line 1380.
(gdb) run -c /opt/nginx-debug/conf/nginx.conf
```

Send a test request from another terminal:
```bash
curl http://127.0.0.1:8080/debug-test?account_id=9876
```

Inspect the breakpoint in GDB:
```gdb
Thread 1 "nginx" hit Breakpoint 1, ngx_http_core_content_phase (r=0x6dd050, ph=0x6de5c0)
    at src/http/ngx_http_core_module.c:1380
1380    {

# Print URI (ngx_str_t)
(gdb) print r->uri
$1 = {len = 11, data = 0x6dd280 "/debug-test"}

# Print Query String Args (ngx_str_t)
(gdb) print r->args
$2 = {len = 15, data = 0x6dd28c "account_id=9876"}

# Print HTTP Method
(gdb) print r->method_name
$3 = {len = 3, data = 0x6dd278 "GET"}

# Print Client IP string from the connection struct
(gdb) print r->connection->addr_text
$4 = {len = 9, data = 0x6db110 "127.0.0.1"}

# Continue execution
(gdb) continue
```

### Example 4: GDB Pretty-Printing for `ngx_str_t`
Add this Python helper to `~/.gdbinit` to automatically format `ngx_str_t` instances:

```python
# ~/.gdbinit
python
import gdb

class NgxStringPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        length = int(self.val['len'])
        data_ptr = self.val['data']
        if not data_ptr:
            return '""'
        data_bytes = gdb.selected_inferior().read_memory(data_ptr, length)
        return '"{}"'.format(bytes(data_bytes).decode('utf-8', errors='replace'))

def ngx_lookup_type(val):
    if str(val.type.unqualified()) == 'ngx_str_t':
        return NgxStringPrinter(val)
    return None

gdb.pretty_printers.append(ngx_lookup_type)
end
```

### Example 5: Tracing Phase Execution with GDB Watchpoints
Trace how request state advances through internal phase indexes:

```gdb
(gdb) break ngx_http_process_request
Breakpoint 2 at 0x489620: file src/http/ngx_http_request.c, line 1920.
(gdb) run
# Trigger curl in other terminal
(gdb) step
(gdb) print r->phase_handler
$1 = 0
(gdb) display r->phase_handler
(gdb) next
# Observe r->phase_handler incrementing as each phase checker function returns NGX_OK
```

---

## 6. Production Best Practices & Safe Code Auditing

- **Always Verify String Bounds**: When auditing or modifying C source, never assume `ngx_str_t.data` has space for a trailing `\0`. Always use `ngx_pstrdup()` or allocate `len + 1` before invoking C standard library string utilities.
- **Isolate Module Work in Connection Pools**: Memory allocated in `r->pool` is retained until the entire HTTP request finishes. For long-lived WebSocket or SSE connections, allocate temporary buffers in custom sub-pools and destroy them periodically (`ngx_destroy_pool()`) to prevent unbounded resident memory growth.
- **Never Block in Worker Threads**: Never call blocking synchronous system calls (`connect()`, `sleep()`, `gethostbyname()`) inside core handlers. All I/O must integrate into `r->connection->read` or `ngx_event_timer_t`.
- **Match Configurations to Compiled Modules**: Use `nginx -V` in production to inspect the exact configure flags, module lists, and compiler flags matching your deployed binary.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern / Mistake | Root Cause in Source Code | Observable Production Impact | Correct Solution |
| :--- | :--- | :--- | :--- |
| Treating `ngx_str_t` as `char*` | Calling `strlen(s.data)` or passing to `strcpy()` | Segment fault or reading memory beyond string boundary | Respect `s.len` or allocate a null-terminated copy |
| Manual `free()` on Pool Pointers | Calling standard libc `free()` on pointer from `ngx_palloc()` | Heap corruption / glibc double-free crash | Allow `ngx_destroy_pool()` to reclaim memory automatically |
| Blocking Syscalls in Content Handlers | Invoking synchronous file reads without `aio` or network socket locks | Entire worker process stalls; all concurrent clients freeze | Use NGINX thread pools (`aio threads`) or asynchronous event hooks |
| Leaking Large Pool Allocations | Allocating multi-megabyte payloads in `r->pool` during keepalive loops | High virtual/resident memory (RSS) per connection | Use `ngx_pfree()` on large allocations or sub-pools |
| Accessing `r->headers_in` Post Cleanup | Referencing request pointers after calling `ngx_http_finalize_request()` | Use-after-free vulnerability / SIGSEGV | Terminate handler logic immediately after finalize call |
| Neglecting `r->main` in Subrequests | Accessing variables or headers on subrequest instead of root request | Inconsistent authentication state and cache corruption | Inspect `r->main->headers_in` for inherited transaction context |

---

## 8. Performance, Memory & Security Implications

- **CPU Cache Alignment**: Structures like `ngx_cycle_t` and `ngx_connection_t` align fields to 64-byte L1 cache line boundaries (`NGX_ALIGNMENT`) to avoid false sharing across SMP CPU cores.
- **Non-reentrant Functions**: Standard NGINX core routines are deliberately single-threaded per worker. Thread safety is maintained through process isolation rather than mutex contention.
- **Zero Copy Buffering**: `ngx_buf_t` instances can reference files directly via `file_pos` and `file_last`, enabling direct Linux `sendfile(2)` invocation without copying file pages into user space.

---

## 9. Architectural Comparison: NGINX vs Envoy vs Node.js Source Layout

| Dimension | NGINX Core (`src/`) | Envoy Proxy (`source/`) | Node.js Core (`src/`) |
| :--- | :--- | :--- | :--- |
| **Language** | ANSI C (C89/C99 strictly structured) | Modern C++ (C++20, Abseil) | C++ (V8 bindings) + JavaScript |
| **Concurrency Model** | Multi-process, single thread per worker | Single-process, multi-threaded event loops | Single-threaded event loop (libuv) |
| **Memory Allocation** | Custom Region Pools (`ngx_pool_t`) | Standard smart pointers & arena allocators | V8 Garbage Collection + libuv buffers |
| **Multiplexing Layer** | Hand-rolled `ngx_epoll_module.c` | Libevent abstraction wrapper | `libuv` abstraction layer |
| **Configuration Model** | Single declarative syntax parsed into cycle | Dynamic xDS API (Protobuf/gRPC) | Scripted / JSON options |
| **Extensibility** | Static or dynamic C shared objects | C++ filters, WebAssembly (Wasm) | Node.js C++ Addons (N-API) |

---

## 10. Technical Interview Questions & Scenarios

<details>
<summary><strong>1. Explain how NGINX represents strings in C and why this improves performance over standard C-strings.</strong></summary>

NGINX represents strings via `ngx_str_t`:
```c
typedef struct {
    size_t      len;
    u_char     *data;
} ngx_str_t;
```
Standard C-strings rely on a terminating null character (`\0`), requiring $O(N)$ operations (`strlen()`) to determine lengths. In network proxying, strings are repeatedly sliced (sub-strings of HTTP headers, URLs, query parameters). With `ngx_str_t`, NGINX can create sub-slices in $O(1)$ time by pointing `data` to an existing buffer and setting `len`, without mutating the original memory or copying characters to add a null byte.
</details>

<details>
<summary><strong>2. What is the role of `ngx_pool_t`, and why does NGINX use pool allocators instead of direct `malloc`/`free` calls?</strong></summary>

`ngx_pool_t` provides arena-based memory management. Individual requests or connections allocate hundreds of small memory fragments (headers, URI parts, config lookups). Calling `malloc()` and `free()` for each fragment causes severe heap fragmentation and overhead. With arena pools:
1. Small allocations advance an internal pointer within pre-allocated 4KB blocks ($O(1)$ allocation).
2. Deallocation happens in a single bulk operation (`ngx_destroy_pool()`) when the request completes, eliminating memory leaks by design.
</details>

<details>
<summary><strong>3. How does the Master Process communicate configuration reloads to Worker processes at the syscall level?</strong></summary>

The master process uses standard UNIX signals. When an operator runs `nginx -s reload`:
1. The CLI utility reads `nginx.pid` and sends `SIGHUP` to the master process.
2. The master process re-parses and validates the configuration via `ngx_init_cycle()`.
3. If valid, the master forks new worker processes running the new cycle.
4. The master sends `SIGQUIT` (graceful shutdown) to the old worker processes.
5. Old workers close listening sockets, finish serving active connections, and exit cleanly.
</details>

<details>
<summary><strong>4. Where in the source code does NGINX transition from reading raw bytes to parsing HTTP request headers?</strong></summary>

In `src/http/ngx_http_request.c`, the connection read event invokes `ngx_http_wait_request_handler()` upon receiving initial socket bytes. It initializes the `ngx_http_request_t` structure and delegates to `ngx_http_process_request_line()`, which uses an optimized state machine (`ngx_http_parse_request_line()`) to extract the method, URI, and HTTP protocol version without backtracking.
</details>

<details>
<summary><strong>5. How does NGINX implement timer expiration without scanning all active connections every event loop iteration?</strong></summary>

NGINX stores timers in an augmented Red-Black Tree (`ngx_rbtree_t`), located at `src/event/ngx_event_timer.c`. Each timer key is the absolute expiration millisecond timestamp (`ngx_current_msec + timeout`). The event loop determines the maximum timeout for `epoll_wait()` by inspecting the minimum element (leftmost node) of the rbtree in $O(1)$ time. Expired timers are processed by traversing the left subtree until reaching a future timestamp.
</details>

<details>
<summary><strong>6. What is the significance of the `r->main` pointer in `ngx_http_request_t`?</strong></summary>

When NGINX executes internal redirects or subrequests (such as `auth_request`, `addition`, or `ssi`), it allocates child `ngx_http_request_t` structs. The `r->main` pointer always points to the original top-level request originating from the client. Global request attributes (like connection status, logging contexts, and response streaming states) are coordinated through `r->main`.
</details>

<details>
<summary><strong>7. How does NGINX manage shared memory across independent worker processes without thread locks?</strong></summary>

NGINX allocates shared memory segments in the master process using `mmap(MAP_SHARED|MAP_ANONYMOUS)` or `shmget()` prior to forking (`src/os/unix/ngx_shmem.c`). Because worker processes are created via `fork()`, they inherit identical virtual address mappings. Coordination across workers for shared dictionaries (e.g., rate limit zones, proxy caches) uses atomic operations and atomic spinlocks (`ngx_shmtx_t`) backed by compiler built-ins or architecture-specific assembly.
</details>

<details>
<summary><strong>8. What happens internally when `epoll_wait()` returns multiple ready events simultaneously?</strong></summary>

In `src/event/modules/ngx_epoll_module.c`, `epoll_wait()` populates an array of `struct epoll_event`. NGINX iterates through the events. If `accept_mutex` is enabled or events are flagged for deferred posting, events are queued into posted event queues (`ngx_posted_accept_events` and `ngx_posted_events`). Accept events are processed first to admit new connections, followed by processing queued I/O events, minimizing latency under high load.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Locate and Trace the HTTP Request State Machine</strong></summary>

**Goal**: Inspect the exact C code that parses HTTP headers.
1. Open `src/http/ngx_http_parse.c`.
2. Locate function `ngx_http_parse_header_line(ngx_http_request_t *r, ngx_buf_t *b, ngx_uint_t allow_underscores)`.
3. Identify how uppercase and lowercase characters are normalized into hash buckets.
4. Verify why underscores in header names trigger a syntax violation unless `underscores_in_headers on;` is set.
</details>

<details>
<summary><strong>Exercise 2: Write a GDB Script to Dump All Open Connections</strong></summary>

**Goal**: Create an automation script to inspect connection states in a live debug worker.
1. Create `dump_connections.gdb`:
```gdb
set pagination off
set $cycle = ngx_cycle
set $i = 0
while $i < $cycle->connection_n
    set $c = &$cycle->connections[$i]
    if $c->fd != -1
        printf "Index %d: FD=%d, Client=%s\n", $i, $c->fd, $c->addr_text.data
    end
    set $i = $i + 1
end
```
2. Run GDB in batch mode against a running debug instance to inspect socket allocations.
</details>

<details>
<summary><strong>Exercise 3: Trace Memory Allocations in `ngx_palloc`</strong></summary>

**Goal**: Understand when allocations trigger a new pool block vs going to `large`.
1. In `src/core/ngx_palloc.c`, set a breakpoint on `ngx_palloc_large`.
2. In another terminal, submit a POST request with a 16KB body.
3. Observe GDB triggering the breakpoint and inspect `size` and `pool->max`.
</details>

<details>
<summary><strong>Exercise 4: Verify Event Loop Timer Traversal</strong></summary>

**Goal**: Observe how `ngx_event_expire_timers()` clears expired keepalive connections.
1. Set a breakpoint on `ngx_event_expire_timers` in `src/event/ngx_event_timer.c`.
2. Configure `keepalive_timeout 5s;`.
3. Open a connection with `telnet 127.0.0.1 8080`, wait 6 seconds without sending bytes, and observe the timer callback triggering connection closure.
</details>

<details>
<summary><strong>Exercise 5: Inspect Upstream Buffer Chaining</strong></summary>

**Goal**: Follow how backend response chunks are assembled into `ngx_chain_t`.
1. Inspect `src/core/ngx_buf.h` to see `ngx_buf_t` and `ngx_chain_t`.
2. Set a breakpoint in `src/http/ngx_http_upstream.c` on `ngx_http_upstream_process_headers`.
3. Inspect how `u->buffer` is processed before streaming to downstream clients.
</details>

<details>
<summary><strong>Exercise 6: Add Custom Debug Logging to NGINX Core</strong></summary>

**Goal**: Modify the source code to print internal cycle information.
1. Edit `src/http/ngx_http_core_module.c`.
2. Inside `ngx_http_core_content_phase()`, insert:
```c
ngx_log_error(NGX_LOG_NOTICE, r->connection->log, 0,
              "*** CUSTOM DEBUG: Handling URI: %V ***", &r->uri);
```
3. Recompile with `make` and run `curl http://127.0.0.1:8080/test`.
4. Check `error.log` for the notice message.
</details>

---

## 12. Summary & Quick Reference

```
+-------------------------------------------------------------------------------+
| NGINX Source Code Quick Reference                                             |
|                                                                               |
| Type / Function            | Location           | Description                 |
| :------------------------- | :----------------- | :-------------------------- |
| ngx_str_t                  | src/core/ngx_string| Length-prefixed string      |
| ngx_pool_t                 | src/core/ngx_palloc| Arena memory pool allocator |
| ngx_cycle_t                | src/core/ngx_cycle | Global process cycle state  |
| ngx_connection_t           | src/core/connection| Socket descriptor wrapper   |
| ngx_http_request_t         | src/http/request   | HTTP transaction context    |
| ngx_init_cycle()           | src/core/ngx_cycle | Parses config & initializes |
| ngx_palloc(pool, size)     | src/core/ngx_palloc| Pool allocation             |
| ngx_destroy_pool(pool)     | src/core/ngx_palloc| Destroys pool and frees mem |
| ngx_http_finalize_request()| src/http/request   | Completes request phase     |
+-------------------------------------------------------------------------------+
```

---

## 13. Further Reading & Source Pointers

- **Official Source Repository**: [NGINX GitHub Mirror](https://github.com/nginx/nginx)
- **NGINX Development Guide**: [Official NGINX Internals Guide](https://nginx.org/en/docs/dev/development_guide.html)
- **Source Files to Read First**:
  - `src/core/ngx_cycle.h` (The heart of NGINX execution state)
  - `src/core/ngx_palloc.h` (Memory management fundamentals)
  - `src/event/ngx_event.h` (Event loop definitions)
  - `src/http/ngx_http_request.h` (HTTP transaction fields)
- **Next Concept**: [Module 05: Security Hardening & Performance Tuning](../05-security-performance/README.md)

---

[← Previous Note: 05-modules-and-dynamic-modules.md](05-modules-and-dynamic-modules.md) | [Back to Module Index](README.md) | [Next Module: 05-security-performance →](../05-security-performance/README.md)
