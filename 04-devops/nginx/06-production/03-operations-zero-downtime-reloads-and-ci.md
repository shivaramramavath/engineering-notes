# Operations

## Concept
Day-to-day running of nginx in production: changing config safely, reloading without downtime, rotating logs, monitoring, renewing certificates, upgrading, and handling maintenance windows. The goal is that every change is **tested, reversible, and invisible to users**.

**Prerequisites:** [architecture.md](../04-internals/architecture.md), [logging.md](../02-core-features/logging.md)

## Safe Change Workflow

```
edit config (in version control) → nginx -t → reload → verify → (rollback if needed)
```

```bash
sudo nginx -t                         # 1. validate syntax and referenced files
sudo systemctl reload nginx           # 2. graceful reload (or: sudo nginx -s reload)
curl -sI https://example.com | head   # 3. verify the site responds as expected
sudo tail -n 50 /var/log/nginx/error.log   # 4. check for new errors
```

Rules:
- **Never reload without `nginx -t` first.** A failed reload keeps the old config running, but a failed `restart` can take the site down.
- Prefer `reload` over `restart`. Restart drops connections, while reload does not.
- Keep `/etc/nginx` in git (or generate it from a config-management tool). Every change then has an author, a diff, and a revert.
- Review diffs of the **full merged config** when includes are involved: `nginx -T > merged.conf`.

### Rollback
```bash
cd /etc/nginx && sudo git revert <commit>      # or restore the previous file
sudo nginx -t && sudo systemctl reload nginx
```
Keep the last known-good config available on the server, not only in a remote repo.

## Service Management (systemd)
```bash
sudo systemctl enable --now nginx
sudo systemctl status nginx
sudo journalctl -u nginx --since "1 hour ago"
sudo systemctl edit nginx            # create an override file (survives package upgrades)
```
Useful overrides:
```ini
# /etc/systemd/system/nginx.service.d/override.conf
[Service]
LimitNOFILE=65535
Restart=on-failure
RestartSec=2s
```
After editing, run `sudo systemctl daemon-reload && sudo systemctl restart nginx`. File-descriptor limits changed this way need a restart, not a reload.

## Reload vs Restart vs Upgrade

| Action | Command | Drops connections? | Use for |
|---|---|---|---|
| Reload | `nginx -s reload` | No | Config changes, new certificates |
| Reopen logs | `nginx -s reopen` | No | After log rotation |
| Graceful stop | `nginx -s quit` | No (drains) | Planned shutdown |
| Restart | `systemctl restart nginx` | **Yes** | Changing `systemd` limits, swapping the nginx binary when you are not doing a USR2 upgrade |
| Binary upgrade | `USR2` + `WINCH` + `QUIT` | No | Upgrading the nginx binary with zero downtime |

Details of each signal are in [architecture.md](../04-internals/architecture.md). A reload re-reads nginx's own config only. It does **not** apply changes outside it, such as systemd or OS limits, or a newly installed nginx binary.

Long-lived connections (WebSockets, downloads) keep old workers alive after a reload. Limit this with:
```nginx
worker_shutdown_timeout 60s;
```

## Log Rotation
Debian and RHEL packages install a logrotate rule. A typical one:
```
/var/log/nginx/*.log {
    daily
    rotate 14
    compress
    delaycompress
    missingok
    notifempty
    create 0640 www-data adm
    sharedscripts
    postrotate
        [ -f /run/nginx.pid ] && kill -USR1 $(cat /run/nginx.pid)
    endscript
}
```
The `USR1` signal makes nginx reopen its log files. Without it, nginx keeps writing to the renamed (rotated) file and the new file stays empty. Verify with `ls -l /var/log/nginx` after a forced rotation: `sudo logrotate -f /etc/logrotate.d/nginx`.

Also consider shipping logs off the machine (Filebeat, Vector, Promtail, or journald/syslog: `access_log syslog:server=...`), and monitoring disk usage of `/var/log` and the cache directory.

## Monitoring

### `stub_status`: basic live metrics
```nginx
server {
    listen 127.0.0.1:8080;
    location = /nginx_status {
        stub_status;
        allow 127.0.0.1;
        deny  all;
        access_log off;
    }
}
```
Example output:
```
Active connections: 291
server accepts handled requests
 16630948 16630948 31070465
Reading: 6 Writing: 179 Waiting: 106
```

| Field | Meaning |
|---|---|
| Active connections | Open connections, including idle keepalive |
| accepts / handled | Connections accepted / successfully handled. They should be equal; `handled < accepts` means resource limits (`worker_connections`, file descriptors) were hit |
| requests | Total requests (can exceed connections due to keepalive) |
| Reading | Workers reading request headers |
| Writing | Workers sending responses or waiting on upstreams |
| Waiting | Idle keepalive connections |

Check the module exists: `nginx -V 2>&1 | grep -o with-http_stub_status_module`. Never expose this endpoint publicly.

### What to monitor and alert on

