# Module 01: NGINX Core Fundamentals

[<- Back to Track Root](../README.md) | [Next Module: 02-core-features ->](../02-core-features/README.md)

---

## 1. Overview & Scope

Module 01 establishes the foundational engineering principles of the NGINX web server, reverse proxy, and edge traffic router. NGINX is not merely a static asset file server; it is a high-performance, asynchronous, non-blocking network engine powering modern microservice gateways and ingress points.

This module dispels common abstractions and builds mastery from package installation and compilation flags up through the compiled configuration Abstract Syntax Tree (AST), context inheritance hierarchies, location matching priority rules, static content zero-copy acceleration, Unix process signals, and host-header security hardening.

---

## 2. Core Architecture & Mental Model

```
+--------------------------------------------------------------------------+
| NGINX CORE ARCHITECTURAL HIERARCHY                                       |
|                                                                          |
| [ Main Context (Global Scope) ]                                          |
| ├── Master Process (PID 1, root) -> Sockets, Signal Handler (HUP, USR2)  |
| ├── Events Context -> Network multiplexing engine (worker_connections)   |
| └── HTTP Context (Layer 7 Engine)                                        |
|     ├── Global Defaults: sendfile, mime.types, keepalive, server_tokens  |
|     ├── Upstream Blocks -> Backend server cluster declarations           |
|     └── Server Contexts (Virtual Hosts)                                  |
|         ├── listen, server_name resolution (exact, wildcard, regex)      |
|         └── Location Contexts (URI Path Matching)                        |
|             ├── Tier 1: Exact Match (=)                                  |
|             ├── Tier 2: Preferential Prefix (^~)                         |
|             ├── Tier 3: Regular Expressions (~ and ~*)                   |
|             └── Tier 4: Longest Prefix Fallback                          |
+--------------------------------------------------------------------------+
```

---

## 3. Module Notes Catalog

| # | Note | Level | Time | Key Concepts & Topics |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [NGINX Installation: Repositories, Docker & Source](./01-installation.md) | Beginner | 45m | Mainline vs Stable branches, official deb/rpm repos, PGP verification, source compilation flags, Docker read-only containers |
| **02** | [Configuration Architecture: Contexts & Inheritance](./02-config-structure.md) | Beginner-Int | 50m | Context hierarchy (`main`, `events`, `http`, `server`, `location`), AST compilation, `include` wildcards, `add_header` inheritance trap |
| **03** | [Server Blocks, Location Modifiers & URI Priority](./03-server-and-location.md) | Intermediate | 55m | `server_name` resolution, the 4-tier location algorithm (`=`, `^~`, `~*`, prefix), regex hijacking defense, named locations, `try_files` |
| **04** | [Static Content Delivery: root vs. alias & Caching](./04-static-files.md) | Beginner-Int | 50m | `root` vs `alias` mechanics, off-by-slash path traversal, kernel `sendfile` zero-copy, `open_file_cache`, SPA client-side routing |
| **05** | [Directives Reference by Context & Bitmasks](./05-directives-reference-by-context.md) | Intermediate | 50m | `ngx_command_t` bitmasks, legal scopes, shared memory allocation rules (`limit_req_zone`), stream vs http separation |
| **06** | [Command Line & Unix Signals: Lifecycle & Upgrades](./06-command-line-and-signals.md) | Intermediate | 50m | Master/Worker IPC, hot reloads (`SIGHUP`), live binary upgrades (`SIGUSR2`/`SIGWINCH`), log rotation (`SIGUSR1`), `daemon off;` |
| **07** | [Default Server, Catch-All Hosts & Host Attacks](./07-default-server-and-catch-all-hosts.md) | Intermediate | 50m | `default_server` flag, connection dropping (`return 444`), SNI fallthrough protection, dummy SSL certs, Host header injection |

---

## 4. Key Milestones & Learning Objectives

By completing Module 01, you will be able to:
1. **Differentiate Release Streams**: Select and deploy between Mainline (`1.31.x`) and Stable (`1.30.x`) branches based on protocol requirements (HTTP/3) and organizational stability policies.
2. **Eliminate Directive Inheritance Bugs**: Avoid the `add_header` trap by utilizing modular configuration snippets across virtual hosts.
3. **Master URI Routing Resolution**: Predictably order location blocks using `=`, `^~`, and regex modifiers, neutralizing authorization bypasses and regex hijacking vulnerabilities.
4. **Harden Static Hosting**: Deliver web assets with kernel zero-copy DMA (`sendfile`) while mathematically eliminating off-by-slash path traversal vulnerabilities.
5. **Execute Zero-Downtime Operations**: Perform live configuration reloads and in-memory binary upgrades using POSIX signals without dropping a single active TCP connection.
6. **Quarantine Scanners**: Construct an explicit `default_server` catch-all on ports 80 and 443 that terminates unrecognized requests with `return 444;`.

---

[<- Back to Track Root](../README.md) | [Next Module: 02-core-features ->](../02-core-features/README.md)
