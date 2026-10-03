# TLS

By default Redis speaks plain text. Anyone who can observe the network path can read keys, values and **passwords**, and can alter traffic. TLS encrypts the connection, verifies the server's identity and, optionally, verifies the client's too.

```
without TLS:  app ──── "AUTH app s3cret" / "GET user:1" ────► redis      (readable by anyone on the path)
with TLS:     app ═══════ encrypted, authenticated ════════► redis
```

Use TLS whenever traffic leaves a single trusted host: between machines, across VPCs or availability zones, to a managed service, or in any environment with compliance requirements. It is cheap insurance even inside a private network.

## What TLS covers

| Link | Setting |
|------|---------|
| Client to server | `tls-port` |
| Primary to replica | `tls-replication yes` |
| Cluster bus (node to node) | `tls-cluster yes` |
| Sentinel connections | Sentinel TLS settings (mirror the same options) |

If you only secure client connections, replication and cluster traffic still travel in plain text.

## Server configuration

Redis must be built with TLS support. Official packages and Docker images usually are. Check with your distribution if the `tls-*` options are rejected.

```bash
# redis.conf
port 0                          # disable the plain-text port
tls-port 6379                   # serve TLS here instead

tls-cert-file /etc/redis/tls/redis.crt
tls-key-file  /etc/redis/tls/redis.key
tls-ca-cert-file /etc/redis/tls/ca.crt

tls-protocols "TLSv1.2 TLSv1.3"
tls-prefer-server-ciphers yes

tls-replication yes
tls-cluster yes

tls-auth-clients no             # yes = mutual TLS (see below)
```

| Option | Purpose |
|--------|---------|
| `port 0` | Turns off the unencrypted listener. Keep it enabled only during a migration |
| `tls-port` | TLS listener |
| `tls-cert-file`, `tls-key-file` | The server's certificate and private key |
| `tls-ca-cert-file`, `tls-ca-cert-dir` | CAs used to verify client certificates and peers |
| `tls-auth-clients` | `no`, `yes` (required) or `optional` client certificate checks |
| `tls-protocols` | Allowed protocol versions. Avoid anything older than TLS 1.2 |
| `tls-ciphers`, `tls-ciphersuites` | Restrict ciphers if policy requires it |
| `tls-replication`, `tls-cluster` | Encrypt replication and the cluster bus |

Protect the private key: file mode `600`, owned by the Redis user, never committed to git or baked into images.

## Generating certificates (development)

For local and test environments you can create a private CA with OpenSSL. For production, use your organization's PKI, a managed certificate service or an ACME-based tool.

```bash
# 1. certificate authority
openssl genrsa -out ca.key 4096
openssl req -x509 -new -nodes -key ca.key -sha256 -days 365 \
  -subj "/CN=Dev Redis CA" -out ca.crt

# 2. server key and signing request
openssl genrsa -out redis.key 2048
openssl req -new -key redis.key -subj "/CN=redis.local" -out redis.csr

# 3. sign it, including Subject Alternative Names (hostnames and IPs clients connect with)
printf "subjectAltName=DNS:localhost,DNS:redis.local,IP:127.0.0.1" > san.ext
openssl x509 -req -in redis.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out redis.crt -days 365 -sha256 -extfile san.ext
```

Modern TLS clients match the **Subject Alternative Name**, not the CN. The name or IP your client connects to must appear in the SAN list or hostname verification fails.

The Redis source tree also ships `utils/gen-test-certs.sh` for quick test certificates.

### Test it

```bash
redis-cli --tls --cacert ca.crt -h localhost -p 6379 PING

# inspect the handshake and certificate
openssl s_client -connect localhost:6379 -CAfile ca.crt </dev/null
```

## Connecting with ioredis

Passing the `tls` option turns TLS on. It accepts the same options as Node's `tls.connect`.

