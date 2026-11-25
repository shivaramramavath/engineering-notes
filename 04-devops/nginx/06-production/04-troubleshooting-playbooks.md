# Troubleshooting

## Concept
A repeatable method for finding why nginx is misbehaving, plus a lookup of the errors you will meet most. Work from the outside in: is nginx running, is the config what you think it is, what do the logs say, and is the problem nginx or the backend?

**Prerequisites:** [logging.md](../02-core-features/logging.md), [request-lifecycle.md](../04-internals/request-lifecycle.md)

## Diagnostic Workflow

1. **Is the config valid?** `sudo nginx -t`
2. **Is it running and listening?** `systemctl status nginx` and `sudo ss -ltnp | grep nginx`
3. **Is it the config you think it is?** `sudo nginx -T | less` (the full merged config, including every include)
4. **What do the logs say?** `sudo tail -f /var/log/nginx/error.log` while reproducing the problem
5. **Reproduce with curl,** bypassing the browser (caches, extensions, redirects): `curl -v ...`
6. **Isolate nginx from the backend.** Call the backend directly: `curl -v http://127.0.0.1:3000/path`
7. **Narrow the cause** with the tables below, then change **one** thing and retest.

## Quick Diagnostic Commands
```bash
sudo nginx -t                                   # syntax and file checks
sudo nginx -T                                   # dump the effective config
nginx -V 2>&1 | tr ' ' '\n' | grep -E "^--"     # build flags and modules, paths
sudo ss -ltnp | grep -E ':80|:443'              # who is listening
curl -vI http://example.com/                    # headers, redirect chain, status
curl -v --resolve example.com:443:203.0.113.10 https://example.com/   # test a specific server
curl -H "Host: example.com" http://127.0.0.1/   # test virtual host selection locally
sudo journalctl -u nginx --since "10 min ago"   # startup and reload failures
namei -l /var/www/example/index.html            # permissions along the whole path
ps -o pid,user,cmd -C nginx                     # which user workers run as
```

## Reading the Error Log
Format: `date level pid#tid: *connection message, client: ..., server: ..., request: "...", upstream: "...", host: "..."`

Use the `client`, `server`, `request`, and `upstream` fields to match the log line to the failing request, and the connection number (`*123`) to correlate lines belonging to the same request.

## Status Codes: Symptom to Cause

| Status | Typical meaning in nginx | First things to check |
|---|---|---|
| **301/302 loop** | Redirect loop | `X-Forwarded-Proto` handling, HTTP to HTTPS redirect repeated by the backend, `rewrite` rules |
| **400** | Bad request | Oversized cookies or headers, malformed request, HTTP sent to an HTTPS port ("plain HTTP request was sent to HTTPS port") |
| **401** | Auth required | `auth_basic`, `auth_request` result, backend auth |
| **403** | Forbidden | File permissions, missing index with `autoindex off`, `deny` rules, SELinux |
| **404** | Not found | Wrong `root`/`alias`, location matched unexpectedly, `try_files`, missing file |
| **413** | Body too large | Raise `client_max_body_size` |
| **414 / 431** | URI or header too long | Raise `large_client_header_buffers` |
| **421** | Misdirected request | HTTP/2 connection reuse across hosts with a mismatched certificate or `server_name` |
| **429** | Rate limited | `limit_req` / `limit_conn` thresholds, wrong client IP behind a proxy |
| **444** | Connection closed by nginx | A catch-all `return 444`, intentional drop |
| **499** | Client closed connection | Client timed out or navigated away. Usually backend slowness |
| **500** | Internal error | Rewrite loop, backend error, config issue, disk full. Read the error log |
| **502** | Bad gateway | Backend unreachable or invalid response (below) |
| **503** | Unavailable | Rate or connection limit, maintenance flag, no live upstreams |
| **504** | Gateway timeout | Backend too slow (below) |

## 502 Bad Gateway

nginx contacted the upstream but could not get a valid response. The error log message tells you which case:

