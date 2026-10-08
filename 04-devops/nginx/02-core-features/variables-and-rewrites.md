# Variables, `map`, `return`, and `rewrite`

## Concept
nginx exposes request data as **variables** (`$host`, `$uri`, ...) and offers a few tools to redirect or rewrite requests. Choosing the right tool avoids fragile configs.

**Prerequisites:** [server-and-location.md](../01-fundamentals/server-and-location.md)

## Built-in Variables

| Variable | Meaning |
|---|---|
| `$host` | Host from the request line or `Host` header (lowercased, no port) |
| `$request_uri` | Original URI **with** query string, unmodified |
| `$uri` | Current URI, **normalized**, without query string; changes after rewrites |
| `$args` / `$arg_name` | Query string / one named parameter |
| `$scheme` | `http` or `https` |
| `$remote_addr` | Client IP |
| `$request_method` | GET, POST, ... |
| `$http_<name>` | Any request header (`$http_user_agent`, `$http_upgrade`) |
| `$upstream_addr`, `$upstream_response_time` | Backend details (useful in logs) |
| `$status`, `$body_bytes_sent` | Response details (available at log time) |

Define your own with `set $var value;` or, preferably, `map`.

## `return`: the Preferred Redirect Tool
```nginx
return 301 https://example.com$request_uri;     # permanent redirect
return 302 /maintenance.html;                    # temporary
return 403;
return 200 "ok\n";                               # quick health endpoint
```
`return` stops processing immediately and is the cheapest, clearest option.

## `map`: Conditional Values Without `if`
`map` lives in the `http` context and computes a variable lazily.

```nginx
map $http_user_agent $is_bot {
    default       0;
    ~*bot|crawler 1;
}

map $uri $new_uri {
    /old-page     /new-page;
    /old-blog     /blog;
}

server {
    if ($new_uri) { return 301 $new_uri; }   # one of the safe uses of if
}
```
Redirect tables, per-client rate-limit keys, and the WebSocket `Connection` header all use `map`.

## `rewrite`
```nginx
rewrite ^/old/(.*)$ /new/$1 permanent;     # 301 redirect
rewrite ^/api/v1/(.*)$ /$1 break;          # internal rewrite, stop processing
rewrite ^/legacy/(.*)$ /app/$1 last;       # internal rewrite, re-run location matching
```

| Flag | Effect |
|---|---|
| `last` | Stop rewrite directives, search for a new matching location |
| `break` | Stop rewrite directives, continue in the current location |
| `redirect` | 302 response |
| `permanent` | 301 response |

Rules of thumb:
- If you only need a redirect, use `return` instead of `rewrite`.
- For regex captures in redirects, `rewrite ... permanent` is fine.
- Excessive `last` can create loops (nginx stops after 10 internal redirects with a 500).

## Why `if` Is "Evil"
`if` inside `location` is implemented as a pseudo-location and behaves unexpectedly with many directives.

**Generally safe inside `if`:** `return` and `rewrite ... last/break`.
**Avoid:** `proxy_pass`, `add_header`, `try_files`, and most other directives.

Alternatives:
- Route by URL: separate `location` blocks
- Route by header/value: `map` plus `return` or a variable in `proxy_pass`
- File existence: `try_files`
- Host canonicalization: separate `server` blocks

## Common Patterns
```nginx
# Canonical host: www -> apex
server { server_name www.example.com; return 301 https://example.com$request_uri; }

# Strip trailing slash
rewrite ^/(.*)/$ /$1 permanent;

# Maintenance mode
if (-f /var/www/maintenance.flag) { return 503; }
```

## Common Mistakes
- Using `$uri` in a redirect instead of `$request_uri` (loses the query string, and is decoded and normalized).
- Writing a regex `rewrite` for a simple fixed redirect.
- Using `if` for logic that `map` or `try_files` handles.
- Infinite redirect loops (HTTP to HTTPS to HTTP) caused by missing `X-Forwarded-Proto` handling at a proxy layer.
- Defining `map` inside `server` (it must be in `http`).

## Debugging
- `curl -I` to see redirect status and `Location`.
- Enable `rewrite_log on;` to write rewrite steps to the error log at `notice` level:
```nginx
error_log /var/log/nginx/error.log notice;
rewrite_log on;
```

## Performance Note
`map` variables are evaluated only when used, and `return` is faster than `rewrite`.

## Related / Next
- [logging.md](logging.md): log the variables you define
- [request-lifecycle](../04-internals/request-lifecycle.md): where rewrite fits in processing phases
