# Reverse Proxy

## Concept
A reverse proxy sits in front of one or more application servers (Node, Python, Java, PHP-FPM, etc.), receives client requests, forwards them to the backend, and returns the response. nginx handles slow clients, TLS, and static files so the application can focus on application logic.

**Prerequisites:** [server-and-location.md](../01-fundamentals/server-and-location.md)

## Minimal Example
```nginx
server {
    listen 80;
    server_name app.example.com;

    location / {
        proxy_pass http://127.0.0.1:3000;

        proxy_set_header Host              $host;
        proxy_set_header X-Real-IP         $remote_addr;
        proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

## Why the Headers Matter
By default nginx sends `Host: <proxy_pass host>` and hides the client's real IP and protocol from the backend.

| Header | Purpose |
|---|---|
| `Host $host` | Backend sees the original hostname (needed for virtual hosts, redirects, cookies) |
| `X-Real-IP` | Client IP as seen by nginx |
| `X-Forwarded-For` | Chain of client and proxy IPs |
| `X-Forwarded-Proto` | Original scheme (`http` or `https`), needed when TLS terminates at nginx |

Backends must be configured to trust these headers only from your proxy.

## URI Handling with `proxy_pass`
```nginx
location /api/ {
    proxy_pass http://backend;     # /api/users  ->  /api/users
}
location /api/ {
    proxy_pass http://backend/;    # /api/users  ->  /users   (prefix replaced)
}
```
A `proxy_pass` with **any URI part** (even just `/`) replaces the matched location prefix. Without a URI part, the original request URI is passed unchanged. With regex locations or variables in `proxy_pass`, the rules differ, so test the result.

## Timeouts
```nginx
proxy_connect_timeout 5s;     # establish connection to backend
proxy_send_timeout    60s;    # between two writes to backend
proxy_read_timeout    60s;    # between two reads from backend
```
Defaults are 60s. Long-running requests (reports, uploads) need a higher `proxy_read_timeout`. A 504 usually means this timeout fired.

## Buffering
```nginx
proxy_buffering on;           # default: buffer backend response, free the backend quickly
proxy_buffering off;          # for streaming responses (SSE, long polling)
```

## WebSocket Support
WebSockets require the HTTP/1.1 upgrade handshake to be forwarded explicitly.

```nginx
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

location /ws/ {
    proxy_pass http://127.0.0.1:3000;
    proxy_http_version 1.1;
    proxy_set_header Upgrade    $http_upgrade;
    proxy_set_header Connection $connection_upgrade;
    proxy_read_timeout 1h;    # keep idle sockets open
}
```

## Proxying to Multiple Backends
Point `proxy_pass` at an `upstream` group to balance across servers: see [load-balancing](../03-traffic-management/load-balancing.md). Keepalive connections to the upstream require `proxy_http_version 1.1;` and `proxy_set_header Connection "";`.

## Common Mistakes
- **Trailing slash surprises** in `proxy_pass` changing the forwarded path.
- **Missing `Host` header**, so the backend generates wrong redirects or fails virtual-host matching.
- **Backend redirects to the wrong host/scheme:** configure `proxy_redirect` or fix forwarded headers and the app's trusted-proxy setting.
- **502 Bad Gateway:** backend down, wrong port, or socket permission issue.
- **504 Gateway Timeout:** backend slower than `proxy_read_timeout`.
- **413 Request Entity Too Large:** raise `client_max_body_size` (default 1m).
- **Re-defining `proxy_set_header` in a child block**, which drops all inherited ones (see [config-structure.md](../01-fundamentals/config-structure.md)).

## Debugging
```bash
curl -v http://app.example.com/            # check status and headers
sudo tail -f /var/log/nginx/error.log      # "connect() failed", "upstream timed out"
sudo ss -ltnp | grep 3000                  # is the backend listening?
```
More in [troubleshooting](../06-production/troubleshooting.md).

## Security Notes
- Never expose backend ports publicly. Bind them to `127.0.0.1` or a private network.
- Strip or overwrite client-supplied `X-Forwarded-*` headers if nginx is your outermost proxy.

## Related / Next
- [tls-https.md](tls-https.md): terminating HTTPS at the proxy
- [caching](../03-traffic-management/caching.md): caching proxied responses
- [load-balancing](../03-traffic-management/load-balancing.md): multiple backends