| Error log message | Cause | Fix |
|---|---|---|
| `connect() failed (111: Connection refused) while connecting to upstream` | Backend not running, wrong port or address | Start the backend, fix `proxy_pass`, check `ss -ltnp` |
| `connect() to unix:/run/app.sock failed (2: No such file or directory)` | Socket missing | Check the backend service and socket path |
| `connect() to unix:... failed (13: Permission denied)` | nginx worker user cannot access the socket | Fix socket ownership or group, or the directory permissions |
| `upstream prematurely closed connection while reading response header` | Backend crashed, restarted, or closed the connection | Check backend logs, memory, and timeouts |
| `recv() failed (104: Connection reset by peer)` | Backend reset the connection | Backend crash, keepalive mismatch (backend closes idle connections sooner than nginx expects) |
| `upstream sent too big header while reading response header from upstream` | Response headers exceed buffer | Raise `proxy_buffer_size` (and `proxy_buffers`), reduce cookie or header size |
| `no live upstreams while connecting to upstream` | All servers marked failed | Check `max_fails` / `fail_timeout` and backend health |
| `host not found in upstream` | DNS name does not resolve | Fix DNS. Note nginx resolves names at startup and reload |
| `SSL_do_handshake() failed ... while SSL handshaking to upstream` | TLS mismatch with HTTPS backend | Check `proxy_ssl_*` settings, SNI (`proxy_ssl_server_name on;`), protocols |

**SELinux (RHEL, Rocky, Fedora):** a 502 with `Permission denied` even though Unix permissions look right is often SELinux.
```bash
getenforce
sudo ausearch -m avc -ts recent | tail
sudo setsebool -P httpd_can_network_connect 1     # allow nginx to connect to network backends
sudo restorecon -Rv /var/www/example              # fix file labels
```

## 504 Gateway Timeout
The backend did not answer within the timeout (`upstream timed out (110: Connection timed out)`).
- Backend is slow or overloaded. Compare `$upstream_response_time` in logs.
- Long jobs legitimately need more time: raise `proxy_read_timeout` for that location only.
- `proxy_connect_timeout` firing means the backend is unreachable or its accept queue is full.
- A firewall or security group dropping traffic to the backend looks the same.

Raising timeouts hides a slow backend. Fix the cause where possible.

## 403 Forbidden
```
directory index of "/var/www/example/" is forbidden
open() "/var/www/example/index.html" failed (13: Permission denied)
access forbidden by rule
```
- **Directory index forbidden:** no `index` file found and `autoindex` is off. Check file name and `index` directive.
- **Permission denied:** the worker user needs read on the file and execute (`x`) on **every parent directory**. Use `namei -l <path>`. A home directory (`/home/user`) often blocks access.
- **Access forbidden by rule:** an `allow`/`deny` matched. Check client IP, especially behind a proxy ([security-hardening.md](../05-security-performance/security-hardening.md)).
- SELinux labels: `ls -Z`, `restorecon`.

## 404 Not Found
```
open() "/var/www/example/missing" failed (2: No such file or directory)
```
The log line shows the **exact path nginx tried**. Compare it with your expectation:
- `root` appends the URI, while `alias` replaces the location prefix ([static-files.md](../01-fundamentals/static-files.md)).
- Another `location` (often a regex) matched first ([server-and-location.md](../01-fundamentals/server-and-location.md)).
- SPA routes need `try_files $uri $uri/ /index.html;`.
- Request reached the wrong `server` block because of `Host` or `default_server`.

## Startup and Reload Errors

| Message | Cause and fix |
|---|---|
| `unknown directive "x"` | Typo, or the module that provides it is not built or loaded (`nginx -V`) |
| `directive "x" is not allowed here` | Wrong context ([config-structure.md](../01-fundamentals/config-structure.md)) |
| `unexpected "}"` / `unexpected end of file` | Missing semicolon or unbalanced braces |
| `bind() to 0.0.0.0:80 failed (98: Address already in use)` | Another process on the port: `sudo ss -ltnp \| grep :80` (Apache, another nginx) |
| `bind() ... failed (13: Permission denied)` | Non-root process binding a port below 1024, or SELinux port labels |
| `cannot load certificate ... No such file or directory` | Wrong path or missing certificate |
| `SSL_CTX_use_PrivateKey_file ... key values mismatch` | Certificate and key do not belong together |
| `conflicting server name "x" on 0.0.0.0:80, ignored` | Duplicate `server_name` on the same listen address: one block is ignored |
| `could not build server_names_hash, you should increase server_names_hash_bucket_size` | Many or long names: raise `server_names_hash_bucket_size` (e.g. to 64 or 128) |
| `no resolver defined to resolve ...` | Variable in `proxy_pass` needs a `resolver` |
| `open() "/run/nginx.pid" failed` | Stale or missing PID file, or nginx not running |

