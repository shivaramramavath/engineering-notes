# Persistence

Redis lives in memory, but it can write data to disk so it survives restarts. There are two mechanisms: **RDB snapshots** and the **AOF log**. You can use either, both, or neither.

## Choosing at a glance

| Setup            | Durability                  | Restart speed | Typical use                         |
| ---------------- | --------------------------- | ------------- | ----------------------------------- |
| None             | Data lost on restart        | Instant       | Pure cache                          |
| RDB only         | May lose minutes of writes  | Fast          | Cache you'd like to warm up quickly |
| AOF (`everysec`) | Lose about 1 second at most | Slower        | Most production data                |
| RDB + AOF        | Best balance                | Moderate      | Common production choice            |

## RDB (snapshots)

Redis periodically writes a compact point-in-time snapshot (`dump.rdb`).

```
# redis.conf (common defaults)
save 3600 1 300 100 60 10000
dbfilename dump.rdb
dir /data
```

Read it as: save if at least 1 change in 3600s, or 100 changes in 300s, or 10000 changes in 60s.

Manual snapshots:

```ts
await redis.bgsave(); // fork a child, does not block clients
await redis.lastsave(); // Unix time of the last successful save
```

Avoid `SAVE` in production: it blocks the server.

### How it works

Redis `fork()`s a child process. The child writes the snapshot while the parent keeps serving requests (copy-on-write memory).

**Pros**

- Compact single file, ideal for backups
- Very fast restarts
- Little impact on normal operation

**Cons**

- Writes since the last snapshot are lost on a crash
- `fork()` on a large dataset can cause a short latency spike
- Under heavy writes, copy-on-write can raise memory use significantly

## AOF (Append Only File)

Redis logs every write command. On restart it replays the log.

```
appendonly yes
appendfsync everysec
```

### `appendfsync` options

| Value      | Behavior                | Durability           | Speed                 |
| ---------- | ----------------------- | -------------------- | --------------------- |
| `always`   | fsync after every write | Safest               | Slowest               |
| `everysec` | fsync once per second   | Lose up to ~1 second | Good (default choice) |
| `no`       | Let the OS decide       | Weakest              | Fastest               |

### AOF rewriting

The log grows forever, so Redis compacts it in the background (`BGREWRITEAOF`), producing the smallest set of commands that rebuilds the dataset. It runs automatically based on growth:

```
auto-aof-rewrite-percentage 100
auto-aof-rewrite-min-size 64mb
```

```ts
await redis.bgrewriteaof(); // trigger manually
```

Redis 7 stores AOF as multiple files (a base file plus incremental files, tracked by a manifest) inside `appendonlydir/`.

**Pros**

- Much better durability than RDB alone
- Human-readable, repairable log (`redis-check-aof`)

**Cons**

- Larger files than RDB
- Slower restart on big datasets
- Slightly more write overhead

## Hybrid: RDB preamble + AOF

With `aof-use-rdb-preamble yes` (the default in modern versions), a rewritten AOF begins with an RDB-format snapshot followed by incremental commands. That gives **faster loading than pure AOF** while keeping AOF durability. Running RDB and AOF together is the usual production recommendation.

If both are enabled, Redis loads from **AOF** on startup because it is the more complete record.

## Docker and persistence

Persistence writes to `/data`. Mount a volume or the data vanishes with the container:

```bash
docker run -d -p 6379:6379 -v redis-data:/data redis:7 \
  redis-server --appendonly yes --appendfsync everysec
```

## Checking persistence from ioredis

```ts
const info = await redis.info("persistence");
console.log(info);
// rdb_last_save_time, rdb_changes_since_last_save,
// aof_enabled, aof_last_write_status, aof_current_size ...
```

Watch these fields in monitoring: `rdb_last_bgsave_status` and `aof_last_write_status` should be `ok`.

## Managed services and replication

- Persistence protects against **restarts**, not against **losing the disk or machine**. Use replicas and off-host backups too
- Replication is asynchronous, so a failover can lose the last writes even with AOF
- `WAIT numreplicas timeout` can make a write wait for replica acknowledgement, which reduces (but doesn't eliminate) that risk:

```ts
await redis.set("order:1", "paid");
const acked = await redis.wait(1, 100); // wait up to 100ms for 1 replica
```

## Backup basics

- Copy `dump.rdb` (or the whole `appendonlydir/`) to another location on a schedule
- Take backups from a **replica** to avoid load on the primary
- **Test restores.** A backup you never restored is a hope, not a backup

More in `19_redis-production/03_backup-and-disaster-recovery.md`.

## Decision guide

| Question                                     | Answer                                                                |
| -------------------------------------------- | --------------------------------------------------------------------- |
| Is Redis only a cache and rebuildable?       | Disable persistence, or RDB only                                      |
| Can I lose a minute of data?                 | RDB may be enough                                                     |
| Data must survive crashes with minimal loss? | AOF `everysec` (+ RDB)                                                |
| Absolutely cannot lose acknowledged writes?  | Redis is probably not the right primary store. Use a durable database |

## Key takeaways

- **RDB** = compact snapshots, fast restart, possible loss of recent writes
- **AOF** = write log, better durability, larger and slower to load
- Use **both** with `appendfsync everysec` for most production data
- Persistence is not a backup strategy, and mounting `/data` is essential in Docker

**Next:** [Memory and Eviction](./05_memory-and-eviction.md)
