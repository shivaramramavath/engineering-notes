---
title: "Interview Prep: Location Matching, Rewrites & URI Resolution"
description: "Master technical interview questions, rigorous evaluation algorithms, rewrite flag semantics, and real-world debugging scenarios for NGINX location matching and rewrites."
module: "08-interview"
topic: "02-location-matching-and-rewrites-qa"
status: "completed"
tags: ["nginx", "interview", "location-matching", "rewrites", "regex", "if-is-evil", "internal-redirects", "qa"]
prerequisites: ["01-fundamentals/03-server-and-location.md", "02-core-features/04-variables-and-rewrites.md"]
estimated_read_time: "35 min"
---

# Interview Prep: Location Matching, Rewrites & URI Resolution

```
+-------------------------------------------------------------------------------+
| NGINX Location Matching Priority Algorithm (Order of Precedence)              |
|                                                                               |
|  Incoming Request URI: e.g. "GET /images/logo.png"                            |
|        │                                                                      |
|        ▼                                                                      |
|  1. Exact Match (`=`):                                                        |
|     - Does an exact `location = /images/logo.png` exist?                     |
|     - YES: Terminate search immediately! Execute this block.                  |
|     - NO:  Proceed to Step 2.                                                 |
|        │                                                                      |
|        ▼                                                                      |
|  2. Longest Prefix String Match:                                              |
|     - Scan all prefix locations (e.g. `/`, `/images`, `/images/logo.png`).    |
|     - Remember the LONGEST matching prefix.                                   |
|     - Did the longest prefix carry the `^~` modifier?                         |
|       - YES: Terminate search! Do NOT check regular expressions.             |
|       - NO:  Remember prefix and proceed to Step 3.                           |
|        │                                                                      |
|        ▼                                                                      |
|  3. Regular Expressions (`~` case-sensitive, `~*` case-insensitive):          |
|     - Evaluate regex locations in FILE ORDER (top to bottom).                 |
|     - First matching regex wins! Terminate search immediately.                |
|        │                                                                      |
|        ├── MATCH FOUND: Execute that regex block.                             |
|        └── NO MATCH: Fall back to the remembered longest prefix from Step 2!  |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [Location Matching Algorithm & Priority Questions](#1-location-matching-algorithm--priority-questions)
2. [Rewrite Directive & Flag Semantics](#2-rewrite-directive--flag-semantics)
3. [The "If is Evil" Rule & Safe Conditional Alternatives](#3-the-if-is-evil-rule--safe-conditional-alternatives)
4. [Internal Redirects & Phase Looping](#4-internal-redirects--phase-looping)
5. [Live Technical Scenarios & Debugging Challenges](#5-live-technical-scenarios--debugging-challenges)
6. [Summary & Quick Reference](#6-summary--quick-reference)

---

## 1. Location Matching Algorithm & Priority Questions

<details>
<summary><strong>Q1: What is the exact step-by-step algorithm NGINX uses to select a `location` block?</strong></summary>

**Model Answer:**
1. **Exact Match (`=`)**: NGINX checks all exact match locations. If the URI matches identically, NGINX halts search and executes this block immediately.
2. **Longest Prefix Search**: NGINX evaluates all prefix locations (without modifiers or with `^~`). It stores the **longest matching prefix string** in memory.
3. **The `^~` Check**: If the longest matching prefix carries the `^~` (non-regex) modifier, NGINX immediately stops searching and selects this block, completely skipping regex evaluation.
4. **Regular Expression Search (`~` and `~*`)**: NGINX tests regex locations in the **sequential order they appear in the configuration file**. The very first regex that matches the URI is selected immediately.
5. **Fallback to Longest Prefix**: If no regular expression matches (or if none are configured), NGINX falls back to the remembered longest prefix location found in Step 2.

*Summary Rule*: Prefixes are evaluated by **length**; regular expressions are evaluated by **file order**.
</details>

<details>
<summary><strong>Q2: What is the difference between `location ~` and `location ~*`?</strong></summary>

**Model Answer:**
- `location ~ <regex>`: Specifies a **case-sensitive** regular expression match (e.g. `location ~ \.PNG$` will match `.PNG` but NOT `.png`).
- `location ~* <regex>`: Specifies a **case-insensitive** regular expression match (e.g. `location ~* \.png$` matches `.png`, `.PNG`, `.Png`).
</details>

<details>
<summary><strong>Q3: Given the following configuration, which location block handles `GET /documents/report.pdf`?</strong></summary>

```nginx
location / {
    return 200 "Block A\n";
}