```ts
import Redis from "ioredis";
import fs from "node:fs";

const redis = new Redis({
  host: "redis.local",
  port: 6379,
  username: "app",
  password: process.env.REDIS_PASSWORD,
  tls: {
    ca: fs.readFileSync("/etc/ssl/redis/ca.crt"),   // trust your private CA
    servername: "redis.local",                      // SNI and hostname verification
    minVersion: "TLSv1.2",
  },
});
```

When the server certificate is signed by a CA that your system already trusts (typical for managed services), an empty object is enough:

```ts
const redis = new Redis({ host: "my-redis.example.com", port: 6379, tls: {} });
```

### The `rediss://` URL

```ts
const redis = new Redis("rediss://app:pw@my-redis.example.com:6380");  // note the double "s"
```

Combine it with options when you need a custom CA:

```ts
const redis = new Redis("rediss://my-redis.example.com:6380", {
  username: "app",
  password: process.env.REDIS_PASSWORD,
  tls: { ca: fs.readFileSync("/etc/ssl/redis/ca.crt") },
});
```

### Never disable verification in production

```ts
// DON'T: removes protection against man-in-the-middle attacks
tls: { rejectUnauthorized: false }
```

This keeps encryption but removes identity checks, so an attacker can impersonate the server. If you hit certificate errors, fix the trust chain or the SAN list instead.

An alternative to passing `ca` in code is to point Node at extra roots through the environment, which also works for other libraries in the same process:

```bash
NODE_EXTRA_CA_CERTS=/etc/ssl/redis/ca.crt node app.js
```

## Mutual TLS (client certificates)

With `tls-auth-clients yes`, Redis only accepts clients that present a certificate signed by a trusted CA. That authenticates the **machine or service** at the transport layer, in addition to ACL credentials.

```bash
# client key and certificate, signed by the same CA
openssl genrsa -out client.key 2048
openssl req -new -key client.key -subj "/CN=orders-service" -out client.csr
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out client.crt -days 365 -sha256
```

```bash
# redis.conf
tls-auth-clients yes
tls-ca-cert-file /etc/redis/tls/ca.crt
```

```ts
const redis = new Redis({
  host: "redis.local",
  port: 6379,
  username: "orders",
  password: process.env.REDIS_PASSWORD,
  tls: {
    ca: fs.readFileSync("ca.crt"),
    cert: fs.readFileSync("client.crt"),
    key: fs.readFileSync("client.key"),
    servername: "redis.local",
  },
});
```

```bash
redis-cli --tls --cacert ca.crt --cert client.crt --key client.key -h redis.local PING
```

Note: if you set `tls-auth-clients yes`, clients without a certificate cannot connect, including `redis-cli` and monitoring agents. Redis 8 can also map certificate fields to ACL users in some setups. Check your version's docs if you want that.

## Replication, Sentinel and Cluster

```bash
# on every Redis node
tls-replication yes
tls-cluster yes
```

```ts
// Sentinel: TLS to Redis nodes and (separately) to the sentinels
new Redis({
  sentinels: [{ host: "s1", port: 26379 }],
  name: "mymaster",
  tls: { ca },             // connection to Redis nodes
  sentinelTLS: { ca },     // connection to sentinels
});

// Cluster
new Redis.Cluster([{ host: "n1", port: 6379 }], {
  redisOptions: { tls: { ca }, username: "app", password },
  // needed when nodes announce IPs but your certificate lists DNS names
  dnsLookup: (address, cb) => cb(null, address),
});
```

Cluster nodes may announce IP addresses, so certificates for nodes must include matching SANs, or you need to configure announced hostnames. See [Cluster](../15_redis-architecture/03_cluster.md).

## Managed services

Most providers support in-transit encryption but differ in how it is enabled:

- It may be a toggle at creation time, and on some services it **cannot be turned on later** without recreating the instance
- The TLS port can differ from 6379 (some use 6380)
- Certificates usually chain to a public CA, so `tls: {}` works. Some require you to download a CA bundle
- Some provide a single endpoint that terminates TLS for you

Check the provider's documentation for the exact endpoint, port and CA instructions.

## Docker example (development)

