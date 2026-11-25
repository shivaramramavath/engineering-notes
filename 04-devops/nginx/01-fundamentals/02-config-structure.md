<!--
================================================================================
Metadata:
  Title: NGINX Configuration Architecture: Contexts, Inheritance & Includes
  Track: NGINX & Traffic Engineering / DevOps
  Difficulty: Beginner to Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 01-fundamentals/01-installation.md
  Key Terminology: Contexts, Inheritance, Blocks, Directives, include, add_header Trap, AST, nginx -t, nginx -T
================================================================================
-->

# NGINX Configuration Architecture: Contexts, Inheritance & Includes

[<- 01-installation.md](./01-installation.md) | [Back to Module Overview](./README.md) | [03-server-and-location.md ->](./03-server-and-location.md)

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

NGINX does not use procedural, per-directory runtime configuration files like Apache HTTP Server's `.htaccess`. In Apache, the server checks the physical storage filesystem on *every single incoming HTTP request* to parse local `.htaccess` files, introducing severe disk I/O latency and CPU overhead.

NGINX enforces a **compiled, declarative configuration tree**. When NGINX starts or reloads, the master process parses `/etc/nginx/nginx.conf` and all included modular files into an in-memory Abstract Syntax Tree (AST), validates directive boundaries, and compiles them into static C memory structures. Subsequent incoming network requests are resolved purely in memory without disk lookups.

For DevOps engineers and SREs, mastering configuration structure is foundational:
- Understanding the **Context Hierarchy** prevents syntax errors where directives are placed in unauthorized scopes (e.g., placing an `upstream` block inside a `server` block).
- Grasping **Inheritance Rules** prevents dangerous operational surprises—most notably the infamous **`add_header` inheritance trap**, where defining a single header in a `location` silently strips all security headers defined at the `http` level.
- Leveraging `nginx -T` allows automated CI/CD linters to validate massive multi-file configurations before deploying changes to live edge clusters.

---

## 2. Core Concepts

### Directives: Simple vs. Block
NGINX configuration consists of two syntactic primitives:
1. **Simple Directive**: Consists of a name and parameters separated by spaces, terminated strictly by a semicolon (`;`).
   ```nginx
   worker_processes auto;
   sendfile on;
   ```
2. **Block Directive**: Has the same structure as a simple directive, but instead of a semicolon, it terminates with a set of curly braces (`{ ... }`) enclosing child directives. A block directive that encloses other directives is formally called a **Context**.
   ```nginx
   server {
       listen 80;
       server_name example.com;
   }
   ```

### The Standard Context Hierarchy
Directives are strictly scoped. Placing a directive outside its designated context causes `nginx -t` to fail with `directive is not allowed here`.

```
[ Main Context (Global Scope) ]
├── user, worker_processes, pid, error_log
├── events { ... }                  (Connection handling, worker_connections)
├── stream { ... }                  (Layer 4 TCP/UDP proxying & load balancing)
└── http { ... }                    (Layer 7 Web & HTTP traffic engine)
    ├── Upstream Blocks             (Backend server pools: upstream { ... })
    ├── Map Blocks                  (Variable transformation: map { ... })
    └── Server Blocks               (Virtual Hosts: server { ... })
        ├── listen, server_name
        └── Location Blocks         (URI routing: location { ... })
            ├── proxy_pass, root, try_files
            └── Nested Location Blocks (Sub-paths: location { ... })
```

### Directive Inheritance Rules
When a directive is defined in an outer parent context, its value cascades downward:
- **Default Inheritance**: Directives set in `http` apply to all `server` blocks; directives set in `server` apply to all `location` blocks.
- **Local Override**: Specifying the directive in a child context overrides the parent context for that scope.
- **Array Directives Trap (`add_header` & `proxy_set_header`)**: Array directives **DO NOT MERGE**. If a child block defines even *one* `add_header`, it completely ignores and overrides **all** `add_header` directives inherited from parent blocks!

---

## 3. How It Works Internally

### Configuration Parsing & Memory Compilation
When the NGINX master process boots or receives the `SIGHUP` reload signal (`nginx -s reload`), it does not alter active worker processes immediately:

