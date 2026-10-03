# Authentication and ACL

Authentication answers **who are you?** Authorization answers **what may you do?** Redis handles both with **ACL users** (Redis 6.0+). Before that, there was a single shared password. This chapter covers both, but new deployments should use ACLs.

```
client ──AUTH app s3cret──► Redis ──► user "app" ──► may run: GET, SET, ...
                                                   on keys:  app:*
                                                   channels: app:events:*
```

## The legacy way: `requirepass`

```bash
# redis.conf
requirepass a-long-random-secret
```

This sets the password of the built-in `default` user. Everyone who knows it has full access to everything, so it cannot be scoped, audited per service or rotated without downtime. Treat it as a stopgap.

```ts
const redis = new Redis({ host: "redis.internal", password: process.env.REDIS_PASSWORD });
```

## ACL users

Every connection runs as a user. A fresh server has one user, `default`, which has **no password and full access**. That is the first thing to fix.

```bash
redis-cli ACL LIST
# 1) "user default on nopass sanitize-payload ~* &* +@all"
redis-cli ACL WHOAMI
# "default"
```

### Anatomy of a rule

```
user app on >s3cret ~app:* &app:events:* +@read +@write -@dangerous +info
     │   │  │       │       │            │               │          │
     │   │  │       │       │            │               │          └ allow INFO again
     │   │  │       │       │            │               └ deny a whole category
     │   │  │       │       │            └ allow categories of commands
     │   │  │       │       └ allowed Pub/Sub channel patterns
     │   │  │       └ allowed key patterns
     │   │  └ password (stored hashed)
     │   └ enabled
     └ username
```

| Rule | Meaning |
|------|---------|
| `on` / `off` | Enable or disable the user |
| `>password` / `<password` | Add or remove a password. A user can have several |
| `#<sha256hex>` | Add a password by its SHA-256 hash, so no plaintext is stored in files |
| `nopass` | Any password is accepted (or none). Avoid |
| `resetpass` | Remove all passwords |
| `~pattern` | Allow keys matching a glob (`~app:*`) |
| `%R~pattern`, `%W~pattern` | Read-only or write-only key access (Redis 7.0+) |
| `allkeys` or `~*` | All keys |
| `&pattern` | Allow Pub/Sub channels. In Redis 7.0+ the default is no channels |
| `+cmd`, `-cmd` | Allow or deny a command |
| `+cmd\|sub`, `-cmd\|sub` | Allow or deny a subcommand (Redis 7.0+) |
| `+@category`, `-@category` | Allow or deny a command category |
| `+@all`, `-@all` | All commands, or none |
| `reset` | Reset the user to a blank, locked state |

**Order matters.** Rules apply left to right, so `+@all -@dangerous` allows everything except dangerous commands, while `-@dangerous +@all` allows everything.

### Command categories

```bash
redis-cli ACL CAT                 # list categories
redis-cli ACL CAT dangerous       # commands in one category
```

| Category | Examples |
|----------|----------|
| `@read` | `GET`, `HGETALL`, `LRANGE`, `SCAN` |
| `@write` | `SET`, `DEL`, `EXPIRE`, `HSET` |
| `@keyspace` | `DEL`, `EXISTS`, `EXPIRE`, `SCAN` |
| `@transaction` | `MULTI`, `EXEC`, `WATCH` |
| `@pubsub` | `PUBLISH`, `SUBSCRIBE` |
| `@scripting` | `EVAL`, `EVALSHA`, `FUNCTION` |
| `@connection` | `AUTH`, `PING`, `SELECT`, `HELLO` |
| `@slow` | O(N) commands such as `KEYS`, `SMEMBERS` |
| `@dangerous` | `FLUSHALL`, `FLUSHDB`, `KEYS`, `CONFIG`, `DEBUG`, `SHUTDOWN`, `REPLICAOF`, `INFO` |
| `@admin` | `CONFIG`, `SHUTDOWN`, `ACL`, `BGSAVE`, `MONITOR` |

Note that `INFO` sits in `@dangerous`. If you deny that category and your client needs `INFO`, add `+info` afterwards.

## Locking down the default user

Create working users **first**, then restrict `default`, or you will lock yourself out.

```bash
# 1. an administrator you control
redis-cli ACL SETUSER admin on '>a-very-long-admin-secret' '~*' '&*' '+@all'

# 2. an application user
redis-cli ACL SETUSER app on '>a-long-app-secret' '~app:*' \
  +@read +@write +@transaction +@connection -@dangerous +info

# 3. verify, then disable default
redis-cli --user app --pass a-long-app-secret PING
redis-cli ACL SETUSER default off
```

