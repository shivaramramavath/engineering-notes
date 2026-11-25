---
title: "NGINX Modules and Dynamic Modules Architecture"
description: "Deep dive into NGINX module architecture: ngx_module_t structure, static compilation vs DSO dynamic modules, binary ABI compatibility, and compiling third-party modules."
module: "04-internals"
track: "devops-nginx"
order: 5
difficulty: "Advanced"
prerequisites:
  - "01-fundamentals/01-installation"
  - "04-internals/01-architecture-master-worker-and-event-loop"
  - "04-internals/02-request-lifecycle-and-phases"
tags:
  - nginx
  - dynamic-modules
  - ngx-module
  - c-extension
  - compilation
  - dso
  - abi-compatibility
last_updated: "2026-03-30"
---

# NGINX Modules and Dynamic Modules Architecture

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. The C Module Architecture Dissected](#4-the-c-module-architecture-dissected)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: Loading Official Dynamic Modules via `load_module`](#example-1-loading-official-dynamic-modules-via-load_module)
  - [Example 2: Compiling a Third-Party Dynamic Module (`headers-more`)](#example-2-compiling-a-third-party-dynamic-module-headers-more)
  - [Example 3: Auditing Active Compiled Modules with `nginx -V`](#example-3-auditing-active-compiled-modules-with-nginx--v)
  - [Example 4: Building the Google Brotli Dynamic Module](#example-4-building-the-google-brotli-dynamic-module)
  - [Example 5: Diagnosing ABI Binary Incompatibility Errors](#example-5-diagnosing-abi-binary-incompatibility-errors)
- [6. Production Best Practices](#6-production-best-practices)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Troubleshooting](#8-performance-security--troubleshooting)
- [9. Architectural Comparison Table](#9-architectural-comparison-table)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

In NGINX, there is virtually no monolithic core: **almost every feature in NGINX is implemented as an independent module**. 
- Even basic HTTP operations (serving static files, rewriting URIs, parsing HTTP headers, upstream load balancing) are handled by separate internal modules (`ngx_http_static_module`, `ngx_http_rewrite_module`, `ngx_http_upstream_module`).

Historically, NGINX required all modules to be compiled **statically** into the main binary at build time. If you needed to add a new module (such as Brotli compression, GeoIP2 lookups, or Lua/njs), you had to download the NGINX source code, configure the build parameters, recompile the entire binary from scratch, and replace the executable.

Starting with **NGINX 1.9.11+**, NGINX introduced **Dynamic Modules (Dynamic Shared Objects / DSO)**. Modules can now be compiled independently as shared object libraries (`.so`) and dynamically loaded into the NGINX master process at runtime using the `load_module` directive. This enables lightweight standard binaries, fast security patching, and modular enterprise package management.

---

## 2. Core Concepts

### Module Classification in NGINX
NGINX organizes modules into distinct functional classes:
- **Core Modules**: Implement fundamental operating system bindings, process management, and logging (`ngx_core_module`, `ngx_errlog_module`).
- **Event Modules**: Implement OS event-notification loops (`ngx_events_module`, `ngx_epoll_module`, `ngx_kqueue_module`).
- **HTTP Modules**: Process Layer 7 HTTP requests, routing, and filters (`ngx_http_core_module`, `ngx_http_ssl_module`, `ngx_http_proxy_module`).
- **Stream Modules**: Process Layer 4 TCP and UDP proxying and load balancing (`ngx_stream_core_module`, `ngx_stream_ssl_module`).
- **Mail Modules**: Reverse proxy standard email protocols (POP3, IMAP, SMTP).

### Static vs Dynamic Modules
- **Static Modules**: Compiled directly into the monolithic `nginx` binary. They cannot be enabled, disabled, or updated without recompiling the NGINX executable.
- **Dynamic Modules (`.so`)**: Compiled as shared libraries using the `--add-dynamic-module` or `--with-compat` flags. They reside in `/etc/nginx/modules/` (or `/usr/lib/nginx/modules/`) and are loaded explicitly at the very top of `nginx.conf`:
  ```nginx
  load_module modules/ngx_http_brotli_filter_module.so;
  ```

### The Binary Compatibility Rule (ABI)
Because C does not have a formal standardized Application Binary Interface (ABI), dynamic modules **must be compiled against the exact same NGINX version, compiler definitions, and configuration flags** as the target NGINX binary. 
If NGINX was compiled with `--with-debug` or a custom `--with-pcre`, a dynamic module compiled without those flags will fail to load, reporting an ABI signature mismatch. To simplify this, modern NGINX provides the **`--with-compat`** compile flag to maintain binary compatibility across standard module builds.

---

## 3. How It Works Internally

### Dynamic Module Loading Pipeline via `dlopen()`

```
Master Process Startup (nginx boot or SIGHUP reload)
                       |
                       v
            Parse Top of nginx.conf
                       |
     Encounter: load_module modules/my_mod.so;
                       |
                       v
         Execute POSIX dlopen() system call
                       |
                       v
          Locate Global Symbol via dlsym():
               "ngx_http_my_mod_module"
                       |
                       v
        Verify ABI Signature Compatibility:
        - NGINX Version Number match?
        - Core configure signature match?
                  /         \
              MATCH        MISMATCH
               /               \
              v                 v
   Insert module pointer     Log [emerg]: module is not binary
   into ngx_modules[] array  compatible; ABORT STARTUP!
              |
              v
   Execute module preconfiguration & init hooks
              |
              v
   Advance to parsing http {} block
```

---

## 4. The C Module Architecture Dissected

Every NGINX module is defined by a primary C structure: **`ngx_module_t`**.

```c
// Simplified representation from src/core/ngx_conf_file.h
struct ngx_module_s {
    ngx_uint_t            ctx_index;       // Module index within its category
    ngx_uint_t            index;           // Global index in ngx_modules[]
    
    char                 *name;            // Module string name
    
    ngx_uint_t            spare0;
    ngx_uint_t            spare1;
    
    ngx_uint_t            version;         // NGINX binary version
    const char           *signature;       // ABI compatibility string hash
    
    void                 *ctx;             // Module Context (Function hooks)
    ngx_command_t        *commands;        // Array of directive definitions
    ngx_uint_t            type;            // NGX_HTTP_MODULE, NGX_CORE_MODULE, etc.
    
    // Lifecycle Callback Function Pointers
    ngx_int_t           (*init_master)(ngx_log_t *log);
    ngx_int_t           (*init_module)(ngx_cycle_t *cycle);
    ngx_int_t           (*init_process)(ngx_cycle_t *cycle);
    ngx_int_t           (*init_thread)(ngx_cycle_t *cycle);
    void                (*exit_thread)(ngx_cycle_t *cycle);
    void                (*exit_process)(ngx_cycle_t *cycle);
    void                (*exit_master)(ngx_cycle_t *cycle);
};
```

### The Directive Command Table (`ngx_command_t`)
Directives handled by the module are declared in a null-terminated array of `ngx_command_t`:
- **`name`**: The directive string as written in `nginx.conf` (e.g., `"proxy_pass"`).
- **`type`**: Flags defining valid configuration contexts (`NGX_HTTP_LOC_CONF`, `NGX_HTTP_SRV_CONF`, etc.) and parameter counts (`NGX_CONF_TAKE1`, `NGX_CONF_FLAG`).
- **`set`**: Function pointer called by the parser to tokenize and validate the arguments.
- **`conf`**: Offset within the module's configuration struct where the parsed value is stored.

---

## 5. Practical Implementation Examples

### Example 1: Loading Official Dynamic Modules via `load_module`

Configuring official Linux package modules (GeoIP2, njs, Image Filter, XSLT) at the top of `nginx.conf`.

```nginx
# /etc/nginx/nginx.conf

# NOTE: load_module directives MUST be placed in the main context
# BEFORE the events {} and http {} blocks!
load_module modules/ngx_http_geoip2_module.so;
load_module modules/ngx_http_js_module.so;
load_module modules/ngx_http_image_filter_module.so;

user nginx;
worker_processes auto;

events {
    worker_connections 1024;
}

http {
    include /etc/nginx/mime.types;
    
    server {
        listen 80;
        server_name example.com;
        location / {
            return 200 "Dynamic modules loaded successfully!\n";
        }
    }
}
```

### Example 2: Compiling a Third-Party Dynamic Module (`headers-more`)

A complete production workflow compiling the popular OpenResty `headers-more-nginx-module` as a dynamic module matching an installed NGINX binary.

```bash
# Step 1: Query exact version and build flags of the installed NGINX
NGINX_VERSION=$(nginx -v 2>&1 | awk -F'/' '{print $2}')
CONFIGURE_ARGS=$(nginx -V 2>&1 | grep "configure arguments:" | sed 's/configure arguments: //')

echo "Compiling for NGINX version: $NGINX_VERSION"

# Step 2: Download NGINX source matching exact version
curl -O https://nginx.org/download/nginx-${NGINX_VERSION}.tar.gz
tar -zxvf nginx-${NGINX_VERSION}.tar.gz

# Step 3: Clone the third-party module source
git clone https://github.com/openresty/headers-more-nginx-module.git

# Step 4: Configure using original arguments + --add-dynamic-module
cd nginx-${NGINX_VERSION}
./configure $CONFIGURE_ARGS --add-dynamic-module=../headers-more-nginx-module

# Step 5: Build modules ONLY (Do not run 'make install'!)
make modules

# Step 6: Copy compiled .so to NGINX modules directory
mkdir -p /etc/nginx/modules
cp objs/ngx_http_headers_more_filter_module.so /etc/nginx/modules/

# Step 7: Load module in /etc/nginx/nginx.conf
# load_module /etc/nginx/modules/ngx_http_headers_more_filter_module.so;
```

### Example 3: Auditing Active Compiled Modules with `nginx -V`

Dumping compile-time configuration to inspect built-in modules, compiler options, and dynamic module support.

```bash
# Inspect compilation arguments
nginx -V

# Filter for third-party dynamic modules
nginx -V 2>&1 | grep -o "\-\-add\-dynamic\-module=[^ ]*"

# Check if NGINX was compiled with binary compatibility flag
nginx -V 2>&1 | grep -o "\-\-with\-compat"
```

### Example 4: Building the Google Brotli Dynamic Module

Building `ngx_brotli` with its embedded C libraries for modern web compression.

```bash
# 1. Install build prerequisites
apt-get install -y build-essential libpcre3-dev zlib1g-dev libssl-dev git cmake

# 2. Clone ngx_brotli with submodules (Brotli C encoder/decoder)
git clone --recurse-submodules https://github.com/google/ngx_brotli.git

# 3. Configure and compile modules against matching NGINX source
cd nginx-1.26.1
./configure --with-compat --add-dynamic-module=../ngx_brotli
make modules

# 4. Copy both filter and static modules to modules path
cp objs/ngx_http_brotli_filter_module.so /usr/lib/nginx/modules/
cp objs/ngx_http_brotli_static_module.so /usr/lib/nginx/modules/
```

### Example 5: Diagnosing ABI Binary Incompatibility Errors

What happens when an incompatible module is loaded, and how to resolve it.

```nginx
# If a module compiled for NGINX 1.24 is loaded into NGINX 1.26:
# nginx -t fails with:
# nginx: [emerg] module "/etc/nginx/modules/old_mod.so" is not binary compatible in /etc/nginx/nginx.conf:1
```

#### Diagnostic Script to Inspect ABI Compatibility Signature:
```bash
# Inspect the signature string embedded in the .so binary using 'strings':
strings /etc/nginx/modules/old_mod.so | grep -E "^[0-9]{7},"
# The signature encodes version, compiler options, and pointer sizes.
# Compare this against the NGINX binary signature:
strings /usr/sbin/nginx | grep -E "^[0-9]{7},"
# If signatures differ by even one character, NGINX rejects loading the module!
```

---

## 6. Production Best Practices

1. **Always Place `load_module` at the Very Top of `nginx.conf`**: The `load_module` directive is strictly valid only in the `main` context. Placing it inside `http {}` or `events {}` triggers a syntax failure.
2. **Always Use `--with-compat`**: When compiling custom dynamic modules, always pass `--with-compat` to `./configure`. This ensures maximum ABI binary compatibility with standard Linux distribution packages.
3. **Never Run `make install` When Building Modules**: When compiling custom modules for a running system, only run `make modules`. Running `make install` will overwrite your production NGINX binary and configuration directories!
4. **Pin Dynamic Modules to Specific NGINX Package Releases**: When using automated package updates (`apt-get upgrade`), an update to NGINX without rebuilding custom third-party dynamic modules will break NGINX reloads due to ABI version mismatches. Use `apt-mark hold nginx` in production.
5. **Store Modules in Standard Paths**: Store modules in `/usr/lib/nginx/modules/` or create a symbolic link from `/etc/nginx/modules` to ensure standard directory consistency.
6. **Audit Dynamic Modules with `nginx -t` in CI/CD**: Always run `nginx -t` after downloading or compiling new modules to ensure `dlopen()` succeeds before signaling production workers.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **`load_module` inside `http {}` context** | `load_module` is a core `main` context directive; placing it in `http` causes: `[emerg] "load_module" directive is not allowed here`. | Declare `load_module` at the very top of `nginx.conf`. |
| **Module ABI mismatch error on package upgrade** | System packages upgrade NGINX from 1.24 to 1.26; third-party `.so` compiled for 1.24 fails ABI check, halting NGINX on next reload. | Lock NGINX package version (`apt-mark hold`), or rebuild dynamic modules during upgrade pipelines. |
| **Running `make install` during module build** | Overwrites running `/usr/sbin/nginx` binary with un-tuned build binary and default configs. | Only execute `make modules` and manually copy the resulting `.so` file. |
| **Compiling module without submodules** | Modules like `ngx_brotli` rely on Git submodules; building without `--recurse-submodules` causes missing C header compilation errors. | Clone modules with `git clone --recurse-submodules`. |
| **Loading the same module twice** | Declaring `load_module` for the same `.so` in two included files triggers: `[emerg] module "..." is already loaded`. | Consolidate all `load_module` statements into `nginx.conf` or a single `modules.conf`. |
| **Missing shared library dependencies on host** | The `.so` was compiled against `libmaxminddb.so`, but the target production host lacks the shared library; `dlopen()` fails with `cannot open shared object file`. | Install underlying C runtime libraries (`libmaxminddb0`, `libbrotli1`) on production hosts. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Test Dynamic Module Loading
nginx -t

# 2. Check Shared Library Dependencies of a Compiled Module via ldd
ldd /usr/lib/nginx/modules/ngx_http_geoip2_module.so
# Verify that all dependencies are resolved (no "not found" lines)

# 3. Check Which Dynamic Modules are Currently Loaded by Running NGINX
lsof -p $(pgrep -f "nginx: master") | grep "\.so"

# 4. Strip Unneeded Symbols from Compiled Module to Reduce Binary Size
strip --strip-unneeded /usr/lib/nginx/modules/ngx_http_brotli_filter_module.so
```

### Performance Impact of Dynamic Modules
- **Zero Runtime Overhead**: Once loaded via `dlopen()`, an NGINX dynamic module executes through direct C function pointers. Its execution speed is **100% identical** to a statically compiled module.
- Startup latency: Loading 10 dynamic modules during boot or reload adds less than **2 milliseconds** to the master process initialization phase.

---

## 9. Architectural Comparison Table

| Dimension / Metric | Static Modules | Dynamic Modules (DSO) | OpenResty Lua Modules |
| :--- | :--- | :--- | :--- |
| **Compilation Method** | Built into `nginx` binary | Compiled as standalone `.so` | Interpreted / JIT compiled |
| **Recompilation on Change**| Recompile entire NGINX | Recompile module only | Zero compilation (Edit script) |
| **Binary Flexibility** | Monolithic (Large binary) | Modular (Load as needed) | Rich scripting runtime |
| **Execution Performance**| Native C speed | **Native C speed (Identical)** | Near-native (LuaJIT) |
| **ABI Version Coupling** | Strict | **Strict (Matches exact version)**| Loosely coupled |
| **Installation via APT** | Baked into package | Available via package or source | OpenResty package |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: What is the architectural difference between a static NGINX module and a dynamic NGINX module?</strong></summary>

**Answer:**
- **Static Module**: Compiled directly into the monolithic NGINX executable at build time using `./configure --add-module=/path`. The module's C code and descriptors are linked directly into the binary's text segment. It cannot be enabled, disabled, or upgraded without completely recompiling the NGINX binary.
- **Dynamic Module (DSO)**: Compiled as a separate Dynamic Shared Object (`.so`) file using `./configure --add-dynamic-module=/path`. The module is loaded into the NGINX master process at runtime using the POSIX `dlopen()` system call triggered by the `load_module` directive. Once loaded, the module registers its handlers into NGINX's global module array. Dynamic modules provide modularity, smaller baseline binaries, and easier maintenance.
</details>

<details>
<summary><strong>Q2: Why does NGINX enforce strict ABI compatibility on dynamic modules, and what causes an ABI mismatch?</strong></summary>

**Answer:**
In C, compilers do not guarantee a fixed memory layout for internal structures across different compiler flags or versions. The NGINX core struct `ngx_http_request_t` contains hundreds of fields, pointers, and bitmasks whose memory offsets shift depending on:
1. The exact NGINX version number.
2. Build-time flags (e.g., `--with-debug`, `--with-http_ssl_module`, `--with-pcre`).

If NGINX loaded a dynamic module compiled with different flags, the module would attempt to read struct fields at incorrect memory offsets, leading to immediate memory corruption and segmentation faults. To prevent this, NGINX embeds an **ABI signature string** into every binary and module. During `load_module`, NGINX verifies that the module's signature matches the master binary's signature character-for-character before executing code.
</details>

<details>
<summary><strong>Q3: What is the purpose of the --with-compat configure flag?</strong></summary>

**Answer:**
Introduced in NGINX 1.11.5, `--with-compat` enables **dynamic module binary compatibility**. 

When building third-party dynamic modules, passing `--with-compat` configures internal struct padding and alignment flags to match standard production NGINX binaries. This allows operators to compile a third-party dynamic module (e.g., `headers-more` or `brotli`) without having to replicate every single obscure configure argument that the official Linux distribution used to build the host NGINX binary.
</details>

<details>
<summary><strong>Q4: Why must load_module directives appear at the very top of nginx.conf?</strong></summary>

**Answer:**
`load_module` is a core directive processed during the earliest lexical tokenization phase of the Master process, before any subsystem blocks (`events {}`, `http {}`, `stream {}`) are initialized.

When NGINX parses a configuration, it needs to know all valid directives registered by all modules. If a dynamic module introduces a new directive (e.g., `brotli on;` in `http {}`), the dynamic module **must already be loaded and registered in memory** when the parser encounters `brotli on;`. If `load_module` were placed inside `http {}`, the parser would encounter `brotli on;` before loading the module, failing with `[emerg] unknown directive "brotli"`.
</details>

<details>
<summary><strong>Q5: What are the lifecycle hooks available in ngx_module_t?</strong></summary>

**Answer:**
The `ngx_module_t` structure provides function pointer callbacks that execute at specific milestones in the server lifecycle:
- `init_master`: Called once by the master process when the module is initialized.
- `init_module`: Called by the master process after the configuration is compiled and validated.
- `init_process`: Called by each worker process immediately after it is forked by the master process (ideal for initializing per-worker connections or timers).
- `exit_process`: Called by the worker process right before it terminates.
- `exit_master`: Called by the master process right before the server completely exits.
</details>

<details>
<summary><strong>Q6: How do you compile a dynamic module without overwriting the existing production NGINX binary?</strong></summary>

**Answer:**
Execute `make modules` instead of `make install`:
1. Extract NGINX source matching the target version.
2. Run `./configure` with `--with-compat --add-dynamic-module=/path/to/module`.
3. Execute:
   ```bash
   make modules
   ```
4. This compiles **only** the shared object files in the `objs/` directory (e.g., `objs/ngx_http_mymodule.so`) without building the main `nginx` executable or touching `/usr/sbin/nginx` or `/etc/nginx/`. You then manually copy the `.so` file to your modules directory.
</details>

<details>
<summary><strong>Q7: What happens if an APT or YUM system update upgrades the NGINX package on a system using custom dynamic modules?</strong></summary>

**Answer:**
If the system package manager upgrades NGINX (e.g., from version 1.26.0 to 1.26.1):
1. The host NGINX binary's ABI signature changes to reflect version 1.26.1.
2. The custom dynamic module on disk remains compiled against version 1.26.0.
3. The next time NGINX is reloaded (`nginx -s reload`) or restarted, NGINX executes `load_module`, detects the version mismatch, and fails with `[emerg] module is not binary compatible`.
4. If this happens during a reload, the reload is aborted, and old workers keep running; if it happens on a reboot, NGINX fails to boot.

To prevent this: pin the NGINX package version, or include module recompilation steps in automated server upgrade pipelines.
</details>

<details>
<summary><strong>Q8: What is ldd, and why is it useful when deploying NGINX dynamic modules?</strong></summary>

**Answer:**
`ldd` is a Linux command-line utility that prints the shared library dependencies required by a dynamic binary or `.so` file. 

When deploying a custom dynamic module (e.g., `ngx_http_geoip2_module.so` or `ngx_http_lua_module.so`), the module often depends on underlying system C libraries (`libmaxminddb.so`, `libluajit-5.1.so`). Running:
```bash
ldd /etc/nginx/modules/ngx_http_geoip2_module.so
```
verifies that all required shared libraries exist on the target system. If any library reports `"not found"`, NGINX will fail to load the module with a `dlopen()` error until that library package is installed via `apt-get` or `yum`.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Loaded Dynamic Modules</strong></summary>

**Goal:** Use `lsof` and `nginx -V` to list all dynamically loaded `.so` modules currently open in the running NGINX master process.

**Solution:**
```bash
MASTER_PID=$(cat /var/run/nginx.pid)
lsof -p $MASTER_PID | grep "\.so"
```
</details>

<details>
<summary><strong>Exercise 2: Load the Official GeoIP2 or njs Dynamic Module</strong></summary>

**Goal:** Install an official NGINX dynamic module package and configure `load_module` at the top of `nginx.conf`.

**Solution:**
```bash
# On Debian/Ubuntu:
apt-get install libnginx-mod-http-geoip2
```
In `/etc/nginx/nginx.conf` (line 1):
```nginx
load_module modules/ngx_http_geoip2_module.so;
```
Validate syntax: `nginx -t`.
</details>

<details>
<summary><strong>Exercise 3: Check Shared Library Dependencies with ldd</strong></summary>

**Goal:** Verify that a dynamic module has zero unresolved system dependencies.

**Solution:**
```bash
MODULE_PATH=$(find /usr/lib/nginx/modules /etc/nginx/modules -name "*.so" 2>/dev/null | head -n 1)
if [[ -n "$MODULE_PATH" ]]; then
    ldd "$MODULE_PATH"
fi
```
Verify that no lines contain `not found`.
</details>

<details>
<summary><strong>Exercise 4: Verify ABI Signature String via strings</strong></summary>

**Goal:** Extract and inspect the embedded NGINX compilation signature from the main binary.

**Solution:**
```bash
strings /usr/sbin/nginx | grep -E "^[0-9]{7},"
# Example output:
# 1026001,1,1,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
```
</details>

<details>
<summary><strong>Exercise 5: Simulate Module Loading in Wrong Context</strong></summary>

**Goal:** Intentionally place `load_module` inside the `http {}` block and observe the configuration error.

**Solution:**
1. Move `load_module` inside `http { ... }`.
2. Run `nginx -t`.
3. Observe error: `nginx: [emerg] "load_module" directive is not allowed here`.
4. Restore `load_module` to the `main` context.
</details>

<details>
<summary><strong>Exercise 6: Query Configure Arguments with grep</strong></summary>

**Goal:** Extract only the `--with` and `--add-module` arguments from `nginx -V` for documentation.

**Solution:**
```bash
nginx -V 2>&1 | tr ' ' '\n' | grep -E "^--(with|add)"
```
</details>

---

## 12. Summary & Quick Reference

```nginx
# Dynamic Module Loading Quick Reference

# 1. Main Context ONLY (Line 1 of nginx.conf)
load_module modules/ngx_http_geoip2_module.so;
load_module modules/ngx_http_js_module.so;
load_module /usr/lib/nginx/modules/ngx_http_brotli_filter_module.so;

user nginx;
worker_processes auto;
# ...
```

```bash
# Compilation Commands
# Always use matching version and --with-compat
./configure --with-compat --add-dynamic-module=/path/to/source
make modules
# Copy result: objs/*.so to /etc/nginx/modules/
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX Dynamic Modules Announcement](https://www.nginx.com/blog/dynamic-modules-nginx-1-9-11/)
- [NGINX Core Development Guide: Architecture & Modules](https://nginx.org/en/docs/dev/development_guide.html)
- [Compiling Dynamic Modules for NGINX Plus and Open Source](https://docs.nginx.com/nginx/admin-guide/dynamic-modules/dynamic-modules/)
- [Emiller's Guide to NGINX Module Architecture](http://www.evanmiller.org/nginx-modules-guide.html)

---

[← Previous: Memory Buffers & Shared Zones](04-memory-buffers-and-shared-zones.md) | [Module Overview](README.md) | [Next: Reading NGINX Source Code →](06-reading-nginx-source-code.md)