```yaml
# docker-compose.yml
services:
  redis:
    image: redis:7
    command: >
      redis-server
      --port 0
      --tls-port 6379
      --tls-cert-file /tls/redis.crt
      --tls-key-file /tls/redis.key
      --tls-ca-cert-file /tls/ca.crt
      --tls-auth-clients no
    volumes:
      - ./tls:/tls:ro
    ports:
      - "127.0.0.1:6379:6379"     # bind to localhost only
```

## Rotating certificates

Certificates expire, and an expired certificate is an outage, not a warning.

- Track expiry dates and alert well ahead of time (for example 30 days)
- Prefer short-lived certificates issued automatically
- On recent Redis versions many TLS settings can be changed at runtime with `CONFIG SET` (`tls-cert-file`, `tls-key-file`, `tls-ca-cert-file`), avoiding a restart. Verify support on your version first
- Deploy new CA certificates to clients **before** switching servers to certificates signed by a new CA, and keep both trusted during the overlap
- Node reads certificate files when the connection is created, so reconnecting clients pick up changes, but long-lived processes may need a restart to reload a changed `ca`

## Migrating from plain text to TLS

1. Enable `tls-port` alongside the existing `port` (both listeners active)
2. Update clients one by one to use TLS, verifying with `CLIENT LIST` or metrics
3. Turn on `tls-replication` and `tls-cluster` during a maintenance window (all nodes must agree)
4. When no plain-text clients remain, set `port 0`

## Performance

- The expensive part is the **handshake**, so reuse connections (one long-lived ioredis client, not one per request)
- Steady-state overhead is usually modest and shows up mostly as extra CPU on both ends
- Benchmark with your own workload using `redis-benchmark --tls ...` (see [Latency and Benchmarking](../16_performance/01_latency-and-benchmarking.md))

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `ECONNRESET` or immediate disconnect | Client without `tls` talking to a TLS port | Add `tls: {}` or use `rediss://` |
| `wrong version number` (SSL error) | TLS client talking to a plain-text port | Use the TLS port, or remove `tls` |
| `self-signed certificate in certificate chain` / `unable to verify the first certificate` | CA not trusted by the client | Provide `ca`, or `NODE_EXTRA_CA_CERTS` |
| `Hostname/IP does not match certificate's altnames` | SAN missing the name you used | Reissue with the right SANs, or connect using a listed name and set `servername` |
| `certificate has expired` | Expired certificate | Renew and rotate |
| `tlsv13 alert certificate required` / handshake failure | Server requires a client certificate | Provide `cert` and `key` |
| `ERR Client sent AUTH, but no password is set` | Mixed up auth setup | Fix ACL and connection options |
| Works with `redis-cli --tls` but not Node | Different trust stores | Provide `ca` explicitly |

Turn on `redis.on("error", ...)` and log `err.code` and `err.message`. Never log the connection options, since they contain passwords and keys.

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| TLS on clients only | Also `tls-replication yes` and `tls-cluster yes` |
| Leaving the plain-text port open | `port 0` once migrated |
| `rejectUnauthorized: false` in production | Fix trust and SANs instead |
| Certificate without SANs | Include every hostname and IP clients use |
| Private key world-readable or in git | Mode `600`, secret storage |
| Forgetting certificate expiry | Monitor and automate renewal |
| Creating a new TLS connection per request | Reuse a long-lived client |
| `tls-auth-clients yes` breaking tooling | Issue client certificates to tools too, or use `optional` |
| Assuming TLS replaces auth | You still need ACL users. TLS protects the channel, not permissions |

## Key takeaways

- Plain-text Redis exposes data and credentials to anyone on the network path
- Use `tls-port`, set `port 0`, and enable `tls-replication` and `tls-cluster` for full coverage
- In ioredis use the `tls` option or a `rediss://` URL, always with certificate verification on
- Mutual TLS adds machine identity on top of ACL users
- Include SANs, protect private keys and automate certificate rotation
- TLS and ACLs are complementary: encryption and identity are different jobs

**Previous:** [Authentication and ACL](./01_authentication-and-acl.md) | **Next:** [Security Checklist](./03_security-checklist.md)