| Signal | Source | Alert when |
|---|---|---|
| 5xx rate | Access log / metrics | Above baseline for several minutes |
| 502/504 rate | Access log | Any sustained increase (backend problem) |
| `$request_time` / `$upstream_response_time` p95 | Access log | Latency regression |
| Connections vs limit | `stub_status` | Active connections approaching `worker_processes × worker_connections` |
| `accepts` ≠ `handled` | `stub_status` | Any difference |
| Disk usage (logs, cache, temp) | OS | Above ~80% |
| Certificate expiry | External probe | Fewer than 14 days left |
| Process health | systemd / orchestrator | nginx not running or restarting |
| Error log patterns | Log shipper | `emerg`, `crit`, "Too many open files", "no live upstreams" |

Tooling options: the official **nginx Prometheus exporter** (reads `stub_status`), a log-based pipeline (Loki, ELK, Vector) for per-status and latency metrics from JSON logs, or your cloud provider's monitoring. See [logging.md](../02-core-features/logging.md) for log formats that include upstream timing.

### Health endpoints
```nginx
location = /healthz { access_log off; return 200 "ok\n"; }
```
This reports that nginx itself is up. A deeper check should also probe a backend path, so decide which one your load balancer needs.

## Certificate Renewal
With Certbot, renewal runs on a timer. nginx must **reload** to use new certificates:
```bash
sudo certbot renew --deploy-hook "systemctl reload nginx"
sudo certbot renew --dry-run
sudo systemctl list-timers | grep certbot
```
Monitor expiry externally, since a silently failing renewal is a common outage:
```bash
echo | openssl s_client -connect example.com:443 -servername example.com 2>/dev/null | openssl x509 -noout -enddate
```

## Upgrading nginx

### Package upgrade (most common)
```bash
sudo apt update && apt list --upgradable | grep nginx
sudo apt install --only-upgrade nginx      # or dnf upgrade nginx
sudo nginx -t && sudo systemctl restart nginx    # restart picks up the new binary
```
Read the changelog for changed defaults and removed directives. Test the upgrade in staging with your real config.

### Zero-downtime binary upgrade
1. Install the new binary (keep the old one for rollback).
2. `sudo kill -USR2 $(cat /run/nginx.pid)`: starts a new master and workers; the old PID file becomes `nginx.pid.oldbin`.
3. `sudo kill -WINCH $(cat /run/nginx.pid.oldbin)`: old workers drain and exit; the old master stays for rollback.
4. Verify traffic and logs.
5. Finish: `sudo kill -QUIT $(cat /run/nginx.pid.oldbin)`.
   Roll back instead: `kill -HUP` the old master, then `kill -QUIT` the new master.

Behind a load balancer, rolling restarts one node at a time is often simpler and safer.

## Maintenance Windows and Deployments

### Maintenance page
```nginx
server {
    if (-f /etc/nginx/maintenance.on) { return 503; }
    error_page 503 /maintenance.html;

    location = /maintenance.html {
        root /var/www/errors;
        internal;
        add_header Retry-After 3600 always;
    }
}
```
Enable with `touch /etc/nginx/maintenance.on` and reload. Disable by deleting the flag and reloading. Returning **503** (not 200) tells search engines the outage is temporary.

### Draining a backend
Mark the server `down` in the `upstream`, reload, wait for in-flight requests to finish, deploy, then re-enable it. See [load-balancing.md](../03-traffic-management/load-balancing.md).

### Blue/green or canary
Switch between two upstream groups by changing one `proxy_pass` or using `split_clients` / `map` for a percentage canary, then reload.

## Backups and Disaster Recovery
Back up: `/etc/nginx`, TLS certificates and keys (securely), custom error pages and static assets, and the service overrides. Test that a fresh server can be rebuilt from your automation (Ansible, Terraform, a container image) and that `nginx -t` passes there.

## Capacity Planning
- Track peak connections, requests per second, bandwidth, and CPU per worker over time.
- Load-test before traffic events. See [performance-tuning.md](../05-security-performance/performance-tuning.md).
- Plan redundancy: at least two nginx nodes behind a floating IP or an external load balancer.

## Common Mistakes
- Reloading without `nginx -t`, or testing a different file than the one nginx loads.
- Using `restart` for routine changes.
- Log rotation without `USR1` / `reopen`, so logs vanish into deleted files.
- Letting the disk fill with logs or cache, which makes nginx return 500s or fail to write temp files.
- Forgetting to reload after certificate renewal.
- Exposing `stub_status` publicly.
- Making direct edits on servers that automation later overwrites.
- No external certificate or uptime monitoring.

## Debugging Operations Problems
```bash
sudo nginx -t                                  # why won't it reload?
sudo journalctl -u nginx -n 100 --no-pager     # service-level errors
ps -ef | grep "nginx: worker process is shutting down"   # old workers still draining
sudo lsof -p $(cat /run/nginx.pid) | grep -c .           # open files held by master
df -h /var/log /var/cache/nginx                # disk pressure
```

## Related / Next
- [troubleshooting.md](troubleshooting.md): diagnosing errors and outages
- [docker-kubernetes.md](docker-kubernetes.md): the same operations in containers
- [architecture.md](../04-internals/architecture.md): how reload and signals work
