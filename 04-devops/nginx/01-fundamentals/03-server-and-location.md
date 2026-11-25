<!--
================================================================================
Metadata:
  Title: Server Blocks, Location Modifiers & URI Routing Priority
  Track: NGINX & Traffic Engineering / DevOps
  Difficulty: Intermediate
  Estimated Time: 55 minutes
  Prerequisites: 01-fundamentals/02-config-structure.md
  Key Terminology: server_name, Location Modifiers, Exact Match (=), Non-Regex Prefix (^~), Regex (~/~*), try_files, Named Locations, internal
================================================================================
-->

# Server Blocks, Location Modifiers & URI Routing Priority

[<- 02-config-structure.md](./02-config-structure.md) | [Back to Module Overview](./README.md) | [04-static-files.md ->](./04-static-files.md)

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

In NGINX, traffic routing operates in two distinct phases:
1. **Virtual Host Selection**: Which `server {}` block handles the incoming TCP/TLS connection based on the IP address, port, and the client's `Host` header (or TLS SNI extension)?
2. **URI Location Routing**: Once the server block is selected, which `location {}` block processes the normalized request URI?

Misunderstanding location matching priority is the **single largest source of configuration bugs and security vulnerabilities** in NGINX. Engineers frequently assume that NGINX evaluates location blocks sequentially from the top of the file to the bottom, similar to a firewall rule list or an Express.js router.

**This is completely false.** NGINX uses a sophisticated, multi-tier evaluation algorithm where exact matches and non-regex prefix matches take precedence over regular expressions, while regular expressions override standard prefix matches regardless of their position in the file. Failing to grasp this leads to regex hijacking, exposed administrative directories, and broken API routing.

---

## 2. Core Concepts

### `server_name` Matching Precedence
When an HTTP request arrives, NGINX tests the incoming `Host` header against `server_name` directives using this exact precedence:

```
[ Precedence 1: Exact Name ]       -> server_name example.com;
               |
               v (if no match)
[ Precedence 2: Leading Wildcard ] -> server_name *.example.com; (Longest match wins)
               |
               v (if no match)
[ Precedence 3: Trailing Wildcard ]-> server_name mail.*; (Longest match wins)
               |
               v (if no match)
[ Precedence 4: Regular Expression]-> server_name ~^(?<subdomain>.+)\.example\.com$;
               |
               v (if no match)
[ Precedence 5: Default Server ]   -> The block with 'listen ... default_server;'
                                      (or the very first defined server block)
```

### The 5 Location Modifiers
Location blocks define routing rules for the normalized URI (the path portion after the domain, with query strings stripped).

| Modifier | Match Type | Priority Tier | Halts Evaluation? | Description |
| :---: | :--- | :---: | :---: | :--- |
| **`=`** | Exact Match | **Tier 1** (Highest) | **Yes** | URI must match string character-for-character |
| **`^~`** | Preferential Prefix | **Tier 2** | **Yes** | Longest matching prefix; skips ALL regex checks! |
| **`~`** | Regex (Case-Sensitive) | **Tier 3** | **Yes** (on first match)| Evaluated sequentially in order of appearance in file |
| **`~*`** | Regex (Case-Insensitive)| **Tier 3** | **Yes** (on first match)| Evaluated sequentially in order of appearance in file |
| *(none)*| Standard Prefix | **Tier 4** (Lowest) | **No** | Longest matching prefix; used ONLY if no regex matches |

### Special Location Types
- **Named Locations (`@name`)**: An internal location that cannot be requested directly by an external client. It is invoked exclusively via internal redirections, such as `try_files` or `error_page` (e.g., `@fallback`).
- **Internal Locations (`internal;`)**: A location marked with the `internal;` directive. External requests to this path immediately receive an `HTTP 404 Not Found`. It is only accessible via internal subrequests, such as `auth_request`, error page handlers, or `X-Accel-Redirect` headers.

---

## 3. How It Works Internally

### The 4-Step Location Resolution Algorithm
When resolving a request URI, NGINX executes the following microarchitectural sequence:

