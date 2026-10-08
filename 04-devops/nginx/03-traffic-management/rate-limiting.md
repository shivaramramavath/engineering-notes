# Rate Limiting

## Concept
Rate limiting caps how fast (or how many concurrent) requests a client may make. It protects backends from overload, slows brute-force attempts, and reduces abuse. nginx offers two modules: `limit_req` (requests per time) and `limit_conn` (simultaneous connections).

**Prerequisites:** [server-and-location.md](../01-fundamentals/server-and-location.md), [variables-and-rewrites.md](../02-core-features/variables-and-rewrites.md)

## Request Rate: `limit_req`
```nginx
http {
    limit_req_zone $binary_remote_addr zone=perip:10m rate=10r/s;
    limit_req_status 429;

    server {
        location /api/ {
            limit_req zone=perip burst=20 nodelay;
            proxy_pass http://app;
        }
    }
}
```
- `limit_req_zone key zone=name:size rate=...` defines the counter. It lives in `http`.
- `$binary_remote_addr` is a compact form of the client IP (about 64 bytes per state; 1 MB holds roughly 16,000 IPs).
- `rate=10r/s` is internally tracked in milliseconds, so it means about one request per 100 ms, not 10 at once.
- `limit_req_status 429` returns "Too Many Requests" (default is 503).

## How `burst` and `nodelay` Work
nginx uses a leaky bucket.

| Configuration | Behavior |
|---|---|
| No `burst` | Any request arriving faster than the rate is rejected immediately |
| `burst=20` | Up to 20 excess requests are queued and released at the configured rate (adds delay) |
| `burst=20 nodelay` | Up to 20 excess requests are served immediately, but the burst slots refill only at the configured rate. Anything beyond is rejected |
| `burst=20 delay=8` | First 8 excess requests served immediately, the rest delayed (nginx 1.15.7+) |

Use `nodelay` for APIs where latency matters; use plain `burst` to smooth traffic to a fragile backend.

## Concurrent Connections: `limit_conn`
```nginx
limit_conn_zone $binary_remote_addr zone=conn_perip:10m;
limit_conn_status 429;

location /downloads/ {
    limit_conn conn_perip 4;           # max 4 simultaneous connections per IP
    limit_rate 500k;                   # bandwidth cap per connection
}
```
`limit_conn` counts a connection only while a request is being processed.

## Choosing the Key
The key decides who is limited.

| Key | Use for |
|---|---|
| `$binary_remote_addr` | Per client IP (default choice) |
| `$server_name` | Total limit for a virtual host |
| `$http_authorization` / `$cookie_session` | Per user or token, if identifiable |
| `$request_uri` | Protect expensive endpoints |

**Behind a proxy or CDN**, `$remote_addr` is the proxy's IP, so everyone shares one bucket. Restore the real client IP with the `realip` module:
```nginx
set_real_ip_from 10.0.0.0/8;          # trusted proxy range only
real_ip_header   X-Forwarded-For;
real_ip_recursive on;
```
Trust only addresses you control. Otherwise clients can spoof the header and evade limits.

## Exemptions and Tiers
An empty key is **not limited**, which lets you skip trusted clients with `map`:
```nginx
geo $limited {
    default        1;
    10.0.0.0/8     0;      # internal traffic
    203.0.113.5    0;      # monitoring
}
map $limited $limit_key {
    0 "";
    1 $binary_remote_addr;
}
limit_req_zone $limit_key zone=perip:10m rate=10r/s;
```

## Multiple Limits
Several `limit_req` directives can apply together; the strictest wins.
```nginx
location /login {
    limit_req zone=perip   burst=5;
    limit_req zone=perhost burst=50;
}
```

## Logging and Dry Run
```nginx
limit_req_log_level warn;        # default is error
limit_req_dry_run on;            # nginx 1.17.1+: count and log, but don't reject
```
Use `dry_run` to measure impact before enforcing.

## Typical Production Setup
- Strict limit on `/login`, password reset, and signup endpoints.
- Moderate limit on the general API.
- No limit (or a high limit) on static assets.
- `limit_conn` on download endpoints.
- A custom 429 page or JSON body:
```nginx
error_page 429 = @too_many;
location @too_many {
    default_type application/json;
    return 429 '{"error":"rate_limited"}';
}
```
Consider also sending a `Retry-After` header with `add_header Retry-After 1 always;`.

## Common Mistakes
- Limiting by `$remote_addr` behind a proxy and blocking all users together.
- Zone too small: when memory is exhausted, nginx evicts old states and may return errors for new ones.
- Forgetting `burst`, so normal page loads (many parallel requests) get rejected.
- Using `nodelay` without a `burst` (it has no effect).
- Trusting `X-Forwarded-For` from the internet.
- Applying one global limit to both HTML pages and assets, so one page view uses up the allowance.
- Expecting this to stop a real DDoS. It cannot, since traffic still reaches nginx and consumes bandwidth and connections.

## Debugging
```bash
# Send 30 quick requests and count statuses
for i in $(seq 1 30); do curl -s -o /dev/null -w "%{http_code}\n" http://localhost/api/; done | sort | uniq -c
```
Check the error log for lines like `limiting requests, excess: 5.200 by zone "perip"`. Add `$limit_req_status` (nginx 1.17.6+) to your access log to see PASSED / DELAYED / REJECTED.

## Security Notes
Rate limiting slows brute-force and scraping but is one layer. Combine with strong authentication, fail2ban or WAF rules for repeated offenders, and upstream DDoS protection. See [security-hardening](../05-security-performance/security-hardening.md).

## Related / Next
- [load-balancing.md](load-balancing.md): `max_conns` for backend protection
- [logging.md](../02-core-features/logging.md): recording limit decisions
- [security-hardening](../05-security-performance/security-hardening.md)
