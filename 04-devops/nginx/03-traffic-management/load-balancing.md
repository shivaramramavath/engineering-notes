# Load Balancing

## Concept
nginx distributes requests across a group of backend servers (an **upstream**) for scalability and availability. It also provides passive failover when a backend stops responding.

**Prerequisites:** [reverse-proxy.md](../02-core-features/reverse-proxy.md)

## Minimal Example
```nginx
http {
    upstream app {
        server 10.0.0.11:3000;
        server 10.0.0.12:3000;
        server 10.0.0.13:3000;
    }

    server {
        listen 80;
        location / {
            proxy_pass http://app;
            proxy_set_header Host $host;
        }
    }
}
```
The `upstream` block lives in the `http` context. `proxy_pass` refers to it by name, with no trailing URI unless you intend prefix replacement.

## Balancing Methods

| Method | Directive | Behavior | Use when |
|---|---|---|---|
| Round robin | (default) | Rotates through servers, honoring weights | Stateless backends of similar capacity |
| Least connections | `least_conn;` | Sends to the server with fewest active connections | Requests vary a lot in duration |
| IP hash | `ip_hash;` | Same client IP goes to the same server | Simple session affinity |
| Generic hash | `hash $request_uri consistent;` | Hash on any key; `consistent` limits remapping when servers change | Cache locality, sharded backends |
| Random | `random two least_conn;` | Picks two at random, then the least busy | Several nginx instances sharing backends |

```nginx
upstream app {
    least_conn;
    server 10.0.0.11:3000 weight=3;
    server 10.0.0.12:3000;
}
```

## Server Parameters

| Parameter | Meaning |
|---|---|
| `weight=N` | Relative share of traffic (default 1) |
| `max_fails=N` | Failures within `fail_timeout` before marking the server unavailable (default 1) |
| `fail_timeout=T` | Window for counting failures **and** how long the server stays marked down (default 10s) |
| `backup` | Used only when all primary servers are unavailable |
| `down` | Permanently excluded (useful during maintenance) |
| `max_conns=N` | Cap on concurrent connections to this server |

```nginx
upstream app {
    server 10.0.0.11:3000 max_fails=3 fail_timeout=30s;
    server 10.0.0.12:3000 max_fails=3 fail_timeout=30s;
    server 10.0.0.99:3000 backup;
}
```

## Failover Behavior
Open-source nginx uses **passive health checks**: a server is marked failed only after real client requests fail. What counts as failure is controlled by:

```nginx
proxy_next_upstream error timeout http_502 http_503 http_504;
proxy_next_upstream_tries 2;
proxy_next_upstream_timeout 10s;
```
Important: by default nginx does **not** retry non-idempotent requests (POST, etc.) after they have been sent. Adding `non_idempotent` to `proxy_next_upstream` allows it, but can duplicate side effects such as double charges. Only enable it if the backend is idempotent.

Active health checks (periodic probes) are an NGINX Plus feature. With open source nginx, use an external checker, a service-discovery layer, or a load balancer in front.

## Keepalive to Backends
Reusing connections reduces latency and CPU on both sides.
```nginx
upstream app {
    server 10.0.0.11:3000;
    keepalive 32;                     # idle connections cached per worker
}
location / {
    proxy_pass http://app;
    proxy_http_version 1.1;
    proxy_set_header Connection "";   # required for upstream keepalive
}
```

## Session Persistence
- `ip_hash` breaks down behind NAT (many users share one IP) or when nginx itself sits behind another proxy (it sees the proxy's IP).
- Cookie-based sticky sessions are an NGINX Plus feature. Open-source alternatives: `hash $cookie_sessionid consistent;`, or better, make the application stateless (shared session store such as Redis).

## Dynamic Backends and DNS
Hostnames in `upstream` or `proxy_pass` are resolved **once at startup/reload**. If a backend's IP changes (cloud, Kubernetes), nginx keeps using the old one. Options:
- Reload nginx when targets change.
- Use a variable with a `resolver` (resolved at runtime, but loses upstream group features):
```nginx
resolver 10.0.0.2 valid=30s;
set $backend "http://api.internal:3000";
proxy_pass $backend;
```
- Use a service mesh or an ingress controller that manages this for you.

## Common Mistakes
- Expecting active health checks in open-source nginx.
- Setting `max_fails=0` (disables failure tracking) and wondering why dead backends still receive traffic.
- Missing `proxy_http_version 1.1` and `Connection ""` when using `keepalive`.
- Using `ip_hash` behind a CDN or corporate NAT.
- Retrying non-idempotent requests blindly.
- Stale DNS for hostnames in `upstream`.

## Debugging
Log which backend served each request:
```nginx
log_format lb '$remote_addr "$request" $status upstream=$upstream_addr urt=$upstream_response_time';
```
A comma-separated `$upstream_addr` means nginx retried on another server. Test distribution with a loop:
```bash
for i in $(seq 1 10); do curl -s http://localhost/whoami; done
```

## Production Notes
- Run at least two nginx instances (with a floating IP or an external load balancer) to avoid a single point of failure.
- Combine with `limit_conn` / `max_conns` to protect fragile backends. See [rate-limiting.md](rate-limiting.md).
- Drain a server by marking it `down` and reloading, then wait for in-flight requests to complete.

## Related / Next
- [caching.md](caching.md): reducing load before it reaches backends
- [rate-limiting.md](rate-limiting.md): protecting backends from bursts
- [architecture](../04-internals/architecture.md): how workers share upstream state