location /documents/ {
    return 200 "Block B\n";
}

location ~* \.pdf$ {
    return 200 "Block C\n";
}

location ^~ /documents/ {
    return 200 "Block D\n";
}
```

**Model Answer:**
**Block D** handles the request.
*Reasoning*:
1. `/documents/` is the longest matching prefix string.
2. Block D defines `^~ /documents/`.
3. The `^~` modifier tells NGINX: *"If this prefix is the longest match, do not evaluate any regular expressions."*
4. Therefore, Block C (`~* \.pdf$`) is completely skipped, and Block D executes.
*(Note: In a real config with duplicate paths, NGINX emits a configuration syntax warning; if `^~` is present on the longest prefix, it supersedes all regexes).*
</details>

---

## 2. Rewrite Directive & Flag Semantics

<details>
<summary><strong>Q4: Explain the differences between the four `rewrite` flags: `last`, `break`, `redirect`, and `permanent`.</strong></summary>

**Model Answer:**
The `rewrite` directive syntax is `rewrite <regex> <replacement> [flag];`.
- **`last`**:
  - Stops processing current rewrite directives in the current location.
  - Starts a **new search for a matching location** based on the newly rewritten URI (triggers an internal redirect loop cycle).
  - Used when the rewritten URI belongs to another location block.
- **`break`**:
  - Stops processing rewrite directives in the current block.
  - **Does NOT initiate a new location search**. Execution continues in the current location block using the rewritten URI.
  - Used when preparing a URI for `proxy_pass` or local file reads within the same location.
- **`redirect`**:
  - Emits an external **HTTP 302 Found** (temporary redirect) response directly to the client browser.
- **`permanent`**:
  - Emits an external **HTTP 301 Moved Permanently** response directly to the client browser. Browsers cache this redirect locally.
</details>

<details>
<summary><strong>Q5: What happens if a rewrite directive inside a location causes an infinite loop with `last`?</strong></summary>

**Model Answer:**
If a `rewrite ... last;` continually modifies the URI into a pattern that re-matches the same rewrite rule, NGINX tracks the internal redirect count.
NGINX enforces a hard internal limit of **10 internal redirect cycles**.
When the counter exceeds 10, NGINX halts execution and returns **HTTP 500 Internal Server Error**, logging `rewrite or internal redirection cycle while processing` to `/var/log/nginx/error.log`.
</details>

---

## 3. The "If is Evil" Rule & Safe Conditional Alternatives

<details>
<summary><strong>Q6: Why is using `if` inside a `location` block considered evil ("If is Evil")? What happens internally?</strong></summary>

**Model Answer:**
In procedural programming, `if` statements execute conditionally in sequence. In NGINX, `if` directives inside a `location` block do NOT execute procedurally; instead, they create an **invisible, nested internal location**.

*The Problem*:
When the `if` condition evaluates to true, NGINX moves request execution into this hidden internal location. In this internal location:
1. Content-phase handlers assigned in the outer location (such as `proxy_pass`, `fastcgi_pass`, or `try_files`) are often not inherited.
2. Request memory structures may remain uninitialized.
3. This triggers unpredictable behavior: silent 404s, ignored proxy directives, or segmentation faults (`SIGSEGV` crashes).

*The Rule of Thumb*:
The ONLY directives that are 100% safe inside an `if` block inside a `location` are:
- `return ...` (e.g. `return 403;` or `return 301 ...;`)
- `rewrite ... last;`
Both directives immediately terminate further phase processing, avoiding the uninitialized state trap.
For all other conditional logic, use declarative `map` blocks in the `http {}` context.
</details>

<details>
<summary><strong>Q7: How do you refactor an unsafe `if` block using `map`?</strong></summary>

**Model Answer:**
*Unsafe Pattern*:
```nginx
location /download/ {
    # UNSAFE: If creates internal location; proxy_pass fails!
    if ($http_user_agent ~* "badbot") {
        set $backend "blocked";
    }
    proxy_pass http://$backend;
}
```

*Safe Refactoring using `map` in `http {}`*:
```nginx
http {
    map $http_user_agent $is_bad_bot {
        default                 0;
        "~*badbot"              1;
        "~*scrapedata"          1;
    }

    server {
        location /download/ {
            if ($is_bad_bot) {
                return 403 "Access Denied\n"; # Safe: return terminates immediately!
            }
            proxy_pass http://backend_pool;
        }
    }
}
```
</details>

---

## 4. Internal Redirects & Phase Looping

<details>
<summary><strong>Q8: What is an "Internal Location" and how is it secured from external client requests?</strong></summary>

**Model Answer:**
An internal location is a location block that can **only be accessed via internal NGINX routing mechanisms**—such as `error_page`, `try_files`, `rewrite ... last`, or subrequests (`auth_request`, `mirror`).

It is declared using the `internal;` directive or named locations (`@name`):
```nginx
location = /custom_502.html {
    internal;
    root /var/www/errors;
}