Disabling `default` means any client that does not send a username (including old `requirepass` style clients) fails with `WRONGPASS`. Update every consumer first: apps, workers, monitoring, Sentinel and replicas.

## Persist users: the ACL file

`ACL SETUSER` changes live only in memory. Persist them with an ACL file.

```bash
# redis.conf
aclfile /etc/redis/users.acl
```

```
# /etc/redis/users.acl
user default off
# replace each <sha256> with the hash of a long random secret (never use the hash of a word like "password")
user admin on #<sha256> ~* &* +@all
user app on #<sha256> ~app:* +@read +@write +@transaction +@connection -@dangerous +info
user readonly on #<sha256> ~app:* +@read +@connection -@dangerous +info
```

```bash
echo -n 'my-secret' | sha256sum        # hash a password for the file
redis-cli ACL GENPASS                  # 256-bit random password (64 hex chars)
redis-cli ACL SAVE                     # write in-memory users to the aclfile
redis-cli ACL LOAD                     # reload users from the aclfile
```

Notes:

- Use **either** `aclfile` **or** `user` lines in `redis.conf`, not both
- `ACL SAVE` and `ACL LOAD` work only when `aclfile` is configured
- File permissions should be `600` or `640` and owned by the Redis user, even though passwords are hashed
- Keep the file in version control only if it contains hashes of strong, random secrets. Better: generate it at deploy time from a secret manager

## Least privilege examples

### Cache service: scoped keys, no admin commands

```
user cache on >... ~cache:* +get +set +del +unlink +expire +ttl +mget +scan +info +@connection
```

### Read replica consumer: read-only

```
user reporting on >... ~report:* ~app:* +@read +@connection -@dangerous +info
```

### Split read and write keys (Redis 7.0+)

```
user etl on >... %R~source:* %W~target:* +@read +@write +@connection +info
```

### Pub/Sub participant

```
user notifier on >... &notifications:* +publish +subscribe +psubscribe +unsubscribe +punsubscribe +ping
```

### Queue worker

Libraries like BullMQ use Lua scripts and a prefix (default `bull:`), so a worker needs `~bull:*` and scripting access. Exact command needs vary by version, so start from this and tighten using `ACL LOG` rather than guessing:

```
user worker on >... ~bull:* +@all -@admin -@dangerous +info +client|setname
```

Pub/Sub channel permissions are **not** enabled by default in Redis 7.0+. If a subscriber gets `NOPERM` on a channel, add an `&channel` rule (or `&*`).

## Connecting from ioredis

```ts
import Redis from "ioredis";

const redis = new Redis({
  host: process.env.REDIS_HOST,
  port: Number(process.env.REDIS_PORT ?? 6379),
  username: process.env.REDIS_USER,       // ACL user, omit for the legacy default user
  password: process.env.REDIS_PASSWORD,
});
```

ioredis authenticates on every connect and **re-authenticates after a reconnect** automatically. Or use a URL:

```ts
const redis = new Redis(`redis://app:${encodeURIComponent(pw)}@redis.internal:6379`);
```

Encode special characters in the password, and never log the URL (see [Security Checklist](./03_security-checklist.md)).

### Gotcha: the ready check needs `INFO`

By default ioredis calls `INFO` after connecting to detect when the server has finished loading (`enableReadyCheck: true`). A user without `+info` fails to become ready:

```ts
// Option A (preferred): allow INFO for the user: +info
// Option B: skip the ready check
const redis = new Redis({ username: "app", password: pw, enableReadyCheck: false });
```

Recent ioredis versions also send `CLIENT SETINFO` to report the library name. A strictly locked-down user may log `NOPERM` for it. It is harmless, but you can allow `+client|setinfo` or turn the behavior off with the client-info option your version provides. Check your version's docs.

### Verifying who you are

```ts
const me = await redis.call("ACL", "WHOAMI");   // "app"
```

## Authentication errors

| Error | Meaning | Fix |
|-------|---------|-----|
| `NOAUTH Authentication required.` | Server needs auth, client sent none | Provide `username` and `password` |
| `WRONGPASS invalid username-password pair or user is disabled.` | Bad credentials or the user is `off` | Check secrets and `ACL GETUSER` |
| `NOPERM this user has no permissions to run the 'x' command` | Command denied | Add `+x` or the right category |
| `NOPERM this user has no permissions to access one of the keys used as arguments` | Key outside `~pattern` | Widen the pattern or fix the key |
| `NOPERM ... access one of the channels` | Channel not allowed | Add an `&channel` rule |
| `ERR AUTH <password> called without any password configured` | Sent a password to a no-password server | Remove the password, or enable auth |

```ts
redis.on("error", (err) => {
  if (err.message.startsWith("WRONGPASS") || err.message.startsWith("NOAUTH")) {
    console.error("Redis authentication failed. Check credentials");
  }
});