```
                  [ Incoming Request URI: /static/app.js ]
                                     |
                                     v
+--------------------------------------------------------------------------+
| STEP 1: Check Exact Matches (=)                                          |
| -> Does URI exactly match any 'location = /path' block?                   |
|    YES -> Execute that location immediately. HALT!                       |
|    NO  -> Proceed to Step 2.                                             |
+--------------------------------------------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
| STEP 2: Evaluate All Prefix Strings (Both '^~' and standard no-modifier) |
| -> NGINX scans ALL prefix locations across the server block.             |
| -> Finds and remembers the LONGEST matching prefix string.               |
|                                                                          |
| -> Did the longest matching prefix have the '^~' modifier?               |
|    YES -> Execute that location immediately. SKIP REGEX CHECKS! HALT!    |
|    NO  -> Remember this prefix, but DO NOT execute yet. Proceed to Step 3|
+--------------------------------------------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
| STEP 3: Evaluate Regular Expressions (~ and ~*) in Order of Appearance   |
| -> Scans regex locations from TOP to BOTTOM as written in the file.      |
| -> Did a regex match the URI?                                            |
|    YES -> Execute the FIRST matching regex location immediately. HALT!   |
|    NO  -> Proceed to Step 4.                                             |
+--------------------------------------------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
| STEP 4: Fallback to Longest Prefix from Step 2                           |
| -> No regex matched. Execute the longest prefix recorded in Step 2.      |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Syntax Reference for Modifiers
```nginx
server {
    listen 80;
    server_name example.com;

    # 1. Exact Match (=): Matches ONLY '/login' (not '/login/', not '/login?a=1')
    location = /login {
        proxy_pass http://auth_backend;
    }

    # 2. Preferential Prefix (^~): Matches '/static/...' and PREVENTS any regex check!
    location ^~ /static/ {
        root /var/www/assets;
    }

    # 3. Case-Sensitive Regex (~): Matches files ending in .php
    location ~ \.php$ {
        fastcgi_pass unix:/run/php/php-fpm.sock;
    }

    # 4. Case-Insensitive Regex (~*): Matches .jpg, .JPG, .png, .PNG
    location ~* \.(jpg|jpeg|png|gif|ico|css|js)$ {
        expires 30d;
        add_header Cache-Control "public, no-transform";
    }

    # 5. Standard Prefix (no modifier): Matches '/api/...' unless overridden by regex
    location /api/ {
        proxy_pass http://api_backend;
    }

    # 6. Default Fallback Prefix: Matches everything
    location / {
        try_files $uri $uri/ /index.html;
    }

    # 7. Named Location: Used as internal fallback
    location @fallback {
        proxy_pass http://maintenance_server;
    }
}
```

---

## 5. Practical Examples

### Example 1: The Classic Regex Hijacking Defect & Remediation
```nginx
# VULNERABLE CONFIGURATION (Regex Hijacks Static Asset):
server {
    listen 80;
    server_name portal.example.com;

    # Intended to serve secure private documents
    location /documents/ {
        auth_basic "Restricted Access";
        auth_basic_user_file /etc/nginx/.htpasswd;
        root /var/www/portal;
    }

    # Intended to cache public images
    location ~* \.(pdf|png|jpg)$ {
        expires 7d;
        root /var/www/public;
    }
}
# WHAT GOES WRONG:
# A request arrives for: /documents/confidential.pdf
# 1. Step 2 finds longest prefix: '/documents/' (standard prefix, no modifier).
# 2. Step 3 scans regexes: '\.(pdf|png|jpg)$' MATCHES!
# 3. NGINX routes request to the regex block. The 'auth_basic' check is BYPASSED!
# Confidential document is served publicly without authentication!

