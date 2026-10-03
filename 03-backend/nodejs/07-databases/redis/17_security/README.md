# 17 Security

Redis was designed to run **inside a trusted network**. Out of the box it listens without encryption, and (depending on version and configuration) may accept commands without a password. That is fine on a laptop and dangerous anywhere else. This chapter shows how to lock Redis down and how to use it safely from Node.js with ioredis.

## Defense in depth

No single control is enough. Each layer assumes the one above it can fail.

```
┌─────────────────────────────────────────────────────────┐
│ 1. Network     private subnet, firewall, bind address   │
│   ┌─────────────────────────────────────────────────┐   │
│   │ 2. Transport   TLS (and optionally mutual TLS)  │   │
│   │   ┌─────────────────────────────────────────┐   │   │
│   │   │ 3. Identity   ACL users, strong secrets │   │   │
│   │   │   ┌─────────────────────────────────┐   │   │   │
│   │   │   │ 4. Permissions  least privilege │   │   │   │
│   │   │   │   ┌─────────────────────────┐   │   │   │   │
│   │   │   │   │ 5. Application          │   │   │   │   │
│   │   │   │   │    input handling,      │   │   │   │   │
│   │   │   │   │    secrets, auditing    │   │   │   │   │
│   │   │   │   └─────────────────────────┘   │   │   │   │
│   │   │   └─────────────────────────────────┘   │   │   │
│   │   └─────────────────────────────────────────┘   │   │
│   └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

## What you will learn

- Authenticate with ACL users instead of one shared password
- Grant each service only the commands, keys and channels it needs
- Encrypt client, replication and cluster traffic with TLS
- Connect from ioredis with credentials and certificates
- Audit a deployment against a practical checklist

## Contents

| # | File | Topic |
|---|------|-------|
| 01 | [Authentication and ACL](./01_authentication-and-acl.md) | `AUTH`, users, command and key permissions, rotation, ACL logs |
| 02 | [TLS](./02_tls.md) | Server config, certificates, ioredis TLS options, mutual TLS, troubleshooting |
| 03 | [Security Checklist](./03_security-checklist.md) | Network, host, application and monitoring checks, plus an audit script |

## Quick start

The minimum viable secure setup, in three moves:

```bash
# redis.conf
bind 127.0.0.1 -::1             # or a private interface, never 0.0.0.0 by accident
protected-mode yes
aclfile /etc/redis/users.acl    # users defined in a file (see chapter 01)
```

```
# /etc/redis/users.acl
user default off
user app on >use-a-long-random-secret ~app:* +@read +@write +@transaction +@connection -@dangerous +info
```

```ts
import Redis from "ioredis";

const redis = new Redis({
  host: "redis.internal",
  port: 6379,
  username: "app",
  password: process.env.REDIS_PASSWORD,
  // TLS: see 02_tls.md
});
```

## Common threats

| Threat | Main defenses |
|--------|---------------|
| Instance exposed to the internet | Network isolation, `bind`, firewall, never publish the port publicly |
| No or weak password | ACL users, long random secrets |
| Traffic sniffed or tampered with | TLS |
| Compromised app does too much | Least-privilege ACL (key patterns, no `@dangerous`) |
| Destructive commands (`FLUSHALL`, `CONFIG`, `KEYS`) | Deny `@dangerous` and `@admin` for app users |
| User input reaches keys, patterns or scripts | Validate and namespace on the application side |
| Secrets leaked via logs or git | Secret manager, redact connection strings |
| Known vulnerabilities | Patch promptly, restrict scripting for untrusted users |

## Prerequisites

- [Connection](../03_ioredis-basics/01_connection.md) and [Configuration](../03_ioredis-basics/02_configuration.md)
- [Redis Architecture](../15_redis-architecture/01_replication.md) if you run replicas, Sentinel or Cluster, since each node needs the same security settings

**Start:** [Authentication and ACL](./01_authentication-and-acl.md)
