# Caching

## Concept
nginx can store backend responses on disk and serve repeated requests without contacting the backend. This lowers latency and backend load, and can keep serving stale content when backends fail.

**Prerequisites:** [reverse-proxy.md](../02-core-features/reverse-proxy.md), [logging.md](../02-core-features/logging.md)

## Minimal Setup
```nginx
http {
    proxy_cache_path /var/cache/nginx/app
                     levels=1:2
                     keys_zone=app_cache:10m
                     max_size=1g
                     inactive=60m
                     use_temp_path=off;

    server {
        location / {
            proxy_cache       app_cache;
            proxy_cache_valid 200 301 10m;
            proxy_cache_valid 404       1m;
            add_header X-Cache-Status $upstream_cache_status always;
            proxy_pass http://app;
        }
    }
}
```

| `proxy_cache_path` parameter | Meaning |
|---|---|
| `levels=1:2` | Two-level directory hashing to avoid huge directories |
| `keys_zone=name:size` | Shared memory for keys and metadata (about 8,000 keys per MB) |
| `max_size` | Disk limit. The cache manager evicts least-recently-used entries beyond it |
| `inactive` | Remove entries not accessed within this time, regardless of validity |

The cache directory must be writable by the nginx worker user.

## What Gets Cached by Default
- Only **GET and HEAD** responses.
- Only status codes listed in `proxy_cache_valid` (or when the backend sends `Cache-Control` / `Expires`; those headers are honored if `proxy_cache_valid` is absent).
- Responses with `Set-Cookie`, `Cache-Control: private/no-cache/no-store`, or `Vary: *` are **not** cached unless you explicitly ignore those headers with `proxy_ignore_headers`.

## Cache Key
Default: `$scheme$proxy_host$request_uri`.
```nginx
proxy_cache_key "$scheme$request_method$host$request_uri";
```
Include everything that changes the response (host, language header, device type), otherwise users can receive the wrong content. Leaving out `$args` or `$request_uri` details causes collisions.

## Bypass and No-Store
```nginx
map $http_cookie $skip_cache {
    default 0;
    ~*session_id 1;          # logged-in users
}

location / {
    proxy_cache_bypass $skip_cache;   # don't serve from cache
    proxy_no_cache     $skip_cache;   # don't save response
}
```
Use both. Bypass alone still stores the response, which could then be served to other users.

## Serving Stale Content
```nginx
proxy_cache_use_stale error timeout updating http_500 http_502 http_503 http_504;
proxy_cache_background_update on;     # refresh in background, serve stale meanwhile
proxy_cache_lock on;                  # one request populates, others wait
proxy_cache_lock_timeout 5s;
```
- `proxy_cache_lock` prevents a **cache stampede** (many identical requests hitting the backend at once on a miss).
- `updating` + `background_update` give users fast responses while content refreshes.

## Cache Status Values (`$upstream_cache_status`)

| Value | Meaning |
|---|---|
| `MISS` | Not in cache, fetched from backend |
| `HIT` | Served from cache |
| `EXPIRED` | Entry was stale, fetched again |
| `STALE` | Served stale due to `use_stale` |
| `UPDATING` | Stale served while another request refreshes |
| `REVALIDATED` | Stale entry confirmed valid via conditional request |
| `BYPASS` | Cache skipped by `proxy_cache_bypass` |

## Invalidation (Purging)
Open-source nginx has **no built-in purge**. Options:
- Use short TTLs and `proxy_cache_use_stale`.
- Vary the key with a version (e.g. a header or path prefix incremented on deploy).
- Delete cache files manually: they are named by the MD5 of the key.
- Third-party purge module, or NGINX Plus `proxy_cache_purge`.
- Cache at a CDN that provides purge APIs.

## FastCGI, uwsgi, and Static Variants
The same model exists for `fastcgi_cache` (PHP-FPM), `uwsgi_cache`, and `scgi_cache`, with matching `*_cache_path`, `*_cache_key`, `*_cache_valid` directives.

## Browser Caching vs Proxy Caching
`expires` and `Cache-Control` headers control **clients**. `proxy_cache` controls nginx's **server-side** cache. Use `proxy_hide_header` / `add_header` deliberately so you don't confuse the two.

## Common Mistakes
- Caching personalized responses (login pages, dashboards, anything with cookies) and leaking them between users.
- Using `proxy_cache_bypass` without `proxy_no_cache`.
- Cache key missing `$host`, mixing responses from different virtual hosts that share a cache zone.
- `keys_zone` too small: entries evicted early even though disk has space.
- Wrong directory permissions, so nothing is stored and `X-Cache-Status` is always `MISS`.
- Backend sends `Set-Cookie` or `Cache-Control: no-cache`, so nothing is cached, and no one notices.
- Forgetting there is no purge, then serving stale prices or content for hours.

## Debugging
```bash
curl -sI http://example.com/page | grep -i x-cache-status     # run twice: MISS then HIT
sudo ls -R /var/cache/nginx/app | head
sudo grep -a "KEY:" -r /var/cache/nginx/app | head            # see stored keys
```
If it never becomes `HIT`, check backend response headers and the error log.

## Performance and Production Notes
- Put the cache on fast local disk. Ideally a separate volume so a full cache can't fill the OS disk.
- Set `max_size` below the disk capacity with headroom.
- Cache only what is safe: public, read-mostly content. Monitor hit ratio via `$upstream_cache_status` in logs.
- Use `slice` for caching large files in chunks, if you serve big downloads.

## Related / Next
- [load-balancing.md](load-balancing.md): distributing cache misses
- [performance-tuning](../05-security-performance/performance-tuning.md): further optimization
- [security-hardening](../05-security-performance/security-hardening.md): avoiding cache poisoning and data leaks
