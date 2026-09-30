# Memory and Eviction

Redis keeps data in RAM, so memory is your most important resource. Without limits, Redis can grow until the OS kills it. This lesson covers measuring memory, capping it, and choosing what gets removed when the cap is hit.

## Set a memory limit

```
maxmemory 2gb
maxmemory-policy allkeys-lru
```

At runtime:

```ts
await redis.config("SET", "maxmemory", "2gb");
await redis.config("SET", "maxmemory-policy", "allkeys-lru");
await redis.config("GET", "maxmemory*");
```

Without `maxmemory`, Redis uses as much RAM as it needs (on 64-bit systems). Always set it in production, leaving headroom for the OS, fork copy-on-write and replication buffers (a common guideline is to cap at roughly 60 to 75 percent of available RAM).

## Eviction policies

When `maxmemory` is reached and a write needs memory, Redis applies the policy.

| Policy            | Evicts                                              | Best for                                         |
| ----------------- | --------------------------------------------------- | ------------------------------------------------ |
| `noeviction`      | Nothing. Writes fail with `OOM command not allowed` | Primary data stores, queues, locks               |
| `allkeys-lru`     | Least recently used key, from all keys              | **General-purpose cache**                        |
| `allkeys-lfu`     | Least frequently used key, from all keys            | Cache with stable hot keys                       |
| `allkeys-random`  | Random key                                          | Uniform access                                   |
| `volatile-lru`    | LRU among keys **with a TTL**                       | Mixed use: cache keys have TTLs, data keys don't |
| `volatile-lfu`    | LFU among keys with a TTL                           | Same, frequency-based                            |
| `volatile-random` | Random among keys with a TTL                        | Rare                                             |
| `volatile-ttl`    | Keys with the shortest remaining TTL                | Prefer dropping soon-to-expire data              |

Notes:

- `volatile-*` policies behave like `noeviction` if no key has a TTL, so writes will fail
- LRU and LFU are **approximated** by sampling. Tune accuracy with `maxmemory-samples` (default 5, 10 is close to true LRU at a modest CPU cost)
- **Do not mix cache data and critical data in one instance under an `allkeys-*` policy**, or critical keys can be evicted

### Which one should I pick?

| Scenario                            | Policy                                                                      |
| ----------------------------------- | --------------------------------------------------------------------------- |
| Redis is purely a cache             | `allkeys-lru` (or `allkeys-lfu`)                                            |
| Redis holds sessions, locks, queues | `noeviction` (and monitor memory)                                           |
| Mixed workload in one instance      | `volatile-lru` with TTLs only on evictable keys, or better, split instances |

## Handling `noeviction` in Node.js

With `noeviction`, writes throw an error you should handle:

```ts
try {
  await redis.set(key, value);
} catch (err: any) {
  if (err.message.includes("OOM")) {
    // alert, shed load, fall back to the database
  } else {
    throw err;
  }
}
```

## Measuring memory

```ts
const mem = await redis.info("memory");
console.log(mem);
```

Key fields:

| Field                     | Meaning                                        |
| ------------------------- | ---------------------------------------------- |
| `used_memory`             | Bytes allocated by Redis for data              |
| `used_memory_human`       | Same, readable                                 |
| `used_memory_rss`         | Memory the OS reports for the process          |
| `mem_fragmentation_ratio` | RSS ÷ used_memory. About 1.0 to 1.5 is healthy |
| `maxmemory`               | Configured limit                               |
| `evicted_keys`            | Keys evicted so far (watch for spikes)         |
| `used_memory_peak`        | Highest usage seen                             |

Interpreting fragmentation:

- **Above about 1.5**: fragmentation. Consider `activedefrag yes` or a restart
- **Below 1.0**: Redis is swapping. This is very bad for latency

Per-key checks:

```ts
await redis.memory("USAGE", "user:1"); // bytes for one key
await redis.object("ENCODING", "user:1"); // internal encoding
```

From the shell:

```bash
redis-cli --bigkeys
redis-cli --memkeys
redis-cli MEMORY DOCTOR
```

## Reducing memory use

1. **TTL everything that is cache**, so keys age out
2. **Use hashes for many small objects** (compact encoding) instead of many string keys
3. **Shorten keys and field names** at large scale (millions of keys)
4. **Use appropriate types**: HyperLogLog for unique counts, bitmaps for flags
5. **Compress large values** (gzip or similar) at the application layer
6. **Cap collections** with `LTRIM`, `XADD ... MAXLEN`, or `ZREMRANGEBYRANK`
7. **Store IDs, not payloads**, when the payload lives in another database
8. **Avoid big keys**: split them across multiple keys

```ts
// Cap a recent-activity list at 100 items
await redis.lpush(`recent:${uid}`, item);
await redis.ltrim(`recent:${uid}`, 0, 99);

// Cap a stream approximately (cheaper than exact)
await redis.xadd("events", "MAXLEN", "~", 100000, "*", "type", "click");
```

## Deleting large values safely

`DEL` on a huge key blocks the main thread while memory is freed. `UNLINK` frees memory in a background thread:

```ts
await redis.unlink("huge:set");
```

Lazy freeing options (advanced):

```
lazyfree-lazy-eviction yes
lazyfree-lazy-expire yes
lazyfree-lazy-server-del yes
replica-lazy-flush yes
```

## Watch out for hidden memory

- **Replication and client output buffers**: slow replicas or slow subscribers can accumulate large buffers (`client-output-buffer-limit`)
- **Fork copy-on-write**: snapshots and AOF rewrites can temporarily raise usage under heavy writes
- **Fragmentation**: long-lived servers with mixed key sizes
- **Big Pub/Sub backlogs** with slow consumers

## Monitoring checklist

- Alert when `used_memory` reaches about 80 percent of `maxmemory`
- Track `evicted_keys` (unexpected evictions mean the cache is too small or the policy is wrong)
- Track `mem_fragmentation_ratio`
- Track `keyspace` counts and the number of keys without TTL
- Test behavior at the memory limit **before** production

## Key takeaways

- Always set `maxmemory` and a deliberate `maxmemory-policy`
- `allkeys-lru` suits caches, `noeviction` suits data you can't lose
- Use `INFO memory`, `MEMORY USAGE` and `--bigkeys` to find problems
- Prefer `UNLINK` for large deletions, and cap growing collections

**Next module:** [03_ioredis-basics](../03_ioredis-basics/README.md)