location @fallback {
    proxy_pass http://backup_cluster;
}
```
If an external client sends a direct HTTP request to `https://example.com/custom_502.html`, NGINX immediately rejects the request with an **HTTP 404 Not Found**, ensuring internal processing logic cannot be invoked directly.
</details>

---

## 5. Live Technical Scenarios & Debugging Challenges

<details>
<summary><strong>Scenario 1: You need to redirect `http://example.com/blog/2026/01/post-name` to `https://example.com/posts/post-name` while preserving query parameters. How do you write this?</strong></summary>

**Solution:**
```nginx
location ^~ /blog/ {
    # Match /blog/YYYY/MM/title and capture title in $1
    # Note: query parameters ($args) are automatically appended by rewrite!
    rewrite ^/blog/\d{4}/\d{2}/(.*)$ https://$host/posts/$1 permanent;
}
```
*Why this is optimal*:
1. Uses `^~ /blog/` so regex locations elsewhere cannot hijack the blog path.
2. `permanent` returns an HTTP 301 redirect.
3. In NGINX, `rewrite` automatically appends query arguments (`$is_args$args`) to the replacement URL unless the replacement URL ends with a question mark (`?`).
</details>

<details>
<summary><strong>Scenario 2: An engineer wrote `return 301 https://$host$uri;` to enforce HTTPS, but security scans report CRLF Injection vulnerabilities. What is wrong?</strong></summary>

**Diagnosis & Fix:**
`$uri` is a normalized variable. NGINX decodes percent-encoded characters (like `%0d%0a`, representing `\r\n`).
If an attacker sends `GET /%0d%0aInjected-Header:1`, `$uri` expands to literal carriage return and line feed characters, injecting arbitrary headers into the response.
*Fix*: Replace `$uri` with `$request_uri`:
```nginx
return 301 https://$host$request_uri;
```
`$request_uri` contains the raw, un-decoded client request line, preserving percent-encoding and preventing CRLF injection.
</details>

---

## 6. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| Location & Rewrite Rules of Thumb                                              |
|                                                                                |
| Feature                 | Evaluation / Behavioral Rule                         |
| :---------------------- | :--------------------------------------------------- |
| Exact `=`               | Checked first; highest priority; instant exit.       |
| Longest Prefix          | Checked second; remembers longest string match.      |
| Prefix with `^~`        | If longest match has ^~, SKIP ALL REGEXES!           |
| Regexes `~` and `~*`    | Evaluated in FILE ORDER; first match wins.           |
| Fallback Prefix         | Used only if NO regex matches.                       |
| `rewrite ... last;`     | Re-starts location search with new URI.              |
| `rewrite ... break;`    | Continues in current block; no location re-search.   |
| If is Evil              | Only safe with `return` or `rewrite ... last;`.      |
| Redirect Variables      | Always use $request_uri, NEVER $uri in redirects.    |
+--------------------------------------------------------------------------------+
```

---

[← Previous Topic: 01-fundamentals-qa.md](01-fundamentals-qa.md) | [Back to Module Index](README.md) | [Next Topic: 03-reverse-proxy-and-load-balancing-qa.md →](03-reverse-proxy-and-load-balancing-qa.md)
