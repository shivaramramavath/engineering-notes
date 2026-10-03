# Security Checklist

A practical audit list you can run before launch and revisit every quarter. Each section links back to the chapter that explains the "why" and the "how".

```
Network ─► Transport ─► Identity ─► Permissions ─► Host ─► Application ─► Monitoring
```

## 1. Network

| Check | Why |
|-------|-----|
| Redis is **not** reachable from the public internet | The single most common cause of Redis compromises |
| Runs in a private subnet or VPC, with security groups or firewall rules allowing only known clients | Shrinks the attack surface |
| `bind` lists specific private interfaces, not `0.0.0.0` | Limits where it listens |
| `protected-mode yes` | Refuses outside connections when no auth is configured |
| Changing the port is **not** counted as security | Scanners probe every port |
| Docker ports published as `127.0.0.1:6379:6379`, not `6379:6379` | Docker's published ports can bypass host firewall rules such as `ufw` |
| Cluster bus port (client port + 10000) restricted to cluster nodes | Node-to-node traffic must not be open to clients |
| Sentinel port (26379) restricted like Redis itself | Sentinel is an attack target too |

```bash
# redis.conf
bind 10.0.1.15 127.0.0.1
protected-mode yes
```

```yaml
# docker-compose.yml
ports:
  - "127.0.0.1:6379:6379"
```

## 2. Transport

- [ ] TLS enabled for clients (`tls-port`), plain-text `port 0` once migrated
- [ ] `tls-replication yes` and `tls-cluster yes` where applicable
- [ ] `tls-protocols "TLSv1.2 TLSv1.3"`
- [ ] Clients verify certificates (no `rejectUnauthorized: false`)
- [ ] Certificates have correct SANs and an expiry alert
- [ ] Mutual TLS considered for service-to-service traffic

See [TLS](./02_tls.md).

## 3. Identity

- [ ] ACL users in place, one per service
- [ ] `default` user disabled (or at minimum password-protected and restricted)
- [ ] Long random secrets (`ACL GENPASS`), never reused across environments
- [ ] Users stored in an `aclfile` with hashed passwords (`#<sha256>`)
- [ ] Rotation procedure documented and tested (add new password, roll out, remove old)
- [ ] Replicas, Sentinel and Cluster nodes configured with matching credentials

See [Authentication and ACL](./01_authentication-and-acl.md).

## 4. Permissions

- [ ] Application users limited to their key patterns (`~app:*`)
- [ ] Pub/Sub channel patterns scoped (`&app:events:*`)
- [ ] `@dangerous` and `@admin` denied for application users
- [ ] Separate read-only user for reporting and dashboards
- [ ] Admin user used only by humans or ops tooling, not applications
- [ ] Scripting (`EVAL`, `FUNCTION`) allowed only for users that need it

Commands worth denying for application users:

| Command | Risk |
|---------|------|
| `FLUSHALL`, `FLUSHDB` | Wipes data |
| `KEYS` | O(N), blocks the server |
| `CONFIG`, `DEBUG`, `SHUTDOWN` | Changes or stops the server |
| `REPLICAOF` / `SLAVEOF` | Redirects replication |
| `MODULE`, `SCRIPT`, `FUNCTION` | Runs or loads code |
| `MONITOR` | Streams every command, including secrets, and costs performance |
| `ACL` | Changes permissions |

`CONFIG SET dir` and `CONFIG SET dbfilename` combined with `SAVE` let a connected attacker write files wherever the Redis user can write. Denying `CONFIG` for app users and running Redis as an unprivileged user closes that path.

## 5. Host and container

- [ ] Redis runs as a dedicated **non-root** user
- [ ] Config file readable only by that user (`600` or `640`)
- [ ] Data directory (RDB and AOF files) mode `700`
- [ ] Disk or volume **encryption at rest** (open-source Redis does not encrypt its data files itself)
- [ ] Backups encrypted, access-controlled and tested for restore
- [ ] Containers: non-root user, read-only root filesystem where possible, no `--privileged`, no host networking
- [ ] OS patched, with unnecessary services disabled
- [ ] systemd hardening options considered (`NoNewPrivileges`, `ProtectSystem`, `PrivateTmp`)

```ini
# /etc/systemd/system/redis.service.d/hardening.conf
[Service]
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=full
ProtectHome=yes
ReadWritePaths=/var/lib/redis /var/log/redis
```

## 6. Resource limits

A secure server also survives misuse and bugs.

```bash
maxmemory 2gb
maxmemory-policy allkeys-lru          # or noeviction for primary data
maxclients 10000
timeout 300                           # close idle clients
tcp-keepalive 300
client-output-buffer-limit pubsub 32mb 8mb 60
```

- [ ] `maxmemory` and an eviction policy set (see [Memory and Eviction](../02_redis-fundamentals/05_memory-and-eviction.md))
- [ ] `maxclients` sized for your fleet
- [ ] Output buffer limits for Pub/Sub and replicas
- [ ] `slowlog` enabled for visibility

