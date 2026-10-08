# Logging

## Concept
nginx writes two kinds of logs: the **access log** (one line per request) and the **error log** (problems and diagnostics). They are the first place to look when something breaks.

**Prerequisites:** [config-structure.md](../01-fundamentals/config-structure.md)

## Access Log
```nginx
http {
    log_format main '$remote_addr - $remote_user [$time_local] '
                    '"$request" $status $body_bytes_sent '
                    '"$http_referer" "$http_user_agent" '
                    'rt=$request_time urt=$upstream_response_time';

    access_log /var/log/nginx/access.log main;
}
```
`access_log` can be set at `http`, `server`, or `location` level. Child contexts replace the parent's setting.

### Useful variables to log
| Variable | Why |
|---|---|
| `$request_time` | Total time to serve the request |
| `$upstream_response_time` | Backend time, compare with `$request_time` to find slow backends |
| `$upstream_addr` | Which backend answered |
| `$upstream_cache_status` | HIT / MISS / BYPASS / EXPIRED (see [caching](../03-traffic-management/caching.md)) |
| `$http_x_forwarded_for` | Client IP behind another proxy |
| `$request_id` | Unique ID to correlate with application logs |

## JSON Logs
Structured logs are easier to ship to Elasticsearch, Loki, or similar systems.
```nginx
log_format json escape=json
  '{"time":"$time_iso8601","ip":"$remote_addr","method":"$request_method",'
  '"uri":"$request_uri","status":$status,"bytes":$body_bytes_sent,'
  '"rt":$request_time,"urt":"$upstream_response_time","req_id":"$request_id"}';
access_log /var/log/nginx/access.json json;
```
Quote `$upstream_response_time` because it can be `-` or contain several comma-separated values.

## Conditional and Reduced Logging
```nginx
map $status $loggable { ~^[23] 0; default 1; }     # log only 4xx/5xx
access_log /var/log/nginx/errors-only.log main if=$loggable;

location = /healthz { access_log off; }            # silence health checks
```

## Buffered Logging
```nginx
access_log /var/log/nginx/access.log main buffer=32k flush=5s;
```
Reduces disk writes under heavy traffic, with a small delay before lines appear.

## Error Log
```nginx
error_log /var/log/nginx/error.log warn;
```
Levels from most to least verbose: `debug`, `info`, `notice`, `warn`, `error`, `crit`, `alert`, `emerg`.
- Use `warn` or `error` in production.
- `debug` requires a binary built with `--with-debug` (the official packages include it), and is very verbose. Use it briefly, for specific clients:
```nginx
events { debug_connection 203.0.113.10; }
error_log /var/log/nginx/debug.log debug;
```

## Common Error Log Messages

| Message | Likely cause |
|---|---|
| `connect() failed (111: Connection refused) while connecting to upstream` | Backend not running or wrong port |
| `upstream timed out (110)` | Backend slower than `proxy_read_timeout` |
| `open() ... failed (13: Permission denied)` | File or directory permissions, or SELinux |
| `open() ... failed (2: No such file or directory)` | Wrong `root` / `alias` path |
| `client intended to send too large body` | Raise `client_max_body_size` |
| `no live upstreams` | All backends marked down |

## Log Rotation
Handled by `logrotate` (installed with the package). After rotation, nginx must reopen files:
```bash
sudo nginx -s reopen          # or: kill -USR1 $(cat /run/nginx.pid)
```
See [operations](../06-production/operations.md).

## Common Mistakes
- Logs fill the disk because rotation is not configured.
- Debugging with `$remote_addr` behind a proxy and seeing only the proxy IP.
- Defining `access_log` in a child context and losing the parent's log destination.
- Logging sensitive data such as tokens in query strings or `Authorization` headers.
- Parsing JSON logs that contain unescaped values (use `escape=json`).

## Quick Reference
```bash
sudo tail -f /var/log/nginx/error.log
sudo awk '{print $9}' /var/log/nginx/access.log | sort | uniq -c | sort -rn   # status counts
sudo grep ' 502 ' /var/log/nginx/access.log | tail
```
(Field positions depend on your `log_format`.)

## Related / Next
- [troubleshooting](../06-production/troubleshooting.md): using logs to diagnose errors
- [load-balancing](../03-traffic-management/load-balancing.md): logging upstream selection
