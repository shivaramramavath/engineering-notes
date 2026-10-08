# Request Lifecycle

## Concept
Inside a worker, every HTTP request passes through a fixed series of **phases**. Each directive belongs to a phase, and phases always run in the same order, **regardless of the order directives appear in your config**. Knowing the phases explains behaviors like "why does my `rewrite` run before my `allow`?" and "why did `limit_req` not apply to this redirect?".

**Prerequisites:** [server-and-location.md](../01-fundamentals/server-and-location.md), [variables-and-rewrites.md](../02-core-features/variables-and-rewrites.md), [architecture.md](architecture.md)

## Overview

```
Client connects → TLS handshake (if ssl) → read request line + headers
        │
        ▼
 select server (listen + Host / SNI)
        │
        ▼
 POST_READ ► SERVER_REWRITE ► FIND_CONFIG (choose location)
        ▼
 REWRITE ► POST_REWRITE ──(URI changed? loop back to FIND_CONFIG)
        ▼
 PREACCESS ► ACCESS ► POST_ACCESS
        ▼
 PRECONTENT ► CONTENT (one handler produces the response)
        ▼
 output filters (gzip, headers, chunking, ...) → send to client
        ▼
 LOG
```

## The Phases

| # | Phase | What runs here | Typical directives / modules |
|---|---|---|---|
| 1 | POST_READ | After headers are read | `realip` (`set_real_ip_from`, `real_ip_header`) |
| 2 | SERVER_REWRITE | Rewrite directives at **server** level | `rewrite`, `return`, `set`, `if` placed directly in `server {}` |
| 3 | FIND_CONFIG | Selects the `location` | Matching rules from [server-and-location.md](../01-fundamentals/server-and-location.md) |
| 4 | REWRITE | Rewrite directives at **location** level | `rewrite`, `return`, `set`, `if` inside `location {}` |
| 5 | POST_REWRITE | If the URI changed, jump back to phase 3 | internal |
| 6 | PREACCESS | Limits before authentication | `limit_req`, `limit_conn` |
| 7 | ACCESS | Authorization | `allow` / `deny`, `auth_basic`, `auth_request` |
| 8 | POST_ACCESS | Applies `satisfy` logic | `satisfy any` / `all` |
| 9 | PRECONTENT | Preparation before content | `try_files`, `mirror` |
| 10 | CONTENT | **Generates the response** | `proxy_pass`, `fastcgi_pass`, `return` (in some cases), static files, `index`, `autoindex` |
| 11 | LOG | After the response is sent | `access_log` |

After CONTENT, the response passes through **output filters** (gzip, `add_header`, `sub_filter`, chunked encoding, etc.) before reaching the client.

### Practical implications
- **Order in the file does not decide phase order.** `limit_req` always runs before `allow/deny`, even if written after.
- **`rewrite` / `return` / `set` / `if` execute in the order written**, but only among themselves, within their context (server level, then location level).
- **`return` stops processing immediately** in the rewrite phases. Nothing later (access checks, proxying) runs for that request. A `return` at server level beats everything below it.
- **Only one content handler runs per request.** If a location has `proxy_pass`, it handles the response, and `root` there serves no purpose for it. If a location has no special handler, nginx falls back to `index` / `autoindex`, then static file serving.
- **Access control runs before content.** `deny` blocks a request before it reaches the backend.

## Internal Redirects
Some directives restart processing by issuing an **internal redirect** (invisible to the client, URL stays the same):

- `rewrite ... last;`
- `try_files` fallback URI (last argument)
- `error_page 404 /404.html;`
- `index` when a directory is requested
- X-Accel-Redirect response header from a backend

Each internal redirect goes back to **FIND_CONFIG** and selects a location again, so a different location may handle the request, with different `root`, `proxy_pass`, or limits. nginx allows at most **10** internal redirects per request, then returns `500` ("rewrite or internal redirection cycle").

`rewrite ... break;` does **not** restart location search; it continues in the current location with the new URI.