## 7. Application code

### Keys built from user input

User input inside key names or patterns can reach other users' data or cause expensive scans.

```ts
// Risky: attacker controls the whole key suffix
const key = `session:${req.query.id}`;       // id = "admin" or "*" or "a:b:c"
await redis.keys(`user:${req.query.q}*`);    // glob injection, plus KEYS blocks the server
```

Validate and encode input, and build keys in one place (see [Redis Key Builder](../08_nodejs-integration/05_redis-key-builder.md)):

```ts
const SAFE_ID = /^[A-Za-z0-9_-]{1,64}$/;

function key(...parts: (string | number)[]): string {
  return parts
    .map((p) => {
      const s = String(p);
      if (!SAFE_ID.test(s)) throw new Error("Invalid key segment");
      return s;
    })
    .join(":");
}

const sessionKey = (id: string) => key("session", id);
```

For search by prefix, use `SCAN` with a **fixed** pattern prefix and escape glob characters from user text:

```ts
const escapeGlob = (s: string) => s.replace(/[\\*?\[\]]/g, "\\$&");
```

### Commands built from user input

```ts
// DON'T: user chooses the command or arguments freely
await redis.call(req.body.command, ...req.body.args);
```

Only call named, fixed commands. If you must accept a limited set, map them from an allow-list.

### Lua scripts

- Pass user data as `KEYS` and `ARGV`, never concatenate it into script text
- Declare every key in `KEYS` so Cluster routing and ACL key checks work
- Keep scripts in code, reviewed like any other source file

```ts
// Good: data goes through ARGV
await redis.eval(`return redis.call("GET", KEYS[1])`, 1, key("user", id));
```

### Session and token values

- Generate session IDs with a cryptographically secure source, not `Math.random()`
- Set a TTL on every session, and rotate IDs after login

```ts
import { randomBytes } from "node:crypto";
const sid = randomBytes(32).toString("base64url");
await redis.set(key("session", sid), JSON.stringify({ userId }), "EX", 1800);
```

See [Session Management](../13_session-management/01_session-storage.md).

### Sensitive data

- Do not store plain-text passwords, card numbers or secrets in Redis
- If you must cache sensitive fields, encrypt them in the application with a key that is **not** stored in Redis
- Set short TTLs on anything sensitive

### Deserialization

- Validate everything read from Redis against a schema
- Do not use unsafe deserializers on data other systems can write (see [Serialization](../16_performance/04_serialization.md))

## 8. Secrets handling

- [ ] Credentials come from environment variables or a secret manager, not source code
- [ ] `.env` files and certificate keys are in `.gitignore`
- [ ] CI logs and error trackers never print connection strings
- [ ] Different secrets per environment (dev, staging, production)
- [ ] Secrets rotated on a schedule and on staff changes

Redact credentials before logging a URL:

```ts
function redactUrl(url: string): string {
  try {
    const u = new URL(url);
    if (u.password) u.password = "***";
    if (u.username) u.username = "***";
    return u.toString();
  } catch {
    return "[invalid url]";
  }
}

console.log("Connecting to", redactUrl(process.env.REDIS_URL!));
```

## 9. Monitoring and detection

| Signal | Where | Alert when |
|--------|-------|------------|
| Failed authentication | `INFO stats` (`acl_access_denied_auth` on recent versions), `ACL LOG` | Count rises quickly |
| Denied commands or keys | `ACL LOG`, `acl_access_denied_cmd` / `_key` | Unexpected spikes |
| Unknown client IPs | `CLIENT LIST` | An address outside your fleet appears |
| Connected clients | `INFO clients` | Approaching `maxclients` |
| Memory | `INFO memory` | Approaching `maxmemory` |
| Slow commands | `SLOWLOG GET` | `KEYS`, `FLUSH*` or large O(N) calls |
| Certificate expiry | External check | Under 30 days |

```ts
const entries = await redis.call("ACL", "LOG", "20");
const clients = await redis.client("LIST");
const slow = await redis.call("SLOWLOG", "GET", "10");
```

Avoid running `MONITOR` in production: it streams every command and its arguments, which exposes secrets and slows the server.

See [Monitoring and Logging](../19_redis-production/02_monitoring-and-logging.md).

## 10. Updates and vulnerabilities

- [ ] Run a supported Redis version and plan upgrades
- [ ] Follow Redis security advisories and release notes
- [ ] Patch promptly. Scripting-related vulnerabilities have been disclosed in the past, so restrict `EVAL` and `FUNCTION` to users that need them
- [ ] Update client libraries (ioredis) and Node.js
- [ ] Run dependency scanning in CI

## Minimal hardened config