```
+--------------------------------------------------------------------------+
| NGINX CONFIGURATION PARSING LIFECYCLE                                    |
|                                                                          |
| 1. Master reads /etc/nginx/nginx.conf                                    |
| 2. Recursive Tokenizer expands all 'include' globs                       |
| 3. Parser checks grammar and directive-to-context compatibility          |
| 4. Compiles merged settings into C struct: ngx_cycle_t                   |
|                                                                          |
|       +---------------------------------------------+                    |
|       | Did parsing or socket binding check fail?   |                    |
|       +---------------------------------------------+                    |
|             /                                 \                          |
|         YES/                                   \ NO                      |
|           v                                     v                        |
|  [ Abort Reload ]                      [ Fork New Workers ]              |
|  Log error to stderr;                  Master forks new workers          |
|  Old workers continue                  with new ngx_cycle_t struct.      |
|  serving traffic unaffected!           Sends SIGQUIT to old workers      |
|  ZERO DOWNTIME!                        to drain active connections.      |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### The `include` Directive
The `include` directive allows splitting monolithic files into clean, modular components:

```nginx
http {
    # Include standard MIME types mapping file extensions to Content-Type
    include /etc/nginx/mime.types;

    # Include modular virtual host configs
    include /etc/nginx/conf.d/*.conf;

    # Include specific fastcgi parameters
    include /etc/nginx/fastcgi_params;
}
```

### CLI Validation & Reload Commands
```bash
# Test configuration syntax and verify file includes
sudo nginx -t

# Test configuration and print complete resolved configuration to stdout
sudo nginx -T

# Test custom config file without modifying active server
sudo nginx -t -c /path/to/test-nginx.conf

# Gracefully reload configuration without dropping client connections
sudo nginx -s reload

# Gracefully stop NGINX (waits for active connections to complete)
sudo nginx -s quit

# Reopen log files (used during logrotate)
sudo nginx -s reopen
```

---

## 5. Practical Examples

### Example 1: Production Modular `nginx.conf` Layout
```nginx
# /etc/nginx/nginx.conf - Production Baseline
user nginx;
worker_processes auto;
pid /run/nginx.pid;
error_log /var/log/nginx/error.log notice;

# Dynamic module loading
include /etc/nginx/modules-enabled/*.conf;

events {
    worker_connections 2048;
    use epoll;
    multi_accept on;
}

http {
    # MIME Types
    include /etc/nginx/mime.types;
    default_type application/octet-stream;

    # Logging Formats
    log_format main '$remote_addr - $remote_user [$time_local] "$request" '
                    '$status $body_bytes_sent "$http_referer" '
                    '"$http_user_agent" "$http_x_forwarded_for"';

    access_log /var/log/nginx/access.log main;

    # Performance Primitives
    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;
    keepalive_timeout 65;
    types_hash_max_size 2048;

    # Security
    server_tokens off;

    # Modular Virtual Host Configurations
    include /etc/nginx/conf.d/*.conf;
}
```

### Example 2: Demonstrating and Fixing the `add_header` Inheritance Trap
```nginx
# VULNERABLE CONFIGURATION (Security Headers Silently Dropped):
http {
    # Global Security Headers
    add_header X-Frame-Options "DENY" always;
    add_header X-Content-Type-Options "nosniff" always;

    server {
        listen 80;
        server_name api.example.com;

        location /static/ {
            # FATAL MISTAKE: Adding this cache header causes NGINX to completely
            # DROP X-Frame-Options and X-Content-Type-Options on /static/!
            add_header Cache-Control "public, max-age=31536000";
            root /var/www/static;
        }
    }
}

# REMEDIATION PATTERN (Snippet Include):
# Create /etc/nginx/snippets/security-headers.conf
# add_header X-Frame-Options "DENY" always;
# add_header X-Content-Type-Options "nosniff" always;

# In your virtual host:
location /static/ {
    include /etc/nginx/snippets/security-headers.conf;
    add_header Cache-Control "public, max-age=31536000";
    root /var/www/static;
}
```

### Example 3: Modular Reverse Proxy Virtual Host (`conf.d/api.conf`)
```nginx
# /etc/nginx/conf.d/api.conf
upstream backend_nodes {
    server 10.0.1.10:8080 max_fails=3 fail_timeout=10s;
    server 10.0.1.11:8080 max_fails=3 fail_timeout=10s;
    keepalive 32;
}

server {
    listen 80;
    server_name api.internal;

    location / {
        proxy_pass http://backend_nodes;
        proxy_http_version 1.1;
        
        # Standard proxy headers
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_set_header Connection "";
    }
}
```

### Example 4: Verifying Full Configuration State with `nginx -T`
```bash
# In CI/CD pipelines, inspect the complete resolved tree to detect accidental overrides
sudo nginx -T | grep -A 5 "server_name api.internal"

# Output confirms exact compiled state:
# server_name api.internal;
# location / {
#     proxy_pass http://backend_nodes;
#     proxy_http_version 1.1;
#     proxy_set_header Host $host;
```

### Example 5: Layer 4 TCP Stream Configuration (`stream {}`)
```nginx
# /etc/nginx/nginx.conf (appended at main context)
# Layer 4 TCP proxy for an internal PostgreSQL cluster
stream {
    upstream postgres_read_pool {
        server db-replica-1.internal:5432;
        server db-replica-2.internal:5432;
    }

    server {
        listen 5433;
        proxy_pass postgres_read_pool;
        proxy_connect_timeout 2s;
        proxy_timeout 10m;
    }
}
```

---

## 6. Best Practices

- **Never Put `upstream` Inside `server`**: Upstream definitions describe backend pools and belong strictly in the `http {}` or `stream {}` context. Placing `upstream` inside `server` is a syntax error.
- **Standardize on `/etc/nginx/conf.d/*.conf`**: Avoid Debian-style `sites-available` / `sites-enabled` symlink sprawl in containerized architectures. In Docker and Kubernetes, drop-in `.conf` files in `/etc/nginx/conf.d/` are easier to mount and automate.
- **Use Snippets for Shared Configuration**: Instead of repeating identical SSL ciphers, CORS headers, or security headers across 50 virtual host files, create a `/etc/nginx/snippets/` directory and use `include /etc/nginx/snippets/ssl.conf;`.
- **Always Validate with `nginx -t` in CI/CD**: Run `nginx -t` in pull request validation pipelines before deploying configuration artifacts to staging or production.
- **Comment Complex Regular Expressions**: NGINX location matching and rewrite blocks can become obscure; always document the intent of regex capture groups and URI rewrites.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| Missing trailing semicolon on a directive line | Syntax parser cannot determine the end of the argument list; throws `unexpected end of file` or `unknown directive`. | Add `;` to the end of every simple directive line. |
| Global `add_header` security headers vanish on child locations | NGINX array directives do not inherit if the child block defines any `add_header` of its own. | Re-include security headers inside the child block via a shared snippet. |
| `include /etc/nginx/conf.d/*.conf` fails if no `.conf` files exist | Older NGINX versions would error out if glob matched zero files. | Ensure at least a dummy or default `.conf` file exists, or use exact filenames. |
| Putting directives in wrong context: `nginx: [emerg] "proxy_pass" directive is not allowed here` | `proxy_pass` was placed directly inside `server {}` instead of inside a `location {}` block. | Move the directive into the appropriate `location` context. |
| Editing `/etc/nginx/sites-available/app` but changes don't take effect | On Debian/Ubuntu systems, only files symlinked into `sites-enabled/` are read by default. | Create symlink: `sudo ln -s /etc/nginx/sites-available/app /etc/nginx/sites-enabled/`. |
| Reloading NGINX directly with `systemctl reload` without testing | If a syntax error is present, systemd reload logs an error, but older scripts might miss the failure. | Always run `sudo nginx -t && sudo nginx -s reload`. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: NGINX's in-memory AST compilation eliminates per-request disk reads. Unlike Apache, which incurs a disk check on every sub-directory for `.htaccess`, NGINX maintains constant $O(1)$ memory lookup times for simple location directives.
- **Security**: The `add_header` inheritance trap is a leading cause of security compliance failures. Teams believe they have applied HSTS and CSP globally, but asset locations with `Cache-Control` strip these protections, exposing users to clickjacking and MIME-sniffing.
- **Cost**: Flawless zero-downtime configuration reloads (`nginx -s reload`) eliminate the need to drain and terminate cloud virtual machines simply to update routing rules or upstream IP pools.

---

## 9. Comparison Table

| Context Level | Enclosing Context | Permitted Directives | Purpose |
| :--- | :--- | :--- | :--- |
| **`main`** | None (Root File) | `worker_processes`, `user`, `pid`, `error_log` | OS process management, worker concurrency |
| **`events`** | `main` | `worker_connections`, `use`, `multi_accept` | Network connection polling engine |
| **`http`** | `main` | `include`, `sendfile`, `gzip`, `default_type` | Global Layer 7 HTTP web settings |
| **`stream`** | `main` | `server`, `upstream`, `proxy_pass` | Layer 4 TCP/UDP raw packet proxying |
| **`server`** | `http` or `stream`| `listen`, `server_name`, `ssl_certificate` | Virtual host definition (IP/Port/Domain) |
| **`location`** | `server` or `location`| `root`, `alias`, `proxy_pass`, `try_files` | URI path routing and content delivery |
| **`upstream`** | `http` or `stream`| `server`, `keepalive`, `least_conn`, `hash` | Backend server pools and load balancing |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the "add_header inheritance trap" in NGINX. How does it occur and how do you resolve it?</strong></summary>

**Answer**:
In NGINX, directives generally inherit from parent contexts (e.g., from `http` down to `server`, and from `server` down to `location`). However, **array directives**—most notably `add_header` and `proxy_set_header`—follow a strict replacement rule:
*If a child context defines even a single `add_header` directive, it inherits NONE of the `add_header` directives from any parent context.*

For example, if you declare `add_header X-Frame-Options "DENY";` in the `http {}` block, and a child `location /images/` defines `add_header Cache-Control "max-age=3600";`, NGINX completely discards `X-Frame-Options` on requests to `/images/`.

*Resolution*: Create a standardized configuration snippet (e.g., `/etc/nginx/snippets/security-headers.conf`) containing all mandatory security headers. Whenever a child location needs to declare custom headers, include the snippet inside that child block.
</details>

<details>
<summary><strong>2. What is the difference between `nginx -t` and `nginx -T`? When would an engineer use `nginx -T`?</strong></summary>

**Answer**:
- `nginx -t`: Tests the configuration syntax of the root file and all included files. It reports whether the syntax is valid or prints line numbers for errors, but emits no configuration text.
- `nginx -T`: Tests the configuration syntax AND dumps the **entire combined configuration tree** to stdout, including all included files in order, complete with header comments showing which file each section originated from.

An engineer uses `nginx -T` for:
1. Debugging complex multi-file configurations where includes and wildcards (`conf.d/*.conf`) obscure precedence.
2. Capturing a complete point-in-time configuration snapshot for backup or post-mortem incident review.
3. Automated CI/CD linting, piping the output to tools like `grep` to verify security baselines across hundreds of virtual hosts.
</details>

<details>
<summary><strong>3. Can an `upstream` block be placed inside a `server` block? Why or why not?</strong></summary>

**Answer**:
No. An `upstream` block defines a pool of backend servers and is only permitted inside the `http {}` or `stream {}` context.

Placing an `upstream` inside a `server` block will cause NGINX to fail to start with the error `directive "upstream" is not allowed here`. This architectural constraint exists because upstream pools are designed to be shared across multiple virtual hosts and multiple locations, maintaining shared socket connections and state across the entire HTTP engine.
</details>

<details>
<summary><strong>4. How does `nginx -s reload` achieve zero-downtime configuration updates?</strong></summary>

**Answer**:
When you execute `nginx -s reload`, the master process:
1. Re-reads and parses the configuration file and all includes.
2. If syntax errors exist, it aborts the reload, logs the error, and leaves existing workers running undisturbed.
3. If syntax is valid, it opens any new listening sockets or shared memory zones.
4. It spawns a set of **new worker processes** running the new configuration.
5. It sends the `SIGQUIT` signal to the **old worker processes**.
6. The old workers stop accepting *new* incoming connections, finish servicing all currently active requests, and terminate cleanly once their active connections drop to zero.

Throughout this entire sequence, the listening socket remains open in the master process, ensuring zero dropped TCP connections.
</details>

<details>
<summary><strong>5. What is the difference between `conf.d/` and `sites-available/` / `sites-enabled/` directory layouts?</strong></summary>

**Answer**:
- `conf.d/`: The standard layout maintained by the official NGINX project and Red Hat/Fedora distros. Any file ending in `.conf` placed in `/etc/nginx/conf.d/` is directly read and activated via `include /etc/nginx/conf.d/*.conf;`.
- `sites-available/` & `sites-enabled/`: A Debian/Ubuntu convention borrowed from Apache. Configurations are authored in `sites-available/` and activated by creating symbolic links into `sites-enabled/`.

In modern containerized deployments (Docker/Kubernetes), the Debian symlink pattern is considered anti-pattern bloat. The official `conf.d/` direct-include structure is preferred because it is simpler, requires no symlink management, and simplifies immutable container packaging.
</details>

<details>
<summary><strong>6. What happens if two included files define the exact same `server_name` on the same listening port?</strong></summary>

**Answer**:
NGINX detects the collision during configuration parsing. It issues a warning during `nginx -t`:
`nginx: [warn] conflicting server name "example.com" on 0.0.0.0:80, ignored`

At runtime, NGINX resolves incoming requests using **first-match precedence**: the server block that was parsed first (based on include order or alphabetical order of globbed files) takes precedence and serves traffic. The conflicting secondary definition is completely ignored for that hostname.
</details>

<details>
<summary><strong>7. How does the `events {}` context affect server concurrency?</strong></summary>

**Answer**:
The `events {}` context dictates how NGINX handles low-level network connections at the OS kernel level. The most critical directive is `worker_connections`, which specifies the maximum number of simultaneous open connections a single worker process can maintain.

Total server concurrency is roughly:
$\text{Max Connections} = \text{worker\_processes} \times \text{worker\_connections}$
In reverse proxy configurations, because NGINX opens two connections per client request (one to the client, one to the upstream), actual concurrent client capacity is halved:
$\text{Max Clients} = \frac{\text{worker\_processes} \times \text{worker\_connections}}{2}$
The `events` block also specifies the polling mechanism (e.g., `use epoll;` on Linux).
</details>

<details>
<summary><strong>8. What is the purpose of the `default_type` directive in the `http {}` context?</strong></summary>

**Answer**:
`default_type` defines the fallback MIME type assigned to response payloads when the requested file extension does not match any entry in `/etc/nginx/mime.types`.

The standard setting is:
`default_type application/octet-stream;`
This instructs client browsers to treat unknown file types as raw binary streams and download them, rather than attempting to render them as HTML or plain text, which could lead to cross-site scripting (XSS) vulnerabilities if untrusted files are uploaded.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Validate Syntax and Dump Configuration</strong></summary>

**Task**: Run the commands to test configuration syntax and then dump the entire resolved configuration tree to a text file in `/tmp/nginx_dump.txt`.

**Solution**:
```bash
sudo nginx -t
sudo nginx -T > /tmp/nginx_dump.txt
head -n 25 /tmp/nginx_dump.txt
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Identify Directive Context Scope</strong></summary>

**Task**: State which context (`main`, `events`, `http`, `server`, or `location`) the following directives belong in:
1. `worker_connections`
2. `proxy_pass`
3. `server_tokens`
4. `server_name`

**Solution**:
1. `worker_connections` -> `events` context.
2. `proxy_pass` -> `location` context (or `limit_except`).
3. `server_tokens` -> `http`, `server`, or `location` context.
4. `server_name` -> `server` context.
</details>

<details>
<summary><strong>Exercise 3 (Medium): Audit and Fix `add_header` Security Leak</strong></summary>

**Task**: Create a modular snippet `/tmp/security-headers.conf` containing `X-Frame-Options "DENY"` and include it inside a custom server block with a static asset location that sets `Cache-Control`.

**Solution**:
```nginx
# 1. Author snippet
# /tmp/security-headers.conf
add_header X-Frame-Options "DENY" always;
add_header X-Content-Type-Options "nosniff" always;

# 2. Virtual host referencing snippet
server {
    listen 8080;
    server_name test.local;

    location /assets/ {
        include /tmp/security-headers.conf;
        add_header Cache-Control "public, max-age=86400";
        return 200 "Static assets with security headers\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Verify Zero-Downtime Reload Under Load</strong></summary>

**Task**: Write a bash script that launches a continuous loop of `curl` requests while executing `nginx -s reload` to confirm zero dropped requests.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

echo "Starting client request stream..."
for i in {1..50}; do
    STATUS=$(curl -o /dev/null -s -w "%{http_code}" http://localhost/)
    if [ "$STATUS" -ne 200 ]; then
        echo "FAILED: Request $i returned status $STATUS"
    fi
    if [ "$i" -eq 25 ]; then
        echo ">>> Triggering NGINX reload at request 25..."
        sudo nginx -s reload
    fi
    sleep 0.05
done
echo "All 50 requests completed successfully during reload!"
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Script Automated CI/CD Configuration Linter</strong></summary>

**Task**: Author a bash validation script for a CI/CD pipeline that checks syntax using `nginx -t` on a custom directory, parses `nginx -T` to verify `server_tokens off;` is enabled globally, and fails the build if missing.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

echo "Running CI NGINX validation..."
if ! nginx -t -c "$PWD/nginx.conf"; then
    echo "ERROR: NGINX syntax validation failed!" >&2
    exit 1
fi

COMPILED_CONF=$(nginx -T -c "$PWD/nginx.conf" 2>/dev/null)

if ! echo "$COMPILED_CONF" | grep -q "server_tokens off;"; then
    echo "SECURITY AUDIT FAILED: 'server_tokens off;' is missing in configuration!" >&2
    exit 1
fi

echo "CI Validation Passed: Syntax OK and server_tokens disabled."
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Construct Layer 4 TCP & Layer 7 HTTP Dual-Stack Configuration</strong></summary>

**Task**: Author a complete `nginx.conf` that manages both an HTTP web reverse proxy on port 80 and a raw TCP proxy for Redis on port 6380 using the `stream {}` context.

**Solution**:
```nginx
user nginx;
worker_processes auto;
pid /run/nginx.pid;

events {
    worker_connections 1024;
}

# Layer 7 Web Traffic
http {
    include /etc/nginx/mime.types;
    server_tokens off;

    server {
        listen 80;
        server_name web.internal;
        location / {
            return 200 "HTTP Web Service Active\n";
        }
    }
}

# Layer 4 TCP Stream Traffic
stream {
    upstream redis_backend {
        server 127.0.0.1:6379;
    }

    server {
        listen 6380;
        proxy_pass redis_backend;
        proxy_timeout 1h;
    }
}
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command / Context | Syntax Example | Rule / Invariant |
| :--- | :--- | :--- |
| **Validate Config** | `sudo nginx -t` | Exits 0 if syntax valid, 1 on error |
| **Dump Config** | `sudo nginx -T` | Dumps entire compiled include tree |
| **Hot Reload** | `sudo nginx -s reload` | Zero dropped connections |
| **Main Context** | `worker_processes auto;` | Top-level process configuration |
| **Events Context**| `events { worker_connections 1024; }` | Network connection polling engine |
| **HTTP Context** | `http { ... }` | Encloses all Layer 7 web servers |
| **Stream Context**| `stream { ... }` | Encloses all Layer 4 TCP/UDP proxies |
| **Server Context**| `server { listen 80; }` | Virtual host definition |
| **Location Scope**| `location /path { ... }` | URI routing and handler binding |

---

## 13. Further Reading & Official Documentation

- [Official NGINX Documentation: Configuration File Structure](https://nginx.org/en/docs/beginners_guide.html#conf_structure)
- [NGINX Core Module Directives Reference](https://nginx.org/en/docs/ngx_core_module.html)
- [Official NGINX: Understanding the NGINX Configuration Inheritance](https://www.nginx.com/resources/wiki/start/topics/tutorials/config_pitfalls/#inheritance-and-directives)
- [Debian NGINX Packaging Guidelines](https://wiki.debian.org/Nginx/DirectoryStructure)
- [CIS NGINX Benchmark: Configuration File Integrity](https://www.cisecurity.org/benchmark/nginx)

---

[<- 01-installation.md](./01-installation.md) | [Back to Module Overview](./README.md) | [03-server-and-location.md ->](./03-server-and-location.md)