If a reload "does nothing":
- Check `nginx -t` output and `journalctl -u nginx`. The reload may have been rejected.
- Make sure the changed file is actually included (`nginx -T | grep -n "configuration file"`).
- Confirm you edited the right server. In containers, the config may be baked into the image or mounted from elsewhere.
- Browser or CDN caching may be serving the old response: test with `curl`.

## Capacity and Resource Errors

| Message | Cause and fix |
|---|---|
| `worker_connections are not enough` | Raise `worker_connections`, check for connection leaks or very long keepalives |
| `socket() failed (24: Too many open files)` / `accept4() failed (24)` | Raise `worker_rlimit_nofile` and the systemd `LimitNOFILE` ([operations.md](operations.md)) |
| `(11: Resource temporarily unavailable)` | Temporary resource limit or a full socket queue. Look at load and backlog |
| `(28: No space left on device)` | Disk full: logs, cache, or temp directories |
| `an upstream response is buffered to a temporary file` | Informational. Response exceeded proxy buffers ([performance-tuning.md](../05-security-performance/performance-tuning.md)) |
| `client intended to send too large body` | `client_max_body_size` too small |
| `limiting requests, excess: ...` | Rate limiting working as configured |

## Redirect Loops and Wrong Schemes
Symptoms: browser shows "too many redirects", or links point to `http://` on an HTTPS site.
- The app behind nginx thinks the request is HTTP. Pass `X-Forwarded-Proto $scheme` and make the app trust it ([reverse-proxy.md](../02-core-features/reverse-proxy.md)).
- A CDN to nginx to app chain where each layer redirects to HTTPS. Decide which layer redirects.
- `return 301` using `$uri` instead of `$request_uri`.
- `curl -IL` shows the chain and the `Location` header at each step.

## Which Server/Location Handled the Request?
When routing is unclear, add temporary diagnostics:
```nginx
add_header X-Debug-Server   $server_name always;
add_header X-Debug-Upstream $upstream_addr always;
add_header X-Debug-Uri      $uri always;
```
Or use a log format with `$server_name $uri $upstream_addr $request_id`. Remove debug headers afterward, since they leak internal details.

## Debug Logging for One Client
```nginx
events { debug_connection 203.0.113.10; }
error_log /var/log/nginx/debug.log debug;
```
Needs a `--with-debug` binary. It records phases, rewrites, location matching, and upstream selection ([request-lifecycle.md](../04-internals/request-lifecycle.md)). Output is very verbose, so enable it briefly and remove it.

## Network-Level Checks
```bash
sudo ss -ltnp                                    # listening sockets
sudo ss -tan state established | wc -l           # connection count
sudo tcpdump -ni any port 3000 -c 50             # traffic between nginx and backend (if needed)
curl -v telnet://127.0.0.1:3000                  # raw TCP reachability
sudo iptables -L -n | head                       # local firewall (or: sudo nft list ruleset)
```

## Common Mistakes When Troubleshooting
- Changing several things at once, so the actual fix is unknown.
- Testing in a browser with cached redirects (301s are cached aggressively). Use `curl` or a private window.
- Reading the access log only, not the error log, where the real cause usually appears.
- Blaming nginx for a slow or failing backend. Check `$upstream_response_time` first.
- Forgetting that nginx resolves hostnames only at start and reload.
- Debugging the wrong server block, container, or host.
- Leaving debug logging or debug headers enabled.

## Quick Reference: Where to Look

| Symptom | Look at |
|---|---|
| Won't start or reload | `nginx -t`, `journalctl -u nginx` |
| 502 / 504 | Error log `upstream` lines, backend logs, `$upstream_response_time` |
| 403 / 404 | Error log file path, `namei -l`, `root`/`alias`/`location` |
| Wrong site served | `server_name`, `default_server`, `curl -H "Host: ..."` |
| Redirect loop | `curl -IL`, forwarded headers |
| Slow | `curl -w` timings, upstream timing in logs ([performance-tuning.md](../05-security-performance/performance-tuning.md)) |
| Resource errors | `ulimit`, `LimitNOFILE`, `worker_connections`, disk space |

## Related / Next
- [operations.md](operations.md): safe changes, monitoring, rotation
- [logging.md](../02-core-features/logging.md): log formats that make diagnosis faster
- [docker-kubernetes.md](docker-kubernetes.md): troubleshooting inside containers