try {
  await redis.set("other:key", "x");
} catch (err) {
  if (err instanceof Error && err.message.startsWith("NOPERM")) {
    // a permissions bug: log and fix the ACL, do not retry
  }
  throw err;
}
```

Retrying `WRONGPASS` or `NOPERM` never helps and can trigger lockouts in front of proxies, so fail fast.

## ACL log: see what was denied

```bash
redis-cli ACL LOG 10      # last 10 denials
redis-cli ACL LOG RESET
```

```ts
const entries = (await redis.call("ACL", "LOG", "10")) as unknown[];
// each entry holds: reason, context, object, username, age-seconds, client-info
```

Use it while rolling out a new ACL: run in staging, read the log, widen only what the app legitimately needs. In production, alert on a rising count of denials or failed auths (`INFO stats` exposes counters such as `acl_access_denied_auth` on recent versions).

## Rotating a password with zero downtime

A user may hold several passwords at once, which makes rotation a safe three-step process:

```bash
# 1. add the new password (both work now)
redis-cli ACL SETUSER app '>new-secret'

# 2. roll out the new secret to every service, then confirm no client uses the old one

# 3. remove the old password
redis-cli ACL SETUSER app '<old-secret'
redis-cli ACL SAVE
```

Rotate on a schedule and immediately after any suspected leak.

## Passwords and brute force

Redis is fast, which also makes it fast to guess against. There is **no built-in lockout**.

- Use long, random secrets (`ACL GENPASS`), never human-chosen ones
- Keep Redis off untrusted networks so it cannot be guessed at all
- Watch failed-auth counters and `ACL LOG`
- Do not reuse one secret across environments

## Replication, Sentinel and Cluster

Every node authenticates, and users are **not** replicated automatically between nodes.

- Define the same ACL file on every node (Redis does not sync users)
- Replicas need `masteruser` and `masterauth` to authenticate to the primary, and that user needs the replication permissions (`+psync +replconf +ping`)
- Sentinel has its own password for itself and uses `sentinel auth-user` and `sentinel auth-pass` to reach Redis
- In ioredis:

```ts
// Sentinel
new Redis({
  sentinels: [{ host: "s1", port: 26379 }, { host: "s2", port: 26379 }],
  name: "mymaster",
  username: "app",
  password: process.env.REDIS_PASSWORD,            // for Redis nodes
  sentinelPassword: process.env.SENTINEL_PASSWORD, // for Sentinel itself
});

// Cluster
new Redis.Cluster([{ host: "n1", port: 6379 }], {
  redisOptions: { username: "app", password: process.env.REDIS_PASSWORD },
});
```

See [Sentinel](../15_redis-architecture/02_sentinel.md) and [Cluster](../15_redis-architecture/03_cluster.md).

## Managed services

Hosted Redis (cloud providers) usually manage the config for you. Some expose ACLs as users and roles in their console, some offer only a single auth token, and many disable `CONFIG` and `ACL` commands. Read your provider's docs and apply the same principles: separate identities per service, least privilege, TLS, secrets in a manager.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Leaving `default` as `nopass` with full access | Create real users, then `ACL SETUSER default off` |
| Disabling `default` before updating clients | Roll out new credentials first |
| `+@all` for application users | Allow `@read`, `@write` and the specifics, deny `@dangerous` |
| `-@dangerous` breaks the client | Re-add `+info` (ready check) and any needed commands afterwards |
| Wrong rule order | Rules apply left to right |
| Pub/Sub fails with `NOPERM` | Add `&channel` rules (default on 7.0+ is none) |
| `ACL SETUSER` changes lost on restart | `ACL SAVE` with an `aclfile` |
| `aclfile` and `user` lines both configured | Use only one |
| One shared password for all services | One user per service |
| Short or guessable passwords | `ACL GENPASS` and a secret manager |
| Assuming ACLs replicate | Configure every node |
| Retrying on `WRONGPASS` or `NOPERM` | Fail fast and fix configuration |

## Key takeaways

- Use ACL users, one per service, and switch off or secure the `default` user
- Grant least privilege: key patterns, channel patterns, command categories
- Rule order matters, and `INFO` is in `@dangerous`, which affects ioredis' ready check
- Store users in an `aclfile`, keep only hashed passwords there and rotate via multiple passwords
- Use `ACL LOG` to tune permissions and to detect abuse
- Configure auth consistently across replicas, Sentinel and Cluster nodes

**Previous:** [Security README](./README.md) | **Next:** [TLS](./02_tls.md)