# REMEDIATION (Using ^~ to halt regex evaluation):
location ^~ /documents/ {
    auth_basic "Restricted Access";
    auth_basic_user_file /etc/nginx/.htpasswd;
    root /var/www/portal;
}
# With ^~, NGINX halts evaluation at Step 2. Regexes are never tested!
```

### Example 2: Modern SPA Routing with Static API Proxy (`try_files`)
```nginx
server {
    listen 80;
    server_name app.internal;
    root /var/www/spa/dist;
    index index.html;

    # Fast-path for exact root
    location = / {
        try_files /index.html =404;
    }

    # Preferential prefix for static bundle assets
    location ^~ /assets/ {
        expires 1y;
        add_header Cache-Control "public, immutable";
    }

    # Reverse proxy for backend REST API
    location ^~ /api/ {
        proxy_pass http://127.0.0.1:3000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }

    # SPA Client-Side Routing Fallback
    location / {
        # Check if actual file exists ($uri)
        # Check if directory exists ($uri/)
        # Otherwise fallback to index.html for React/Vue Router to handle
        try_files $uri $uri/ /index.html;
    }
}
```

### Example 3: Protecting Microservice Internal Endpoints (`internal;`)
```nginx
server {
    listen 80;
    server_name billing.internal;

    # Public endpoint initiates payment
    location /checkout {
        proxy_pass http://checkout_service;
    }

    # Critical internal endpoint that triggers database account credit
    # MUST NEVER be accessible by clients calling http://billing.internal/credit
    location = /internal/credit {
        internal; # Returns 404 if accessed directly by an external client!
        proxy_pass http://ledger_service;
    }

    # Custom internal error handler
    error_page 500 502 503 504 /custom_error.html;
    location = /custom_error.html {
        internal;
        root /var/www/errors;
    }
}
```

### Example 4: Named Location Fallback with Upstream Retry
```nginx
upstream primary_api {
    server 10.0.1.10:8080 max_fails=2 fail_timeout=5s;
}

upstream backup_api {
    server 10.0.2.10:8080;
}

server {
    listen 80;
    server_name api.example.com;

    location / {
        proxy_pass http://primary_api;
        proxy_intercept_errors on;
        error_page 502 503 504 = @backup;
    }

    location @backup {
        proxy_pass http://backup_api;
        proxy_set_header X-Failover "true";
    }
}
```

### Example 5: Regex Capture Groups in `server_name` and `location`
```nginx
server {
    listen 80;
    # Capture tenant subdomain dynamically
    server_name ~^(?<tenant>[a-z0-9]+)\.saasplatform\.com$;

    location / {
        proxy_pass http://tenant_backend;
        # Forward extracted tenant identifier to backend application
        proxy_set_header X-Tenant-ID $tenant;
    }

    # Regex location with named capture group for versioning
    location ~ ^/v(?<apiversion>[0-9]+)/data {
        proxy_pass http://api_gateway/api/$apiversion/data;
    }
}
```

---

## 6. Best Practices

- **Always Use `^~` for Static Asset Paths**: Prefixing asset paths with `^~` (`location ^~ /static/`, `location ^~ /media/`) eliminates regex checking overhead and eliminates regex hijacking vulnerabilities.
- **Use Exact Match (`=`) for High-Traffic Homepages**: Setting `location = / { ... }` short-circuits evaluation on the very first step, reducing CPU cycles for homepage requests.
- **Never Rely on Location File Order for Prefix Matches**: Standard prefix matches (`location /api/` vs `location /api/v1/`) are resolved by **longest prefix length**, not by the order they appear in `nginx.conf`.
- **Always Test Regular Expressions with Automated Suites**: Regexes are evaluated sequentially. A loose regex at line 10 will prevent a specific regex at line 50 from ever executing.
- **Use `internal;` for Subrequests & X-Accel-Redirect**: Protect backend authentication handlers and private storage downloads by marking them `internal;`.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| Static images in `/admin/images/` served without admin auth | A case-insensitive regex `location ~* \.png$` lower in the file hijacked the request from `location /admin/`. | Use preferential prefix: `location ^~ /admin/`. |
| `try_files` ends with file instead of fallback | `try_files $uri $uri/ index.html;` (missing slash on `index.html`) tries to open a literal relative file, failing with 500. | Ensure fallback path begins with `/`: `try_files $uri $uri/ /index.html;`. |
| Trailing slash mismatch in prefix location | `location /app` matches `/application`, `/app/`, and `/apple`; while `location /app/` matches only children. | Be explicit: use `/app/` if you only want to match paths inside the `/app/` directory. |
| Attempting to access named location directly via browser (`http://host/@fallback`) | Named locations are internal to NGINX memory and cannot be resolved by incoming URL paths; returns 404. | Use standard URI paths or trigger via `error_page` / `try_files`. |
| Regex evaluation slows down server under heavy load | Excessive complex regex locations force NGINX to execute PCRE string evaluation on every request. | Consolidate regexes or convert to prefix matches with `^~`. |
| Named capture group in `server_name` collides with variable | Creating a capture variable named `$host` overwrites the built-in NGINX `$host` variable. | Always use distinct custom names (e.g., `$tenant_id`, `$subdomain`). |

---

## 8. Performance, Security & Cost Implications

- **Performance**:
  - `=` (Exact Match) has $O(1)$ constant time complexity.
  - Prefix matching evaluates a compiled trie structure in $O(L)$ time where $L$ is URI length.
  - Regular expressions require the PCRE engine to scan strings sequentially, consuming significantly more CPU cycles. Using `^~` to skip regex evaluation provides measurable latency improvements under high load.
- **Security**: Location priority confusion is the primary mechanism behind authorization bypasses in NGINX. If a reverse proxy enforces token validation at `location /api/`, but an unauthenticated regex `location ~* \.json$` matches `/api/users.json`, sensitive user data is exposed without credentials.
- **Cost**: Optimized location routing decreases CPU utilization per request, allowing smaller compute instance sizes (e.g., downsizing from AWS `c6i.2xlarge` to `c6i.xlarge`).

---

## 9. Comparison Table

| Feature / Trait | Exact Match (`=`) | Non-Regex Prefix (`^~`) | Case-Sensitive Regex (`~`) | Case-Insensitive Regex (`~*`) | Standard Prefix (*none*) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Priority** | **1** (Top) | **2** | **3** | **3** | **4** (Lowest) |
| **Halts on Match?** | Yes | Yes (at Step 2) | Yes (first match)| Yes (first match) | No |
| **File Order Matters?**| No | No | **Yes** (top to bottom) | **Yes** (top to bottom) | No (longest wins) |
| **Regex Enabled?** | No | No | Yes | Yes | No |
| **Performance** | Fastest ($O(1)$) | Very Fast | Moderate (PCRE) | Moderate (PCRE) | Fast (Trie search) |
| **Best Used For** | Homepage (`= /`), `/login` | `/static/`, `/assets/`, `/docs/` | Binary file types | Image/Font caches | Broad route buckets |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Walk through the exact algorithm NGINX uses to select which `location {}` block handles an incoming request.</strong></summary>

**Answer**:
1. **Step 1 (Exact Match)**: NGINX tests the normalized URI against all locations with the `=` modifier. If an exact match is found, NGINX halts search immediately and executes that location.
2. **Step 2 (Prefix Search)**: NGINX tests the URI against all prefix locations (both standard prefix and `^~`). It evaluates all of them and determines the **longest matching prefix string**.
   - If that longest matching prefix has the **`^~`** modifier, NGINX halts search immediately and executes that location (all regular expressions are completely skipped!).
   - If that longest matching prefix is a standard prefix (no modifier), NGINX temporarily stores this location and proceeds to Step 3.
3. **Step 3 (Regular Expressions)**: NGINX scans all regular expression locations (`~` and `~*`) in the exact order they are written in the configuration file from top to bottom. The **first** regex that matches the URI is executed immediately, and evaluation halts.
4. **Step 4 (Prefix Fallback)**: If no regular expression matched, NGINX executes the longest prefix location remembered from Step 2.
</details>

<details>
<summary><strong>2. How does `location ^~ /images/` differ from `location /images/`?</strong></summary>

**Answer**:
Both locations match any URI starting with `/images/`. However:
- `location /images/` is a standard prefix match. If a regular expression later in the configuration matches the URI (for example, `location ~* \.png$`), NGINX will execute the regex location instead, overriding `/images/`.
- `location ^~ /images/` is a **preferential prefix match**. If `/images/` is the longest matching prefix, the `^~` modifier explicitly tells NGINX: *"Stop searching! Do not evaluate ANY regular expressions."* It guarantees that requests inside `/images/` cannot be intercepted or hijacked by subsequent regex blocks.
</details>

<details>
<summary><strong>3. Why does NGINX evaluate regular expression locations in file order, but prefix locations by longest match?</strong></summary>

**Answer**:
- **Prefix Locations** represent directory and path hierarchies. Hierarchical paths naturally have a "most specific" subset relationship (e.g., `/api/v1/users` is more specific than `/api/`). To prevent file ordering from breaking nested routing, NGINX always selects the longest matching prefix regardless of where it appears in the configuration.
- **Regular Expressions** can overlap in arbitrary, mathematically non-deterministic ways that do not have a clear "longest" or "most specific" definition (e.g., `^/user_[0-9]+` vs `.*\.json$`). Because the NGINX engine cannot objectively rank regex specificity, it relies on the engineer's explicit ordering, evaluating regexes sequentially from top to bottom and selecting the first match.
</details>

<details>
<summary><strong>4. What is a "Named Location" in NGINX, and what are its constraints?</strong></summary>

**Answer**:
A Named Location is defined with an `@` prefix (e.g., `location @app_backend { ... }`).
Constraints and properties:
1. It is not associated with any URI path prefix or regex.
2. It **cannot** be matched directly by client requests. A client browsing to `http://example.com/@app_backend` will receive a 404.
3. It can only be reached through internal redirections from directives such as `try_files`, `error_page`, or `post_action`.
4. Named locations cannot be nested inside other location blocks.
</details>

<details>
<summary><strong>5. Explain the execution flow of `try_files $uri $uri/ /index.html;`. What happens if none of the elements exist?</strong></summary>

**Answer**:
The `try_files` directive tests for the existence of files and directories in order:
1. `$uri`: Checks if a regular file matching the exact URI exists relative to `root`. If found, serves it.
2. `$uri/`: If `$uri` was not a file, checks if a directory exists at that path. If found, serves the index file defined for that directory.
3. `/index.html`: If neither exists, NGINX triggers an **internal rewrite** to `/index.html`. NGINX re-runs the entire location resolution algorithm for the new URI `/index.html`.

If the final parameter is a code (e.g., `=404`), NGINX returns that HTTP status code. If the final parameter is a URI (like `/index.html`) and that file also does not exist on disk, NGINX enters an internal loop or returns an `HTTP 500 Internal Server Error`.
</details>

<details>
<summary><strong>6. How does the `internal;` directive function, and what security use case does it solve?</strong></summary>

**Answer**:
The `internal;` directive specifies that the enclosing location block may only be used for internal requests. If an external client attempts to access the location directly, NGINX returns `404 Not Found`.

Common production use cases:
1. **`auth_request` authentication verification**: An internal location that queries an identity provider (e.g., `/internal_auth`).
2. **`X-Accel-Redirect` private file downloads**: A backend application (Node.js/Django) authenticates a user, then sends an `X-Accel-Redirect: /protected/file.pdf` header. NGINX intercepts this header and serves the file from the `internal;` location, offloading file I/O from the application while keeping files private.
3. **Custom error pages**: Ensuring clients cannot view raw template error pages directly.
</details>

<details>
<summary><strong>7. How does NGINX select a `server {}` block when the client does not provide a `Host` header?</strong></summary>

**Answer**:
If an incoming request lacks a `Host` header (or supplies an IP address or unrecognized domain), NGINX routes the connection to the **Default Server** for the listening IP and port:
1. It checks if any server block on that port explicitly specifies the `default_server` parameter:
   `listen 80 default_server;`
2. If no server block has the `default_server` parameter explicitly declared, NGINX designates the **very first server block** defined in the configuration files (based on file load order) as the default server.
</details>

<details>
<summary><strong>8. What is the danger of using unescaped dots in regular expression locations (e.g., `location ~ .php$`)?</strong></summary>

**Answer**:
In regular expression syntax, an unescaped dot (`.`) matches **any single character**, not just a literal dot.

If configured as `location ~ .php$`:
- It matches valid scripts like `/index.php`.
- But it also matches `/malicious_payload_xphp` or `/upload/image.aphp`.

An attacker could upload a file with an arbitrary extension that matches the regex, tricking NGINX into passing untrusted user input to the FastCGI/PHP interpreter. Dots intended as literal file extension separators must always be properly escaped with a backslash: `location ~ \.php$`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Determine Winning Location Block</strong></summary>

**Task**: Given the following configuration, which location block will NGINX select for a request to `http://example.com/api/v1/users`?

```nginx
location / { ... }                  # Block A
location /api/ { ... }              # Block B
location /api/v1/ { ... }           # Block C
location ^~ /api/v1/users { ... }   # Block D
```

**Solution**:
**Block D**.
Block D uses the `^~` modifier on `/api/v1/users`, which is the longest matching prefix for the URI. In Step 2, NGINX selects Block D and halts evaluation immediately.
</details>

<details>
<summary><strong>Exercise 2 (Easy): Identify Regex Hijacking</strong></summary>

**Task**: In the configuration below, which block handles `/downloads/report.pdf`?

```nginx
location /downloads/ { ... }        # Block A
location ~* \.pdf$ { ... }          # Block B
```

**Solution**:
**Block B**.
Block A is a standard prefix match (Step 2). Because it lacks `^~`, NGINX proceeds to Step 3 and evaluates regular expressions. Block B matches `\.pdf$` and is executed immediately, overriding Block A.
</details>

<details>
<summary><strong>Exercise 3 (Medium): Construct SPA Routing Fallback with `try_files`</strong></summary>

**Task**: Author an NGINX server block for `app.local` that serves static files from `/var/www/my-app`, caches images ending in `.png` and `.jpg` for 7 days, and routes all other non-existent paths to `/index.html` without breaking image 404s.

**Solution**:
```nginx
server {
    listen 80;
    server_name app.local;
    root /var/www/my-app;
    index index.html;

    # Image cache with explicit 404 on missing assets (does NOT fallback to index.html)
    location ~* \.(png|jpg|jpeg|gif|svg|ico)$ {
        expires 7d;
        try_files $uri =404;
    }

    # SPA routing fallback
    location / {
        try_files $uri $uri/ /index.html;
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Implement Hardened Default Catch-All Server</strong></summary>

**Task**: Author an explicit `default_server` block that listens on port 80 and 443, drops any connection that does not match an approved hostname by returning NGINX's non-standard close status `444`.

**Solution**:
```nginx
server {
    listen 80 default_server;
    listen 443 ssl default_server;
    server_name _;

    # Self-signed placeholder certificates for SSL handshake termination
    ssl_certificate /etc/nginx/ssl/dummy.crt;
    ssl_certificate_key /etc/nginx/ssl/dummy.key;

    # Return 444 (Connection closed without sending response headers)
    return 444;
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Route Subdomains Dynamically via Regex Capture</strong></summary>

**Task**: Configure a server block that captures a wildcard subdomain and routes requests to an internal microservice running on a port derived from a map block.

**Solution**:
```nginx
map $client_tenant $backend_port {
    alpha    8081;
    beta     8082;
    default  8080;
}

server {
    listen 80;
    server_name ~^(?<client_tenant>[a-z0-9]+)\.platform\.internal$;

    location / {
        proxy_pass http://127.0.0.1:$backend_port;
        proxy_set_header Host $host;
        proxy_set_header X-Tenant $client_tenant;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build an `internal;` Protected Download Service</strong></summary>

**Task**: Configure an NGINX location `/protected/` that stores sensitive files on disk (`/var/data/vault/`), is inaccessible to direct client calls, but serves files when redirected by a backend application using `X-Accel-Redirect`.

**Solution**:
```nginx
server {
    listen 80;
    server_name files.internal;

    # Public gateway endpoint proxies to authentication service
    location /download/ {
        proxy_pass http://127.0.0.1:4000/authorize-download/;
    }

    # Protected storage location
    location ^~ /protected/ {
        internal; # Blocks direct external requests with 404!
        alias /var/data/vault/;
    }
}
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Modifier | Precedence | Traversal Behavior | Example Syntax |
| :---: | :---: | :--- | :--- |
| **`=`** | **1** | Exact string match; halts search immediately | `location = /health` |
| **`^~`** | **2** | Preferential prefix; halts search, skips all regexes | `location ^~ /static/` |
| **`~`** | **3** | Case-sensitive regex; first match wins | `location ~ \.php$` |
| **`~*`** | **3** | Case-insensitive regex; first match wins | `location ~* \.png$` |
| *(none)*| **4** | Longest prefix match; fallback if no regex matches | `location /api/` |
| **`@`** | N/A | Named location; internal redirection only | `location @fallback` |
| **`internal`**| N/A | Subrequest only; returns 404 to external clients | `location /internal/ { internal; }` |

---

## 13. Further Reading & Official Documentation

- [Official NGINX Documentation: How NGINX Processes a Request](https://nginx.org/en/docs/http/request_processing.html)
- [Official NGINX Documentation: ngx_http_core_module Location Directive](https://nginx.org/en/docs/http/ngx_http_core_module.html#location)
- [Official NGINX Documentation: Server Names Resolution](https://nginx.org/en/docs/http/server_names.html)
- [DigitalOcean: Understanding Nginx Server and Location Block Selection Algorithms](https://www.digitalocean.com/community/tutorials/understanding-nginx-server-and-location-block-selection-algorithms)
- [OWASP Secure NGINX Configuration Guide](https://cheatsheetseries.owasp.org/)

---

[<- 02-config-structure.md](./02-config-structure.md) | [Back to Module Overview](./README.md) | [04-static-files.md ->](./04-static-files.md)