```bash
# network
bind 10.0.1.15 127.0.0.1
protected-mode yes
port 0
tls-port 6379
tls-cert-file /etc/redis/tls/redis.crt
tls-key-file  /etc/redis/tls/redis.key
tls-ca-cert-file /etc/redis/tls/ca.crt
tls-protocols "TLSv1.2 TLSv1.3"
tls-replication yes
tls-cluster yes

# identity
aclfile /etc/redis/users.acl

# resources
maxmemory 2gb
maxmemory-policy allkeys-lru
maxclients 10000
timeout 300
tcp-keepalive 300

# persistence on a protected directory
dir /var/lib/redis
```

## Audit script

A quick sanity check you can run in CI or on a schedule. Managed services often block `CONFIG`, so treat a failure to read a setting as "check manually", not as "pass".

```ts
import Redis from "ioredis";

async function cfg(redis: Redis, name: string): Promise<string | null> {
  try {
    const res = (await redis.config("GET", name)) as string[];
    return res[1] ?? null;
  } catch {
    return null; // CONFIG blocked or denied
  }
}

export async function audit(redis: Redis) {
  const findings: string[] = [];

  const bind = await cfg(redis, "bind");
  if (bind && /(^|\s)(0\.0\.0\.0|\*)(\s|$)/.test(bind)) findings.push("bind includes 0.0.0.0");

  if ((await cfg(redis, "protected-mode")) === "no") findings.push("protected-mode is off");
  if ((await cfg(redis, "maxmemory")) === "0") findings.push("maxmemory is unlimited");

  const port = await cfg(redis, "port");
  const tlsPort = await cfg(redis, "tls-port");
  if (port !== "0" && (!tlsPort || tlsPort === "0")) findings.push("TLS not enabled");
  if (port && port !== "0" && tlsPort && tlsPort !== "0") findings.push("plain-text port still open");

  try {
    const users = (await redis.call("ACL", "LIST")) as string[];
    for (const u of users) {
      if (/^user default on\b/.test(u) && u.includes("nopass")) {
        findings.push("default user is enabled without a password");
      }
      if (/\bon\b/.test(u) && u.includes("nopass") && !u.startsWith("user default")) {
        findings.push(`user without password: ${u.split(" ")[1]}`);
      }
    }
  } catch {
    findings.push("could not read ACL LIST (check manually)");
  }

  return findings;   // empty array means nothing flagged
}
```

## Pre-launch sign-off

```
Network
[ ] Not internet-reachable; bind and firewall verified from outside
[ ] Docker ports bound to localhost or a private interface

Transport
[ ] TLS on client, replication and cluster links; plain-text port off
[ ] Certificates verified by clients; expiry alert in place

Identity and permissions
[ ] ACL users per service; default user off
[ ] No @dangerous / @admin for application users
[ ] Secrets in a manager; rotation tested

Host
[ ] Non-root process; data and config permissions tight
[ ] Disk encryption and encrypted backups

Application
[ ] User input never forms raw keys, patterns, commands or script text
[ ] Session IDs from crypto.randomBytes; TTLs set
[ ] Values validated on read; no secrets in logs

Operations
[ ] maxmemory, maxclients, output buffer limits set
[ ] Alerts on auth failures, denied commands, memory, cert expiry
[ ] Update and advisory process owned by a named team
```

## Common mistakes

| Mistake | Consequence | Fix |
|---------|-------------|-----|
| Exposing 6379 to the internet | Full compromise, data loss or ransom | Private network, firewall, bind, TLS, ACL |
| Docker `-p 6379:6379` on a public host | Bypasses host firewall rules | Bind to `127.0.0.1` or a private IP |
| Everything uses one admin password | One leak equals total access | Per-service ACL users |
| `rejectUnauthorized: false` | Man-in-the-middle possible | Fix CA and SAN setup |
| App user can run `CONFIG` or `FLUSHALL` | Easy escalation or accidental wipe | Deny `@dangerous` and `@admin` |
| User input in keys or `KEYS` patterns | Data leakage, server stalls | Validate, escape, use `SCAN` |
| Credentials in logs or git | Leaked access | Secret manager, redaction |
| No `maxmemory` | Out-of-memory crash | Always set a limit |
| No monitoring on auth failures | Attacks go unnoticed | Alert on `ACL LOG` and stats |
| Skipping updates | Known vulnerabilities stay open | Patch regularly |

## Key takeaways

- Layer your defenses: private network, TLS, ACL users, least privilege, host hardening and careful application code
- The default Redis setup is for trusted environments. Production needs deliberate hardening
- Deny dangerous commands for applications, and never expose Redis publicly
- Treat user input, secrets and deserialization as application-side risks that Redis cannot fix for you
- Monitor failed auth and denied commands, and patch on a schedule
- Re-run the checklist after every infrastructure change

**Previous:** [TLS](./02_tls.md) | **Next:** [Testing with Redis](../18_testing-and-debugging/01_testing-with-redis.md)
