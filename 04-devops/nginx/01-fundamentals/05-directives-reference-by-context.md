<!--
================================================================================
Metadata:
  Title: NGINX Directives Reference by Context: Scope, Inheritance & Bitmasks
  Track: NGINX & Traffic Engineering / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 01-fundamentals/02-config-structure.md
  Key Terminology: ngx_command_t, Context Bitmasks, main, events, http, server, location, upstream, stream
================================================================================
-->

# NGINX Directives Reference by Context: Scope, Inheritance & Bitmasks

[<- 04-static-files.md](./04-static-files.md) | [Back to Module Overview](./README.md) | [06-command-line-and-signals.md ->](./06-command-line-and-signals.md)

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Troubleshooting](#7-common-mistakes--troubleshooting)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference Cheat Sheet](#12-summary--quick-reference-cheat-sheet)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

Every directive in NGINX possesses a strictly enforced **contextual boundary**. Attempting to declare a directive outside its designated context triggers a fatal parsing error during startup:
`nginx: [emerg] "upstream" directive is not allowed here in /etc/nginx/conf.d/app.conf:12`

To design clean, maintainable, and modular configurations across large enterprise infrastructures, engineers must know precisely where directives belong. Furthermore, understanding which directives **cascade downward via inheritance** versus which directives are **scoped strictly to their enclosing block** is essential to prevent unintended security leaks and proxy behavior anomalies.

---

## 2. Core Concepts

### Context Scopes in NGINX
The configuration engine recognizes several primary hierarchical scopes:
1. **`main`**: The global, top-level context (outside any block). Controls operating system process state, user credentials, PID files, and global logging.
2. **`events`**: Connection processing settings (worker connections, polling mechanisms).
3. **`http`**: The root of the Layer 7 web engine. Houses HTTP configurations, MIME types, caches, and defaults.
4. **`server`**: Represents a virtual host (IP, port, domain). Nested inside `http` (Layer 7) or `stream` (Layer 4).
5. **`location`**: Represents URI routing rules. Nested inside `server` (or recursively inside other `location` blocks).
6. **`upstream`**: Defines backend server clusters. Nested strictly inside `http` or `stream`.
7. **`stream`**: The root of the Layer 4 TCP/UDP raw packet proxy engine.

```
+--------------------------------------------------------------------------+
| NGINX CONTEXT TAXONOMY                                                   |
|                                                                          |
| [ main ]                                                                 |
|   ├── events { ... }                                                     |
|   ├── stream {                                                           |
|   │     ├── upstream { ... }                                             |
|   │     └── server { ... }                                               |
|   │   }                                                                  |
|   └── http {                                                             |
|         ├── upstream { ... }                                             |
|         ├── map { ... }                                                  |
|         └── server {                                                     |
|               └── location {                                             |
|                     └── location { ... } (nested)                        |
|                   }                                                      |
|             }                                                            |
|       }                                                                  |
+--------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### The C Module Directive Bitmask (`ngx_command_t`)
In the NGINX C source code, every directive is defined by a `struct ngx_command_s` (typedef `ngx_command_t`) inside its parent module. This struct contains a 32-bit integer bitmask (`type`) that encodes exactly which contexts are legally allowed to contain the directive:

```c
// Simplified excerpt from NGINX src/core/ngx_conf_file.h
#define NGX_MAIN_CONF        0x01000000  // main context
#define NGX_EVENT_CONF       0x02000000  // events context
#define NGX_HTTP_MAIN_CONF   0x04000000  // http context
#define NGX_HTTP_SRV_CONF    0x08000000  // server context inside http
#define NGX_HTTP_LOC_CONF    0x10000000  // location context inside http
#define NGX_HTTP_UPS_CONF    0x20000000  // upstream context inside http
#define NGX_DIRECT_CONF      0x00010000  // direct configuration

// Example directive definition for 'proxy_pass' in ngx_http_proxy_module.c:
static ngx_command_t ngx_http_proxy_commands[] = {
    { ngx_string("proxy_pass"),
      NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
      ngx_http_proxy_pass,
      ... },
    ngx_null_command
};
```

When the configuration parser encounters a directive token like `proxy_pass`, it checks:
$\text{Is Current Context Bit Set in Directive Type Bitmask?}$
If the current parsing context is `NGX_HTTP_SRV_CONF` (inside `server {}`), but `proxy_pass` only has `NGX_HTTP_LOC_CONF` set, the bitwise AND evaluates to zero. The parser emits `directive is not allowed here` and aborts compilation.

---

## 4. Syntax & Usage

### Comprehensive Context Compatibility Matrix

| Directive | Permitted Contexts | Default Value | Inheritance Behavior |
| :--- | :--- | :--- | :--- |
| `worker_processes` | `main` | `1` (recommend: `auto`) | None |
| `worker_connections`| `events` | `512` (recommend: `1024+`) | None |
| `sendfile` | `http`, `server`, `location` | `off` | Inherited / Overridable |
| `tcp_nopush` | `http`, `server`, `location` | `off` | Inherited / Overridable |
| `keepalive_timeout` | `http`, `server`, `location` | `75s` (recommend: `65s`) | Inherited / Overridable |
| `server_name` | `server` | `""` | Scoped to virtual host |
| `listen` | `server` | `*:80` or `*:8000` | Scoped to virtual host |
| `root` | `http`, `server`, `location` | `html` | Inherited / Overridable |
| `alias` | `location` strictly | None | Scoped strictly to location |
| `proxy_pass` | `location`, `if in location` | None | Scoped strictly to location |
| `proxy_set_header` | `http`, `server`, `location` | `Host $proxy_host; Connection close;` | Array replacement (no merge!) |
| `add_header` | `http`, `server`, `location` | None | Array replacement (no merge!) |
| `upstream` | `http`, `stream` | None | Global to protocol engine |
| `try_files` | `server`, `location` | None | Scoped to block |
| `return` | `server`, `location`, `if` | None | Executes immediately |
| `limit_req_zone` | `http` strictly | None | Global shared memory zone |
| `limit_req` | `http`, `server`, `location` | None | Inherited / Overridable |
| `proxy_cache_path` | `http` strictly | None | Global shared memory zone |
| `proxy_cache` | `http`, `server`, `location` | `off` | Inherited / Overridable |

---

## 5. Practical Examples

### Example 1: Full-Scope Multi-Context Skeleton
```nginx
# 1. Main Context
user nginx;
worker_processes auto;
pid /run/nginx.pid;
error_log /var/log/nginx/error.log warn;

# 2. Events Context
events {
    worker_connections 2048;
    use epoll;
}

# 3. HTTP Context (Layer 7)
http {
    include /etc/nginx/mime.types;
    default_type application/octet-stream;
    
    # HTTP-wide defaults
    sendfile on;
    keepalive_timeout 65;
    client_max_body_size 16M;

    # Shared memory definitions (must be in http context!)
    limit_req_zone $binary_remote_addr zone=api_limit:10m rate=10r/s;
    proxy_cache_path /var/cache/nginx levels=1:2 keys_zone=cache_zone:10m max_size=1g;

    # Upstream definition (inside http context)
    upstream backend_pool {
        server 10.0.1.10:8080;
        server 10.0.1.11:8080;
        keepalive 32;
    }

    # 4. Server Context
    server {
        listen 80;
        server_name api.example.com;

        # 5. Location Context
        location / {
            limit_req zone=api_limit burst=20 nodelay;
            proxy_pass http://backend_pool;
            proxy_set_header Host $host;
        }
    }
}

# 6. Stream Context (Layer 4 Raw TCP Proxy)
stream {
    upstream dns_servers {
        server 1.1.1.1:53;
        server 8.8.8.8:53;
    }

    server {
        listen 53 udp;
        proxy_pass dns_servers;
    }
}
```

### Example 2: Diagnosing Context Misplacement Errors
```bash
# Common Defect: Engineer places 'upstream' inside 'server'
cat << 'EOF' > /tmp/bad_context.conf
events {}
http {
    server {
        listen 80;
        # ERROR: upstream is illegal inside server!
        upstream my_cluster {
            server 127.0.0.1:8080;
        }
    }
}
EOF

# Run test
nginx -t -c /tmp/bad_context.conf
# Output:
# nginx: [emerg] "upstream" directive is not allowed here in /tmp/bad_context.conf:6
# nginx: configuration file /tmp/bad_context.conf test failed
```

### Example 3: Hierarchical Directive Overrides (`client_max_body_size`)
```nginx
http {
    # Default limit across the entire server fleet is 1 Megabyte
    client_max_body_size 1M;

    server {
        listen 80;
        server_name portal.example.com;

        # Inherits 1M for all standard routes
        location /api/ {
            proxy_pass http://api_backend;
        }

        # Override strictly for file upload endpoint to allow 50 Megabytes
        location /api/upload/ {
            client_max_body_size 50M;
            proxy_pass http://api_backend;
        }
    }
}
```

### Example 4: Enforcing Shared Memory Zones in Main vs HTTP
```nginx
# Directives that allocate kernel shared memory (shm) MUST reside in http {}
# Placing them in server {} fails because memory slabs are created at server boot,
# not during virtual host initialization.

http {
    # CORRECT: Defined at http root
    limit_conn_zone $binary_remote_addr zone=addr_conn:10m;

    server {
        listen 80;
        server_name app.internal;

        location / {
            # Referenced inside location
            limit_conn addr_conn 10;
        }
    }
}
```

### Example 5: Testing Deeply Nested Context Inheritances with `nginx -T`
```bash
# Verify how directives cascade to deeply nested locations
sudo nginx -T | grep -E "(client_max_body_size|proxy_read_timeout)"

# Shows exact resolved values at each server and location block.
```

---

## 6. Best Practices

- **Declare Global Security & Performance Defaults in `http {}`**: Place `sendfile on;`, `tcp_nopush on;`, `server_tokens off;`, and base timeouts in the `http {}` block so all virtual hosts inherit a hardened baseline.
- **Keep Shared Memory Zones Consolidated**: Place all `limit_req_zone`, `limit_conn_zone`, and `proxy_cache_path` directives at the very top of `nginx.conf` inside `http {}` (or in a dedicated `/etc/nginx/conf.d/00-zones.conf` file) so they are initialized before virtual hosts load.
- **Never Nest Location Blocks More Than Two Levels Deep**: While NGINX supports nesting (`location { location { ... } }`), deep nesting creates opaque directive inheritance bugs and makes regex execution difficult to trace.
- **Keep Upstreams Grouped Logically**: Maintain upstream server pool declarations either directly above the server blocks that use them or in a modular `/etc/nginx/conf.d/upstreams/` directory.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| `directive "upstream" is not allowed here` | `upstream` block placed inside `server {}` or `location {}`. | Move `upstream` out to the `http {}` or `stream {}` context. |
| `directive "proxy_pass" is not allowed here` | `proxy_pass` placed directly in `server {}` outside any `location {}`. | Enclose `proxy_pass` inside an explicit `location {}` block. |
| `directive "limit_req_zone" is not allowed here` | Placed inside `server {}` or `location {}`. Memory zones must be allocated globally. | Move `limit_req_zone` to the `http {}` context. |
| `directive "alias" is not allowed here` | `alias` placed in `server {}` or `http {}`. | `alias` is valid strictly inside `location {}`. Use `root` for global paths. |
| `directive "worker_processes" is not allowed here` | Placed inside `http {}` or `events {}`. | Move `worker_processes` to the top-level `main` context. |
| Placing `stream {}` inside `http {}` | `stream` (Layer 4) and `http` (Layer 7) are sibling contexts; they cannot be nested within each other. | Place `stream {}` at the root `main` context parallel to `http {}`. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Defining directives at parent levels (`http {}`) and allowing them to inherit minimizes memory duplication. Every time NGINX compiles a child context, it inherits parent struct pointers, keeping the compiled memory footprint of NGINX under 50 MB even with thousands of virtual hosts.
- **Security**: Understanding context scope prevents security header leakage. Relying on `add_header` at the `http {}` level without re-declaring it in child `location {}` blocks that set headers leaves sensitive endpoints completely unprotected by CSP, HSTS, or X-Frame-Options.
- **Cost**: Misplacing connection or timeout limits in the wrong context can lead to premature connection termination for long-polling clients or file uploads, causing customer-facing operational failures and unnecessary support escalations.

---

## 9. Comparison Table

| Directive Category | Allowed Contexts | Key Directives | Scope Impact |
| :--- | :--- | :--- | :--- |
| **Process & OS** | `main` strictly | `user`, `worker_processes`, `pid` | Entire OS process lifecycle |
| **Connection Polling**| `events` strictly | `worker_connections`, `use epoll` | Worker multiplexing engine |
| **Layer 7 Engine** | `http` | `include`, `default_type`, `sendfile` | All web traffic |
| **Layer 4 Engine** | `stream` | `proxy_pass`, `listen` | Raw TCP/UDP packet routing |
| **Virtual Host** | `server` | `listen`, `server_name` | Domain & port routing |
| **URI Path Handler** | `location` | `proxy_pass`, `root`, `alias`, `try_files` | Request path execution |
| **Upstream Pool** | `http`, `stream` | `server`, `keepalive`, `least_conn` | Backend load balancing |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. How does the NGINX C source code enforce where directives are allowed to exist?</strong></summary>

**Answer**:
Each directive is registered as an `ngx_command_t` struct containing a 32-bit bitmask field (`type`). Constants like `NGX_MAIN_CONF`, `NGX_HTTP_MAIN_CONF`, `NGX_HTTP_SRV_CONF`, and `NGX_HTTP_LOC_CONF` define valid contexts.

When the configuration parser runs, it tracks its current depth in the syntax tree (e.g., currently inside `NGX_HTTP_SRV_CONF`). When it encounters a directive name, it retrieves its `ngx_command_t` and performs a bitwise AND:
`if (!(cmd->type & cf->cmd_type)) { ... }`
If the result is zero, the directive is illegal in the current context, and the parser emits the fatal error: `directive "<name>" is not allowed here`.
</details>

<details>
<summary><strong>2. Why are shared memory zones (like `limit_req_zone` and `proxy_cache_path`) restricted strictly to the `http {}` context?</strong></summary>

**Answer**:
Shared memory zones (`shm`) are allocated by the master process during boot using the `mmap()` system call before worker processes are forked. These memory slabs are shared across all worker processes to maintain global lock-free state (e.g., rate limit request counts or cache key hash tables).

Because shared memory allocations are global resources tied to the HTTP engine's lifecycle, they cannot be scoped to individual virtual hosts (`server`) or URI paths (`location`), which may be dynamically created or modified. They must be declared globally in `http {}` so the master process knows the exact memory requirements to allocate upon startup.
</details>

<details>
<summary><strong>3. Can `stream {}` be nested inside `http {}`? What is the difference between them?</strong></summary>

**Answer**:
No. `stream {}` cannot be nested inside `http {}`. Both are top-level sibling contexts declared directly inside the `main` context.

- **`http {}`**: Operates at **Layer 7** (Application Layer). It understands HTTP request semantics: headers, cookies, URIs, query strings, and status codes.
- **`stream {}`**: Operates at **Layer 4** (Transport Layer). It acts as a raw TCP and UDP packet proxy. It does not parse HTTP headers or URLs; it forwards raw byte streams to backend servers (e.g., proxying MySQL, PostgreSQL, DNS, or raw TLS connections).
</details>

<details>
<summary><strong>4. What is the difference between how `root` and `alias` inherit down the configuration tree?</strong></summary>

**Answer**:
- `root` can be declared in `http`, `server`, and `location` contexts. It inherits downward naturally: defining `root /var/www/html;` at the `server` level applies to all locations within that server unless overridden.
- `alias` can **only** be declared inside a `location` context. It cannot be declared at the `server` or `http` level because `alias` requires an explicit location prefix to substitute in the URI path calculation. It does not inherit.
</details>

<details>
<summary><strong>5. Explain the execution order of `return`, `rewrite`, and `proxy_pass` when declared in the same location.</strong></summary>

**Answer**:
NGINX processes directives according to internal **processing phases**, not file order:
1. `rewrite` and `return` are part of the **Rewrite Phase** (`NGX_HTTP_REWRITE_PHASE`).
2. `proxy_pass` belongs to the **Content Phase** (`NGX_HTTP_CONTENT_PHASE`).

If a location contains both `return 301 ...;` and `proxy_pass http://backend;`:
The `return` directive executes during the Rewrite phase and terminates the request immediately by sending the 301 redirect. The Content phase is never reached, and `proxy_pass` is completely ignored.
</details>

<details>
<summary><strong>6. What is the effect of placing `server_tokens off;` in the `http {}` context versus the `server {}` context?</strong></summary>

**Answer**:
- In the **`http {}`** context: `server_tokens off;` applies globally to every virtual host and location across the entire NGINX instance. All HTTP responses omit the NGINX version number in the `Server:` response header and error pages.
- In a **`server {}`** context: It applies only to requests handled by that specific virtual host. Other server blocks will still advertise the NGINX version unless they also set `server_tokens off;`. Best practice is to set it in `http {}` to enforce system-wide security.
</details>

<details>
<summary><strong>7. Why cannot `listen` be declared in the `location` context?</strong></summary>

**Answer**:
The `listen` directive instructs the master process which IP addresses and TCP/UDP ports to bind to using the `bind()` and `listen()` kernel system calls.

Port and socket binding occurs at the operating system networking layer before any HTTP request data is transmitted. Because a request URI is only transmitted *after* a TCP connection is established and an HTTP request line is parsed, NGINX cannot route traffic to a location until after the socket has already accepted the packet. Therefore, `listen` is strictly a `server` context property.
</details>

<details>
<summary><strong>8. What is the inheritance behavior of `proxy_set_header` when declared in both `http` and `location`?</strong></summary>

**Answer**:
`proxy_set_header` is an array directive. Like `add_header`, it does **not merge** across context boundaries.

If `http` defines:
`proxy_set_header Host $host;`
`proxy_set_header X-Real-IP $remote_addr;`
And a child `location` defines only:
`proxy_set_header Authorization $http_authorization;`
The child location completely overrides the parent array. The upstream server will receive the `Authorization` header, but will **NOT** receive `Host` or `X-Real-IP`! To retain them, you must redefine all headers in the child location or use an included snippet.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Identify Invalid Directive Placements</strong></summary>

**Task**: Identify which of the following lines contain a syntax error due to illegal context placement:

```nginx
events {
    worker_processes 4;        # Line 1
}
http {
    upstream pool { ... }     # Line 2
    server {
        proxy_pass http://p;   # Line 3
        location / {
            alias /var/www/;   # Line 4
        }
    }
}
```

**Solution**:
- **Line 1 is invalid**: `worker_processes` belongs in the `main` context, not inside `events`.
- **Line 3 is invalid**: `proxy_pass` belongs inside `location`, not directly inside `server`.
- Lines 2 and 4 are valid.
</details>

<details>
<summary><strong>Exercise 2 (Easy): Relocate Upstream to Correct Context</strong></summary>

**Task**: Rewrite the configuration snippet below so that it passes `nginx -t`.

```nginx
server {
    listen 80;
    server_name app.local;
    upstream node_servers {
        server 127.0.0.1:3000;
    }
    location / {
        proxy_pass http://node_servers;
    }
}
```

**Solution**:
Move `upstream` outside the `server` block into the enclosing `http` scope:
```nginx
upstream node_servers {
    server 127.0.0.1:3000;
}

server {
    listen 80;
    server_name app.local;

    location / {
        proxy_pass http://node_servers;
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implement Scoped Upload Limits</strong></summary>

**Task**: Configure a server block where all requests default to a 2 MB upload limit, but requests to `/upload/` are permitted up to 100 MB.

**Solution**:
```nginx
server {
    listen 80;
    server_name files.local;

    # Server-wide default
    client_max_body_size 2M;

    location / {
        proxy_pass http://app_backend;
    }

    # Override for specific upload location
    location ^~ /upload/ {
        client_max_body_size 100M;
        proxy_pass http://app_backend;
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Configure Shared Memory Rate Limiting Across Virtual Hosts</strong></summary>

**Task**: Configure an HTTP-level rate limiting zone named `login_zone` allowing 5 requests per second, and apply it to `/login` across two separate server blocks (`app1.local` and `app2.local`).

**Solution**:
```nginx
http {
    # Allocated globally in http
    limit_req_zone $binary_remote_addr zone=login_zone:10m rate=5r/s;

    server {
        listen 80;
        server_name app1.local;
        location /login {
            limit_req zone=login_zone burst=10 nodelay;
            proxy_pass http://backend1;
        }
    }

    server {
        listen 80;
        server_name app2.local;
        location /login {
            limit_req zone=login_zone burst=10 nodelay;
            proxy_pass http://backend2;
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Resolve Multi-Level Proxy Header Inheritance</strong></summary>

**Task**: Author a standardized proxy header snippet `/etc/nginx/snippets/proxy-headers.conf` and use it inside a multi-tier location configuration to avoid header loss.

**Solution**:
```nginx
# 1. /etc/nginx/snippets/proxy-headers.conf
proxy_set_header Host $host;
proxy_set_header X-Real-IP $remote_addr;
proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
proxy_set_header X-Forwarded-Proto $scheme;

# 2. Virtual host using snippet
server {
    listen 80;
    server_name gateway.local;

    location / {
        include /etc/nginx/snippets/proxy-headers.conf;
        proxy_pass http://default_backend;
    }

    location /authenticated/ {
        include /etc/nginx/snippets/proxy-headers.conf;
        proxy_set_header X-Auth-Token "verified-token";
        proxy_pass http://secure_backend;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build Dual Layer 4 & Layer 7 Production Config</strong></summary>

**Task**: Construct an `nginx.conf` that manages both an SSL terminating web proxy on port 443 and a raw TCP proxy for an SMTP mail server on port 25 using appropriate contexts.

**Solution**:
```nginx
user nginx;
worker_processes auto;
error_log /var/log/nginx/error.log notice;
pid /run/nginx.pid;

events {
    worker_connections 2048;
}

# Layer 7 Web Traffic
http {
    include /etc/nginx/mime.types;
    server_tokens off;

    server {
        listen 443 ssl;
        server_name web.internal;
        ssl_certificate /etc/ssl/certs/web.crt;
        ssl_certificate_key /etc/ssl/private/web.key;

        location / {
            return 200 "HTTPS Web Portal Active\n";
        }
    }
}

# Layer 4 Raw TCP Stream Traffic
stream {
    upstream mail_servers {
        server 10.0.0.50:25;
        server 10.0.0.51:25;
    }

    server {
        listen 25;
        proxy_pass mail_servers;
        proxy_timeout 5m;
    }
}
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Context | Purpose | Illegal Directives (Common Errors) |
| :--- | :--- | :--- |
| **`main`** | Global process management | `server`, `location`, `proxy_pass` |
| **`events`**| Event loop / connection settings | `http`, `server_name`, `sendfile` |
| **`http`** | Global Layer 7 web settings | `worker_processes`, `proxy_pass` |
| **`stream`**| Global Layer 4 raw TCP/UDP settings | `http2`, `client_max_body_size` |
| **`server`**| Virtual host definition | `upstream`, `limit_req_zone`, `proxy_pass` |
| **`location`**| URI routing rules | `upstream`, `worker_connections`, `server_name` |
| **`upstream`**| Backend pool balancing | `location`, `listen`, `root` |

---

## 13. Further Reading & Official Documentation

- [Official NGINX Documentation: Alphabetical Index of Directives](https://nginx.org/en/docs/dirindex.html)
- [Official NGINX Documentation: Core Module Directives](https://nginx.org/en/docs/ngx_core_module.html)
- [Official NGINX Documentation: HTTP Core Module Directives](https://nginx.org/en/docs/http/ngx_http_core_module.html)
- [NGINX Development Guide: Configuration Architecture](https://nginx.org/en/docs/dev/development_guide.html#config)
- [CIS NGINX Benchmark: Directives Hardening Standards](https://www.cisecurity.org/benchmark/nginx)

---

[<- 04-static-files.md](./04-static-files.md) | [Back to Module Overview](./README.md) | [06-command-line-and-signals.md ->](./06-command-line-and-signals.md)
