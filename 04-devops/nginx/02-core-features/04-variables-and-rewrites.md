---
title: "NGINX Variables, Rewrites, and the Map Directive"
description: "Mastering NGINX built-in variables, the high-performance map directive, URI rewrites, regex captures, and avoiding the 'If is Evil' trap."
module: "02-core-features"
track: "devops-nginx"
order: 4
difficulty: "Intermediate to Advanced"
prerequisites:
  - "01-fundamentals/02-config-structure"
  - "01-fundamentals/03-server-and-location"
tags:
  - nginx
  - variables
  - rewrite
  - map
  - if-is-evil
  - regex
  - routing
last_updated: "2026-03-30"
---

# NGINX Variables, Rewrites, and the Map Directive

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: The High-Performance `map` Directive for Clean Routing](#example-1-the-high-performance-map-directive-for-clean-routing)
  - [Example 2: Canonical Redirections and Query String Preservation](#example-2-canonical-redirections-and-query-string-preservation)
  - [Example 3: Complex URL Rewriting with Regex Captures (`break` vs `last`)](#example-3-complex-url-rewriting-with-regex-captures-break-vs-last)
  - [Example 4: Safe Device / Bot Detection without `if` Hell](#example-4-safe-device--bot-detection-without-if-hell)
  - [Example 5: Dynamic Upstream Routing Based on GeoIP / Headers](#example-5-dynamic-upstream-routing-based-on-geoip--headers)
- [6. Production Best Practices](#6-production-best-practices)
- [7. Common Pitfalls & "If is Evil"](#7-common-pitfalls--if-is-evil)
- [8. Performance, Security & Troubleshooting](#8-performance-security--troubleshooting)
- [9. Architectural Comparison Table](#9-architectural-comparison-table)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

In NGINX, dynamic request processing, intelligent traffic routing, and URL transformations rely on three interdependent primitives:
1. **Variables**: Dynamic runtime values representing HTTP headers, socket attributes, parsed URI elements, and upstream telemetry.
2. **The `map` Directive**: A hash-table-backed lookup engine declared in the `http {}` context that dynamically evaluates an output variable based on an input variable with zero runtime overhead.
3. **The `rewrite` Engine**: A PCRE (Perl Compatible Regular Expressions) based engine provided by `ngx_http_rewrite_module` to alter request URIs internally or issue HTTP redirects (301/302).

### The Danger: "If is Evil"
Historically, engineers arriving from procedural programming backgrounds attempt to use the `if` directive inside NGINX `location {}` blocks as if it were a standard programming construct. In NGINX's declarative event-driven architecture, `if` creates hidden internal locations that silently disable standard inheritance, fail to execute proxy directives, or trigger segmentation faults. Mastering NGINX variables and rewrites means understanding **why `map` and `return` are superior to `if`** in almost all production scenarios.

---

## 2. Core Concepts

### Variable Categories in NGINX
Variables in NGINX are prefixed with `$` and fall into two primary types:
- **Core Built-in Variables**: Provided by the NGINX HTTP core and modules (e.g., `$host`, `$uri`, `$request_uri`, `$args`, `$remote_addr`, `$upstream_response_time`).
- **Dynamic Header Variables**:
  - `$http_<header_name>`: Extracts incoming HTTP client headers (e.g., `$http_authorization`, `$http_user_agent`, `$http_x_custom_token`). Note: Hyphens are converted to underscores, and names are lowercased.
  - `$sent_http_<header_name>`: Extracts HTTP response headers sent back to the client.
  - `$upstream_http_<header_name>`: Extracts response headers received from an upstream backend.
- **User-Defined Variables**: Created via `set $var value;` or dynamically generated via `map $source $target {}`.

### Key Built-in Variables Dissected
Understanding the subtle differences between URI variables is essential:
- `$request_uri`: The complete raw request URI sent by the client, including query strings, exactly as requested (e.g., `/products/item?id=42&sort=asc`). It is **immutable** and never modified by rewrites or decoding.
- `$uri`: The normalized current URI path *without* query arguments, with percent-decoding applied and consecutive slashes collapsed (e.g., `/products/item`). This variable can change dynamically if a `rewrite` occurs.
- `$document_uri`: An identical alias for `$uri`.
- `$args` or `$query_string`: The unparsed query string following the `?` delimiter (e.g., `id=42&sort=asc`).
- `$is_args`: Evaluates to `?` if `$args` is non-empty, or an empty string `""` if there are no query arguments.

### The `map` Directive
The `map` directive defines a key-value mapping evaluated lazily (on demand). It is compiled at configuration reload into an optimized hash table. If a request never references the mapped variable, NGINX executes zero CPU cycles evaluating it.

### Rewrite Flags
The `rewrite regex replacement [flag];` directive supports four flags:
- `last`: Stops processing the current set of rewrite directives and starts a completely new location search based on the modified URI.
- `break`: Stops processing the current set of rewrite directives and continues executing the current `location` block without searching for a new location.
- `redirect`: Issues a temporary HTTP `302 Moved Temporarily` redirect to the client.
- `permanent`: Issues a permanent HTTP `301 Moved Permanently` redirect to the client.

---

## 3. How It Works Internally

### Lazy Variable Evaluation vs Eager Execution

```
Incoming Request arrives at NGINX worker
           |
           v
+------------------------------------+
| HTTP Core Request Initialization   |
| (Raw buffer parsed into ngx_http_  |
|  request_t struct)                 |
+------------------+-----------------+
                   |
     Does config use $custom_var?
        /                       \
      YES                        NO
       |                          |
+------v-------------------+      |
| On-Demand Getter Executed|      |
| (Hash lookup or regex    |      |
|  evaluation via map)     |      |
+--------------+-----------+      |
               |                  |
               v                  v
+------------------------------------+
| Value Cached in Request Context    |
| (Subsequent reads in same request  |
|  reuse cached memory pointer)      |
+------------------------------------+
```

### The "If is Evil" State Machine Trap

When you place an `if` directive inside a `location` block:

```
location /api/ {
    set $val 1;
    if ($http_x_token = "") {
        # NGINX creates an anonymous "nested location" here
        # It copies some parent directives but silently discards others!
    }
    proxy_pass http://backend; # <-- Often SILENTLY IGNORED or triggers segfault!
}
```

Because NGINX evaluates `if` during the `NGX_HTTP_REWRITE_PHASE`, entering the `if` block switches the request's execution pointer to an internal pseudo-location. Directives belonging to the `CONTENT_PHASE` (like `proxy_pass`, `fastcgi_pass`, or `try_files`) are often not inherited into this pseudo-location, causing mysterious 404s or empty responses.

---

## 4. Syntax & Directives Reference

### 1. `map` Directive
```nginx
# Context: http ONLY
map $string $variable {
    default        value;
    hostnames;     # Enables wildcard matching for domain names
    volatile;      # Disables caching variable value across request phases

    key1           value1;
    *.example.com  value2;
    ~^/api/v[12]/  value3; # Regex match
}
```

### 2. `set` Directive
```nginx
# Context: server, location, if
set $variable value;
```
Assigns a value to a variable. Note: All variables in NGINX are dynamically allocated strings.

### 3. `rewrite` Directive
```nginx
# Context: server, location, if
rewrite regex replacement [last | break | redirect | permanent];
```

### 4. `return` Directive
```nginx
# Context: server, location, if
return code [text];
return code URL;
return URL;
```
Immediately stops request processing and sends the specified HTTP status code or redirection URL to the client. Highly optimized and completely safe.

---

## 5. Practical Implementation Examples

### Example 1: The High-Performance `map` Directive for Clean Routing

Replace messy nested `if` statements with a clean, declaratively evaluated hash map.

```nginx
# /etc/nginx/conf.d/map_routing.conf

# Map incoming Origin header to allowed CORS headers
map $http_origin $cors_origin {
    default                         "";
    "~^https://(.*\.)?example\.com$" "$http_origin";
    "https://admin.internal-portal.net" "$http_origin";
}

# Map API version header to internal upstream clusters
map $http_x_api_version $upstream_backend {
    default     backend_v1_cluster;
    "v2"        backend_v2_cluster;
    "v3-beta"   backend_v3_canary;
}

server {
    listen 80;
    server_name api.example.com;

    location / {
        # Only inject CORS headers if origin matched whitelist
        if ($cors_origin != "") {
            add_header 'Access-Control-Allow-Origin' $cors_origin always;
            add_header 'Access-Control-Allow-Credentials' 'true' always;
        }

        proxy_pass http://$upstream_backend;
        proxy_set_header Host $host;
    }
}
```

### Example 2: Canonical Redirections and Query String Preservation

Ensure strict canonical URLs: redirect HTTP to HTTPS, strip trailing slashes, enforce lowercase hostnames, and preserve complex query arguments.

```nginx
# /etc/nginx/conf.d/canonical_redirects.conf

server {
    listen 80;
    server_name example.com www.example.com;
    
    # 301 Permanent Redirect to HTTPS apex domain, keeping all query params
    return 301 https://example.com$request_uri;
}

server {
    listen 443 ssl;
    http2 on;
    server_name www.example.com;

    ssl_certificate     /etc/ssl/certs/example.com.crt;
    ssl_certificate_key /etc/ssl/private/example.com.key;

    # Canonical redirect from www to non-www
    return 301 https://example.com$request_uri;
}

server {
    listen 443 ssl;
    http2 on;
    server_name example.com;

    ssl_certificate     /etc/ssl/certs/example.com.crt;
    ssl_certificate_key /etc/ssl/private/example.com.key;

    # Strip accidental trailing slash on static product URLs while keeping arguments
    rewrite ^/products/(.+)/$ /products/$1 permanent;

    location /products/ {
        proxy_pass http://products_upstream;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-Proto https;
    }
}
```

### Example 3: Complex URL Rewriting with Regex Captures (`break` vs `last`)

Demonstrating the crucial behavioral difference between `break` and `last`.

```nginx
# /etc/nginx/conf.d/rewrite_engine.conf

server {
    listen 80;
    server_name media.example.com;
    root /var/www/media;

    # 1. Using 'break' inside location:
    # Rewrites the URI internally to /storage/2026/03/image_45.jpg
    # 'break' halts rewrite processing and immediately serves the file from /var/www/media
    location /images/ {
        rewrite ^/images/(\d{4})/(\d{2})/(.*)\.(png|jpg)$ /storage/$1/$2/$3.$4 break;
    }

    # 2. Using 'last' in rewrite:
    # Rewrites legacy /v1/items/123 to /v2/items/123, then halts and TRIGGERS
    # A NEW LOCATION SEARCH. NGINX will re-enter at 'location /v2/'!
    location /v1/ {
        rewrite ^/v1/(.*)$ /v2/$1 last;
    }

    location /v2/ {
        proxy_pass http://127.0.0.1:5000;
        proxy_set_header Host $host;
    }
}
```

### Example 4: Safe Device / Bot Detection without `if` Hell

Use `map` with regular expressions to classify incoming User-Agents into categories cleanly.

```nginx
# /etc/nginx/conf.d/bot_detection.conf

map $http_user_agent $client_category {
    default             "desktop";
    "~*Mobile|Android|iPhone" "mobile";
    "~*Googlebot|bingbot|Baiduspider|YandexBot" "search_crawler";
    "~*curl|wget|python-requests|Go-http-client" "cli_tool";
}

server {
    listen 80;
    server_name content.example.com;

    location / {
        # Inject classification header for backend analytics
        proxy_set_header X-Client-Device $client_category;
        
        # Rate-limit or block aggressive CLI tools
        if ($client_category = "cli_tool") {
            return 403 "Direct CLI access forbidden. Please use official API client.\n";
        }

        proxy_pass http://frontend_ssr_service;
        proxy_set_header Host $host;
    }
}
```

### Example 5: Dynamic Upstream Routing Based on GeoIP / Headers

Conditionally route traffic to regional datacenters based on Cloudflare or custom geo headers.

```nginx
# /etc/nginx/conf.d/geo_routing.conf

map $http_cf_ipcountry $datacenter_upstream {
    default         origin_us_east;
    "~^(GB|DE|FR|NL)$" origin_eu_west;
    "~^(JP|SG|KR|AU)$" origin_ap_southeast;
}

upstream origin_us_east {
    server us-east.internal.net:8080 max_fails=2 fail_timeout=10s;
}

upstream origin_eu_west {
    server eu-west.internal.net:8080 max_fails=2 fail_timeout=10s;
}

upstream origin_ap_southeast {
    server ap-se.internal.net:8080 max_fails=2 fail_timeout=10s;
}

server {
    listen 80;
    server_name global-api.example.com;

    location /api/ {
        proxy_pass http://$datacenter_upstream;
        proxy_set_header Host $host;
        proxy_set_header X-Routing-Region $datacenter_upstream;
        proxy_connect_timeout 2s;
    }
}
```

---

## 6. Production Best Practices

1. **Prefer `return` over `rewrite`**: When redirecting a request or returning a fixed HTTP error code, `return 301 https://$host$request_uri;` is faster, uses fewer CPU cycles, and is free of regex backtracking vulnerabilities.
2. **Always Use `map` in `http {}`**: Do not use multiple `if` statements to set variables conditionally. The `map` directive builds a constant-time or logarithmic hash tree, evaluated lazily.
3. **Capture Query Strings Correctly**: Remember that the replacement part of a `rewrite` automatically appends the incoming query string unless you terminate the replacement with an explicit `?`:
   ```nginx
   # Appends incoming query string automatically:
   rewrite ^/old/(.*)$ /new/$1 permanent;
   # Discards incoming query string:
   rewrite ^/old/(.*)$ /new/$1? permanent;
   ```
4. **Guard Regex Backtracking**: When writing regular expressions in `rewrite` or `map`, avoid catastrophic backtracking patterns like `(.*)+` or complex overlapping wildcards. Test regexes against ReDoS (Regular Expression Denial of Service).
5. **Beware of Variable Scope**: Variables declared with `set` in NGINX have request-wide scope, not block-level scope. Setting `$my_var` inside one block can inadvertently overwrite values in another context.

---

## 7. Common Pitfalls & "If is Evil"

### The Golden Rule of `if`
> **The only 100% safe directives inside an `if` block are `return` and `rewrite ... last;`!**

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **`proxy_pass` inside `if` in `location`** | NGINX creates an anonymous nested location where content handlers and proxy inheritance are broken, leading to silent 404s or crashes. | Use `map` in `http {}` to determine upstream variable, then call single `proxy_pass http://$target;`. |
| **Using `if ($uri ~* ...)` instead of `location`** | Evaluates regular expressions on every single request sequentially, bypassing NGINX's compiled prefix radix tree. | Use exact or prefix `location` blocks (`location /api/ { ... }`). |
| **Forgetting `?` to drop query params** | `rewrite ^/item/(.*)$ /new-item/$1 permanent;` passes old query params like `?legacy=true` automatically to the new URL. | Append `?` to strip parameters: `rewrite ^/item/(.*)$ /new-item/$1? permanent;`. |
| **Multiple sequential `set` and `if` checks** | Increases config complexity and runtime latency; can cause unpredictable variable overwriting due to phase evaluation order. | Use `map $var1:$var2 $combined_result` with delimited matching. |
| **Using `$uri` instead of `$request_uri` in redirects** | `$uri` strips query arguments and normalizes encodings. Redirecting to `$uri` drops `?token=abc` and corrupts encoded characters. | Always use `$request_uri` in redirections (`return 301 https://$host$request_uri;`). |
| **Off-by-slash with rewrites** | `rewrite ^/app/(.*) /$1 break;` converts `/app/test` to `/test`, but `/app` becomes empty string, causing root directory index leakage. | Use `rewrite ^/app(?:/(.*))?$ /$1 break;` with safe defaults. |

---

## 8. Performance, Security & Troubleshooting

### Debugging Variables with `add_header` and Log Formats
To inspect variables without guessing, create a dedicated debugging log format:

```nginx
# /etc/nginx/conf.d/debug_logging.conf

log_format debug_vars 'TIME:[$time_iso8601] CLIENT:$remote_addr '
                      'HOST:$host URI:$uri REQ_URI:$request_uri '
                      'ARGS:$args STATUS:$status UPSTREAM:$upstream_addr '
                      'UP_STATUS:$upstream_status UP_TIME:$upstream_response_time';

server {
    listen 80;
    server_name test.example.com;

    access_log /var/log/nginx/vars_debug.log debug_vars;

    # Expose variable in HTTP response header for curl inspection
    add_header X-Debug-URI "$uri" always;
    add_header X-Debug-Args "$args" always;
    add_header X-Debug-ReqURI "$request_uri" always;

    location / {
        return 200 "OK\n";
    }
}
```

### Performance Characteristics of `map`
- **Hash Table**: When keys are exact strings or hostnames, NGINX uses a bucketed hash table lookup ($O(1)$ time complexity).
- **Lazy Evaluation**: If a request to `/static/logo.png` does not read `$upstream_backend`, the `map` code path is never executed.
- **Memory Footprint**: Hash tables for maps reside in the worker's shared read-only configuration pool, consuming virtually zero additional memory at runtime.

---

## 9. Architectural Comparison Table

| Feature / Mechanism | `map` Directive | `rewrite` Engine | `if` Directive |
| :--- | :--- | :--- | :--- |
| **Context** | `http` ONLY | `server`, `location`, `if` | `server`, `location` |
| **Evaluation Timing** | Lazy (on variable read) | Eager (in Rewrite Phase) | Eager (in Rewrite Phase) |
| **Performance** | $O(1)$ Hash table lookup | PCRE regex parsing overhead | Procedural condition evaluation |
| **Safety** | 100% Safe | Safe when flags managed | **DANGEROUS** in `location` |
| **Primary Use Case** | Header mapping, backend routing, bot tagging | Modifying paths, stripping prefixes | Simple global server-level redirects |
| **Query String Handling**| Independent of URI strings | Appends by default (unless `?`) | N/A |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: Why is the NGINX if directive considered "evil" inside a location block?</strong></summary>

**Answer:**
In NGINX, the `if` directive does not behave like a standard conditional branch in C or JavaScript. Because NGINX is a declarative state machine, encountering an `if` inside a `location` creates an anonymous, hidden nested location block. 

When the condition evaluates to true, the request is internally transferred into this pseudo-location. Content phase handlers (such as `proxy_pass`, `fastcgi_pass`, `try_files`, or static file serving) are often not copied into this pseudo-location, or inheritance rules fail unpredictably. This results in baffling behaviors where directives are skipped, requests return 404s, or NGINX crashes with a segmentation fault.
</details>

<details>
<summary><strong>Q2: What is the exact difference between $uri, $document_uri, and $request_uri?</strong></summary>

**Answer:**
- `$request_uri`: Contains the raw, original request URI sent by the client, including the path and the query string (e.g., `/search?q=nginx%20tutorial`). It is immutable and never modified by normalization or internal rewrites.
- `$uri`: Contains the current normalized URI path *without* query arguments (e.g., `/search`). Percent-encoding is decoded, and redundant slashes are collapsed. `$uri` can change during request processing if an internal `rewrite` occurs.
- `$document_uri`: A direct alias for `$uri` with identical behavior.
</details>

<details>
<summary><strong>Q3: Explain the difference between the last and break flags in the rewrite directive.</strong></summary>

**Answer:**
Both flags stop the execution of further `rewrite` directives in the current block, but they differ in what happens next:
- `break`: Stops the rewrite engine and keeps the request inside the **current location block**. It continues directly to the content generation phase (serving a static file or proxying) using the modified URI.
- `last`: Stops the rewrite engine and **starts a completely new round of location matching**. NGINX takes the newly modified URI and searches all `location` blocks from the beginning to find a new matching block.
</details>

<details>
<summary><strong>Q4: How does the map directive achieve zero-cost lazy evaluation?</strong></summary>

**Answer:**
When NGINX parses a `map` block in the configuration, it compiles the entries into an optimized hash table (or an array of regexes). However, the map is not executed when a request enters NGINX. 

Instead, NGINX attaches a getter function pointer to the target variable. Only if a subsequent directive (e.g., `proxy_pass`, `add_header`, `log_format`) actually references that variable does NGINX invoke the getter, perform the hash lookup, and cache the computed string in the request's memory pool. If the variable is never referenced during the request lifecycle, the map consumes zero CPU cycles.
</details>

<details>
<summary><strong>Q5: How do you prevent NGINX from automatically appending query parameters during a URL rewrite?</strong></summary>

**Answer:**
By default, the `rewrite` directive automatically appends the original query string (`$args`) to the replacement URI. To prevent this and completely discard the original query parameters, place a question mark `?` at the very end of the replacement string:
```nginx
# Preserves query parameters:
rewrite ^/old-path$ /new-path permanent;

# Discards query parameters:
rewrite ^/old-path$ /new-path? permanent;
```
</details>

<details>
<summary><strong>Q6: How can you safely implement conditional logic that would otherwise require multiple nested if statements?</strong></summary>

**Answer:**
Use the `map` directive to concatenate multiple variables using a delimiter (e.g., a colon or hyphen) and match the combined string:
```nginx
map "$http_x_role:$request_method" $access_decision {
    default         "deny";
    "admin:POST"    "allow";
    "admin:GET"     "allow";
    "viewer:GET"    "allow";
}

server {
    location /data/ {
        if ($access_decision = "deny") {
            return 403 "Access Denied\n";
        }
        proxy_pass http://backend;
    }
}
```
This avoids nested `if` statements and maintains clean, declarative configuration.
</details>

<details>
<summary><strong>Q7: What happens when an invalid regular expression is used in a rewrite directive?</strong></summary>

**Answer:**
NGINX compiles regular expressions using the PCRE (or PCRE2) library at configuration parsing time. If a regex has invalid syntax (unbalanced parentheses, illegal escape sequences), `nginx -t` will immediately fail with a syntax error specifying the file and line number. 

If NGINX is already running, an invalid reload (`nginx -s reload`) will be rejected, and the running worker processes will continue operating using the previous valid configuration, preventing downtime.
</details>

<details>
<summary><strong>Q8: When should you use return 301 instead of rewrite ... permanent?</strong></summary>

**Answer:**
You should almost always use `return 301` whenever you are redirecting an entire domain, protocol (HTTP to HTTPS), or simple prefix. `return 301 https://$host$request_uri;`:
1. Does not invoke the PCRE regular expression engine, saving CPU.
2. Is immune to regex capture bugs and encoding corruption.
3. Is immediately understood by reviewers as an unambiguous redirection without side effects.
Reserve `rewrite ... permanent` only for cases requiring fine-grained path segment restructuring or regex pattern transformation.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Clean Lowercase Canonical Redirect</strong></summary>

**Goal:** Configure NGINX to redirect any incoming request on port 80 to HTTPS `example.com` while maintaining query parameters exactly.

**Solution:**
```nginx
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    server_name _;

    return 301 https://example.com$request_uri;
}
```
Test with:
```bash
curl -I "http://localhost/test/path?foo=bar&num=123"
# Expect: Location: https://example.com/test/path?foo=bar&num=123
```
</details>

<details>
<summary><strong>Exercise 2: Implement Multi-Tier Environment Routing with `map`</strong></summary>

**Goal:** Route incoming requests to `prod_backend` or `staging_backend` based on the value of a cookie named `X-Env`.

**Solution:**
```nginx
map $cookie_x_env $target_pool {
    default     prod_backend;
    "staging"   staging_backend;
    "canary"    canary_backend;
}

upstream prod_backend {
    server 127.0.0.1:8001;
}

upstream staging_backend {
    server 127.0.0.1:8002;
}

upstream canary_backend {
    server 127.0.0.1:8003;
}

server {
    listen 80;
    server_name app.local;

    location / {
        proxy_pass http://$target_pool;
        proxy_set_header Host $host;
    }
}
```
</details>

<details>
<summary><strong>Exercise 3: Strip File Extensions Internally without Changing Client URL</strong></summary>

**Goal:** Allow users to request `/about` or `/contact`, and internally serve `/about.html` or `/contact.html` from disk using `try_files` and rewrite.

**Solution:**
```nginx
server {
    listen 80;
    server_name web.local;
    root /var/www/static;

    location / {
        try_files $uri $uri.html $uri/ =404;
    }
}
```
</details>

<details>
<summary><strong>Exercise 4: Legacy API Migration with Query Parameter Translation</strong></summary>

**Goal:** Permanently redirect legacy URLs `/item.php?id=99` to `/v2/items/99` using `map` and `rewrite`.

**Solution:**
```nginx
map $arg_id $new_item_url {
    default "";
    "~^(\d+)$" "/v2/items/$1";
}

server {
    listen 80;
    server_name store.local;

    location = /item.php {
        if ($new_item_url != "") {
            return 301 $new_item_url; # Drops old query param automatically
        }
        return 400 "Missing or invalid item ID\n";
    }

    location /v2/items/ {
        proxy_pass http://127.0.0.1:4000;
    }
}
```
</details>

<details>
<summary><strong>Exercise 5: Dynamic Blacklist with Custom HTTP 429 Response</strong></summary>

**Goal:** Use `map` to inspect the incoming `$remote_addr` or `$http_x_api_key` against a blocklist, returning HTTP 429 Too Many Requests with a JSON error payload.

**Solution:**
```nginx
map $http_x_api_key $is_abusive {
    default             0;
    "bad-actor-token-1" 1;
    "bad-actor-token-2" 1;
}

server {
    listen 80;
    server_name api.local;

    location / {
        if ($is_abusive = 1) {
            default_type application/json;
            return 429 '{"error": "Too Many Requests", "message": "API key blocked due to rate violations"}';
        }
        proxy_pass http://127.0.0.1:3000;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6: Diagnose Rewrite Loops and Debug via Custom Headers</strong></summary>

**Goal:** Configure NGINX to expose `$uri` and rewrite transitions via response headers to troubleshoot an internal redirection loop.

**Solution:**
```nginx
server {
    listen 80;
    server_name loop-debug.local;

    # Expose current internal execution state
    add_header X-Debug-Stage-1 "$uri" always;

    location /start/ {
        rewrite ^/start/(.*)$ /middle/$1 last;
    }

    location /middle/ {
        rewrite ^/middle/(.*)$ /end/$1 last;
    }

    location /end/ {
        return 200 "Destination reached successfully! Final URI: $uri\n";
    }
}
```
</details>

---

## 12. Summary & Quick Reference

```nginx
# Syntax Reference Cheat Sheet

# 1. High-Performance Variable Mapping (in http context)
map $input_var $output_var {
    default         "fallback";
    "exact_match"   "result1";
    "~*regex_match" "result2";
}

# 2. Canonical Redirection
return 301 https://$host$request_uri;

# 3. Safe Rewriting
# 'break' terminates rewrites and serves from current location
rewrite ^/old/(.*)$ /new/$1 break;

# 'last' terminates rewrites and re-runs location selection
rewrite ^/v1/(.*)$ /v2/$1 last;

# 4. Discard Query Arguments
rewrite ^/purge/(.*)$ /clean/$1? permanent;
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_rewrite_module Documentation](https://nginx.org/en/docs/http/ngx_http_rewrite_module.html)
- [Official NGINX ngx_http_map_module Documentation](https://nginx.org/en/docs/http/ngx_http_map_module.html)
- [NGINX Official Wiki: If is Evil](https://www.nginx.com/resources/wiki/start/topics/depth/ifisevil/)
- [Alphabetical List of Built-in NGINX Variables](https://nginx.org/en/docs/varindex.html)

---

[← Previous: TLS & HTTPS Termination](03-tls-https.md) | [Module Overview](README.md) | [Next: WebSockets and SSE Proxying →](05-websockets-and-sse-proxying.md)