## Subrequests
Modules such as `auth_request`, `ssi`, and `addition` generate **subrequests**, internal requests that go through (most of) the pipeline and return results to the parent request. `auth_request /auth;` makes a subrequest to `/auth`; if it returns 2xx, the parent continues, and 401/403 rejects it. Each subrequest adds latency and backend load.

## Request Body Handling
- For `proxy_pass`, nginx reads the **entire request body** first by default (`proxy_request_buffering on`), buffering it in memory or a temp file, then forwards it to the backend. This protects backends from slow uploads, but delays the start of processing and uses disk.
- `client_max_body_size` is checked early. A too-large body returns `413` without reaching the backend.
- With `proxy_request_buffering off`, the body streams through as it arrives.

## Response Handling
- Backend responses are read into buffers (`proxy_buffering on`) and sent to the client at the client's pace, freeing the backend sooner.
- Filters run on the way out. Directives like `add_header` and `gzip` apply here. Note that `add_header` only applies to success and redirect statuses (200, 201, 204, 206, 301, 302, 303, 304, 307, 308) unless you add `always`, so error responses such as 404 or 502 miss the header otherwise.

## Directive Merging at Runtime
Each location's effective config is the **merge** of `http`, `server`, and `location` settings, computed at load time using inheritance rules (see [config-structure.md](../01-fundamentals/config-structure.md)). For list-like directives (`add_header`, `proxy_set_header`, `error_page`), a lower level that sets any value **replaces** the whole inherited list. This is decided at config time, not per request.

## Worked Example
```nginx
server {
    listen 80;
    server_name example.com;

    set $backend "app";                                 # SERVER_REWRITE
    if ($http_user_agent ~* "badbot") { return 403; }   # SERVER_REWRITE: stops here for bots

    location /api/ {
        rewrite ^/api/v1/(.*)$ /api/$1 break;           # REWRITE: modifies URI, stays here
        limit_req zone=perip burst=20;                  # PREACCESS
        allow 10.0.0.0/8; deny all;                     # ACCESS
        proxy_pass http://app;                          # CONTENT
        access_log /var/log/nginx/api.log;              # LOG
    }
}
```
For a request to `/api/v1/users` from a normal browser: the server-level `if` is skipped, FIND_CONFIG selects `/api/`, `rewrite ... break` changes the URI to `/api/users`, the rate limit and IP check pass, `proxy_pass` forwards it, and the log line is written. A "badbot" request is answered with 403 in step 2, before any location is selected.

## Common Mistakes
- Assuming directives run top to bottom across phases.
- Expecting `limit_req` to apply after a `rewrite ... last` lands in a different location (the new location's limits apply instead).
- Using `rewrite ... last` inside a location and causing a loop.
- Expecting `allow/deny` to protect a path served by a different location after an internal redirect.
- Putting `try_files` and `proxy_pass` fallbacks together in ways that mask 404s.
- Missing `always` on `add_header` for error responses.

## Debugging
```nginx
error_log /var/log/nginx/error.log notice;
rewrite_log on;                          # shows each rewrite step

events { debug_connection 203.0.113.10; }
error_log /var/log/nginx/debug.log debug;   # phase-by-phase trace (needs a --with-debug build)
```
```bash
curl -v -H "Host: example.com" http://127.0.0.1/api/v1/users
sudo nginx -T | sed -n '/server_name example.com/,/^}/p'
```
In `debug` output, search for lines such as `rewrite phase`, `http script`, `test location`, `using configuration`, and `access phase`.

## Quick Reference
**Order:** realip → server rewrite → find location → location rewrite → (loop if URI changed) → limit_conn/limit_req → allow/deny/auth → try_files → content handler → filters → log.

## Related / Next
- [architecture.md](architecture.md): the process model that runs these phases
- [security-hardening](../05-security-performance/security-hardening.md): access control in context
- [troubleshooting](../06-production/troubleshooting.md): applying phase knowledge to real problems
