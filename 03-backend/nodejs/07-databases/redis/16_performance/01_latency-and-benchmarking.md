# Latency and Benchmarking

You can't improve what you haven't measured, and you can easily **mismeasure**: averages hide the tail, synthetic benchmarks hide real payloads, and a blocked Node.js event loop gets blamed on Redis. This lesson covers how to measure honestly and how to diagnose a slow Redis.

## What "fast" means

| Metric | Meaning | Why it matters |
|--------|---------|----------------|
| **Latency** | Time for one request, start to finish | What users feel |
| **Throughput** | Requests per second | What capacity you have |
| **p50 / p95 / p99** | Percentiles of latency | The tail is where problems live |
| **Utilization** | How busy the Redis main thread is | Latency climbs steeply as it nears 100% |

A typical healthy picture inside one availability zone is **sub-millisecond to low-millisecond** round trips for simple commands, with most of that being the network. Your numbers depend on your network and payloads, so measure yours.

**Averages lie.** An average of 1 ms can hide a p99 of 40 ms. Always record and alert on percentiles.

## The single-thread reality

Commands execute **one at a time** on the main thread ([Redis Architecture](../00_introduction/03_redis-architecture.md#1-single-threaded-command-execution)). So:

- Latency = **your command's execution time + the time it waited behind others**
- A 50 ms command makes every command behind it wait **up to 50 ms**, whatever they do
- As load approaches one saturated core, queueing delay grows sharply, so **keep headroom** (watch the Redis process CPU: a core at 100% is saturated)

```
latency
   │                                   ╱
   │                                 ╱      ← the "knee": small load increases, big latency increases
   │                         ______╱
   │_____________________----
   └────────────────────────────────── load (requests/sec)
```

## Built-in diagnostics

### Is the host itself noisy?

```bash
redis-cli --intrinsic-latency 100       # run on the Redis host for 100 s: the baseline jitter of the machine
```

If the intrinsic latency is already several milliseconds (a busy VM, throttled container), Redis can't do better there.

### Round-trip latency

```bash
redis-cli -h redis.internal --latency                 # continuous min/avg/max
redis-cli -h redis.internal --latency-history -i 5    # a sample every 5 s
redis-cli -h redis.internal --latency-dist            # a distribution view
```

Run these **from an app host**, not your laptop, so you measure the path your traffic takes.

### The slow log

`SLOWLOG` records commands whose **execution time** exceeded a threshold:

```
slowlog-log-slower-than 10000     # microseconds (default 10 ms). Lower it while investigating
slowlog-max-len 128
```

```bash
redis-cli SLOWLOG GET 10
redis-cli SLOWLOG LEN
redis-cli SLOWLOG RESET
```

> `SLOWLOG` measures **execution only**: not time queued, not network, not client parsing. A command can feel slow to clients and **never appear** in the slow log, because the delay was queueing behind another command (which will be in the log) or the network.

```ts
export async function slowCommands(redis: Redis, count = 20) {
  const rows = (await redis.slowlog("GET", count)) as [number, number, number, string[], string, string][];
  return rows.map(([id, ts, micros, args, client, name]) => ({
    id,
    at: new Date(ts * 1000).toISOString(),
    ms: micros / 1000,
    command: args.slice(0, 3).join(" "),            // never log whole args: they can hold values and secrets
    client,
    name,
  }));
}
```

### Command statistics

`INFO commandstats` accumulates calls and time **per command** since the last reset. It shows **which commands cost the most in total**, which is often not the slowest single call:

```ts
import { parseInfo } from "./redis-info.js";          // from the Replication lesson

export async function commandCost(redis: Redis, top = 15) {
  const info = parseInfo(await redis.info("commandstats"));
  return Object.entries(info)
    .map(([k, v]) => {
      const m = Object.fromEntries(v.split(",").map((p) => p.split("=") as [string, string]));
      return {
        command: k.replace("cmdstat_", ""),
        calls: Number(m.calls),
        totalMs: Number(m.usec) / 1000,
        perCallUs: Number(m.usec_per_call),
        failed: Number(m.failed_calls ?? 0),
      };
    })
    .sort((a, b) => b.totalMs - a.totalMs)
    .slice(0, top);
}
```

Read it as: "`GET` is 40 µs per call but called 900 million times, so it dominates total CPU", and "`SMEMBERS` is called rarely but takes 80 ms per call, so it is stalling everyone". Reset with `CONFIG RESETSTAT` between experiments.

Redis 7 also tracks **per-command latency percentiles** (`INFO latencystats`, with `latency-tracking yes`, the default), so you can see p50/p99/p99.9 per command directly.

### Latency events

```
latency-monitor-threshold 100          # log events slower than 100 ms (0 = off, the default)
```

```bash
redis-cli LATENCY DOCTOR               # human-readable analysis and advice
redis-cli LATENCY LATEST               # recent latency spikes by event (fork, aof-fsync, command, ...)
redis-cli LATENCY HISTORY fork
```

`LATENCY DOCTOR` is a good first stop for **system-level** causes: forks, slow `fsync`, expiry cycles.

### Useful `INFO` fields

| Section / field | Look for |
|-----------------|----------|
| `stats`: `instantaneous_ops_per_sec` | Current load |
| `stats`: `total_net_input_bytes` / `output_bytes` | Bandwidth (large values) |
| `stats`: `rejected_connections` | `maxclients` reached |
| `stats`: `expired_keys`, `evicted_keys` | Expiry storms, memory pressure |
| `clients`: `connected_clients`, `blocked_clients` | Connection leaks, blocked consumers |
| `persistence`: `latest_fork_usec` | How long the last `fork()` took (large datasets stall here) |
| `persistence`: `aof_delayed_fsync` | Slow disk with AOF |
| `memory`: `mem_fragmentation_ratio` | Fragmentation or swapping |
| `cpu`: `used_cpu_sys` / `used_cpu_user` | CPU burn |

## Measuring from Node.js

The latency your **application** sees includes Node's event loop. Measure there, with a histogram:

```ts
import { createHistogram, monitorEventLoopDelay } from "node:perf_hooks";

export class LatencyRecorder {
  private hists = new Map<string, ReturnType<typeof createHistogram>>();

  async time<T>(op: string, fn: () => Promise<T>): Promise<T> {
    const start = performance.now();
    try {
      return await fn();
    } finally {
      const us = Math.max(1, Math.round((performance.now() - start) * 1000));      // microseconds, at least 1
      let h = this.hists.get(op);
      if (!h) this.hists.set(op, (h = createHistogram()));
      h.record(us);
    }
  }

  report() {
    return Object.fromEntries([...this.hists].map(([op, h]) => [op, {
      count: h.count, p50us: h.percentile(50), p95us: h.percentile(95), p99us: h.percentile(99), maxUs: h.max,
    }]));
  }
}

const rec = new LatencyRecorder();
const value = await rec.time("cache.get", () => redis.get(key));
```

In production, export these as metrics (Prometheus, Datadog) per operation name. **Don't** use the raw key as a label (unbounded cardinality).

### Is it Redis, or is it your event loop?

If the event loop is blocked, **every await looks slow**, including Redis calls that were actually quick:

```ts
const loop = monitorEventLoopDelay({ resolution: 20 });
loop.enable();

setInterval(() => {
  console.log({ loopP99ms: loop.percentile(99) / 1e6, loopMaxMs: loop.max / 1e6 });
  loop.reset();
}, 10_000);
```

| Redis-side latency | Event-loop delay | Diagnosis |
|--------------------|------------------|-----------|
| High | Low | Redis, network, or a slow command |
| **Low** (per `SLOWLOG` and `redis-cli --latency`) | **High** | **Your Node process** (CPU-bound code, big `JSON.parse`, sync I/O, GC) |
| Both high | Both high | Check CPU contention on shared hosts |

Compare the application-measured p99 against `redis-cli --latency` from the **same host**. A large gap points at the client side.

## Benchmarking

### `redis-benchmark`

```bash
# GET and SET, 256-byte values, 50 clients, 500k requests, 4 threads
redis-benchmark -h redis.internal -p 6379 -t get,set -d 256 -c 50 -n 500000 --threads 4 -q

# with pipelining: 16 commands per round trip
redis-benchmark -h redis.internal -t get,set -d 256 -c 50 -n 500000 -P 16 -q

# random keys across a keyspace, CSV output
redis-benchmark -t get,set -d 256 -r 1000000 -n 500000 --csv
```

| Flag | Meaning |
|------|---------|
| `-c` | Concurrent clients (default 50) |
| `-n` | Total requests (default 100,000) |
| `-d` | Value size in bytes (**default 3, unrealistically small**) |
| `-P` | Pipeline depth |
| `-r` | Random key space size (so you don't hit one hot key) |
| `-t` | Which tests to run |
| `--threads` | Benchmark client threads (Redis 6+) |

Honest benchmarking:

- **Use realistic payload sizes** (`-d`) and a realistic key spread (`-r`). Three-byte values on one key prove nothing
- Run the benchmark from **separate machines**, close to production topology, never on the Redis host
- **Warm up** first, then measure a steady-state window
- Use **production-like config**: persistence on, TLS if you use it, replicas attached
- **Benchmark the whole path** (your client library and serialization), not only the server
- Treat results as **relative** (before versus after a change), not absolute truth

### A Node load generator that tells the truth about your code

`redis-benchmark` can't see your serialization, your ioredis settings, or your access pattern. Write a small harness:

```ts
import { createHistogram } from "node:perf_hooks";

interface RunOptions { concurrency: number; warmupMs: number; durationMs: number; op: () => Promise<unknown> }

export async function load({ concurrency, warmupMs, durationMs, op }: RunOptions) {
  const hist = createHistogram();
  let ok = 0, errors = 0;

  const measureFrom = Date.now() + warmupMs;
  const stopAt = measureFrom + durationMs;

  await Promise.all(Array.from({ length: concurrency }, async () => {
    while (Date.now() < stopAt) {
      const t = performance.now();
      try { await op(); } catch { errors++; continue; }
      if (Date.now() >= measureFrom) {
        hist.record(Math.max(1, Math.round((performance.now() - t) * 1000)));
        ok++;
      }
    }
  }));

  return {
    throughputPerSec: Math.round(ok / (durationMs / 1000)),
    p50ms: hist.percentile(50) / 1000,
    p95ms: hist.percentile(95) / 1000,
    p99ms: hist.percentile(99) / 1000,
    errors,
  };
}
```

Compare access patterns on **your** deployment:

```ts
const ids = Array.from({ length: 100_000 }, (_, i) => `bench:${i}`);
const pick = () => ids[Math.floor(Math.random() * ids.length)]!;

console.log("one GET per call", await load({ concurrency: 50, warmupMs: 2_000, durationMs: 10_000,
  op: () => redis.get(pick()) }));

console.log("pipeline of 16", await load({ concurrency: 50, warmupMs: 2_000, durationMs: 10_000,
  op: () => { const p = redis.pipeline(); for (let i = 0; i < 16; i++) p.get(pick()); return p.exec(); } }));

console.log("MGET of 16", await load({ concurrency: 50, warmupMs: 2_000, durationMs: 10_000,
  op: () => redis.mget(Array.from({ length: 16 }, pick)) }));
```

Remember that this is a **closed-loop** test: each worker waits for a reply before sending the next request, so a slow server **reduces** the load you offer (called *coordinated omission*), which makes tail latency look better than a real, open-loop workload would. For capacity numbers, increase concurrency until throughput stops growing, and treat the latency at the knee as your limit.

### Reading results

| Observation | Meaning |
|-------------|---------|
| Throughput rises with concurrency, latency flat | Headroom |
| Throughput flat, **latency rising** | Saturation (Redis CPU, network, or the Node process) |
| p99 far above p50 | Intermittent stalls: slow commands, forks, GC, network |
| Pipelining gives a big jump | You were **round-trip bound** (very common) |
| Pipelining gives little | Server-bound or client-CPU-bound |
| Node CPU at 100%, Redis idle | The bottleneck is **your process** |

## Common causes of latency spikes

| Symptom | Likely cause | Check | Fix |
|---------|--------------|-------|-----|
| Periodic multi-ms to second stalls | **`fork()`** for RDB or AOF rewrite on a large dataset | `INFO persistence` (`latest_fork_usec`), `LATENCY DOCTOR` | Smaller instances or shards, off-peak snapshots, enough memory headroom, avoid THP |
| Stalls that grow with dataset size | Fork cost plus copy-on-write | Same | Same |
| Steady slowness plus huge `rss` | **Swapping** | `INFO memory`, host `vmstat` | Never swap Redis. Right-size memory |
| Spikes at fixed intervals | A scheduled slow command (`KEYS`, big `SMEMBERS`, a cron-driven script) | `SLOWLOG` | Remove or paginate it |
| Spikes with many keys expiring together | Active-expire cycle | `expired_keys` rate | TTL jitter ([Expiration](../02_redis-fundamentals/03_expiration-and-ttl.md)) |
| Slow writes with AOF | Slow disk, `appendfsync always`, `aof_delayed_fsync` | `INFO persistence` | Faster disk, `everysec`, no competing I/O |
| Random spikes on cloud VMs | Noisy neighbors, CPU steal, throttled containers | `--intrinsic-latency`, host metrics | Dedicated or larger instances, raise CPU limits |
| Slow only for some requests | Big keys | `--bigkeys`, [Command Optimization](./02_command-optimization.md) | Split, cap, paginate |
| Latency rising with connection count | Many connections, big output buffers | `INFO clients`, `CLIENT LIST` | Reuse one client, limit blocking consumers |
| Slow only from some hosts | Network path (cross-AZ, NAT, DNS) | `redis-cli --latency` from that host | Co-locate, fix routing |
| Client p99 high, server healthy | **Event loop blocked** in Node | `monitorEventLoopDelay` | Offload CPU work, shrink payloads |

### System settings that matter

- **Disable Transparent Huge Pages** (`echo never > /sys/kernel/mm/transparent_hugepage/enabled`). Redis warns about it at startup, because THP inflates fork copy-on-write and latency
- **`vm.overcommit_memory = 1`** so `fork()` for snapshots doesn't fail under memory pressure
- Enough **memory headroom**: persistence can temporarily raise usage under write load ([Memory and Eviction](../02_redis-fundamentals/05_memory-and-eviction.md))
- Keep the host **un-swapped**, and avoid running other heavy processes beside Redis
- `net.core.somaxconn` and `tcp-backlog` for large connection bursts

### Network and connection costs

- **Co-locate** apps and Redis in the same region and zone where possible. Every cross-AZ hop adds latency, and cross-region is much worse
- **Reuse connections.** A new connection costs a TCP handshake, an optional TLS handshake and `AUTH` ([Connection Management](../08_nodejs-integration/01_connection-management.md))
- ioredis disables Nagle's algorithm (`noDelay`) by default, which is what you want
- TLS adds CPU and handshake cost, which is a good trade, but measure it. Connection reuse makes it negligible in steady state
- Large values are bounded by **bandwidth**: 1 MB × 1,000 ops/sec is about 8 Gbit/s, so check the NIC before blaming Redis

## A diagnosis workflow

```
1. Is it real?             Compare app p99 vs `redis-cli --latency` from the same host.
2. Client or server?       Event-loop delay high, Redis low → fix your process.
3. Any slow commands?      SLOWLOG GET, INFO commandstats (total time per command).
4. Any big keys?           redis-cli --bigkeys / --memkeys; MEMORY USAGE on suspects.
5. Any system stalls?      LATENCY DOCTOR; INFO persistence (fork, fsync); swap; THP.
6. Network?                --latency from the app host; cross-AZ; DNS; TLS handshakes.
7. Load?                   ops/sec, CPU of the Redis process, connection count.
8. Fix ONE thing, re-measure, repeat.
```

Capture the findings (queries, graphs, before/after numbers) in the incident or PR, because performance work without numbers is folklore.

## Capacity planning in three numbers

| Resource | Estimate | Watch |
|----------|----------|-------|
| **CPU** | Ops/sec at your command mix, from a benchmark | Redis process near one full core means scale out or reduce work |
| **Memory** | `entries × (value bytes + overhead)`, verified with `MEMORY USAGE` on real samples, plus fork and buffer headroom | `used_memory` against `maxmemory` ([Memory Optimization](./03_memory-optimization.md)) |
| **Bandwidth** | `average payload × ops/sec` (both directions) | NIC limits, cloud network caps |

Leave headroom for peaks, failovers (a replica taking over must handle the full load), and growth.

## Dashboard essentials

- Client-side **p50/p95/p99 per operation**, error rate, timeouts
- Redis: ops/sec, CPU, memory versus `maxmemory`, connected and blocked clients, evictions, expired keys, `latest_fork_usec`, replication lag, `mem_fragmentation_ratio`
- Slow-log **count** and the top commands by total time
- Node: event-loop delay, CPU, heap, GC pauses

## Testing for performance regressions

- Keep the **load harness** in the repo, and run it before and after risky changes (a new data structure, a new serialization format)
- In CI, assert **structural** properties that predict performance (bounded list lengths, compact encodings, no `KEYS` usage) rather than flaky absolute timings
- Run a periodic **big-key and TTL audit** in production ([Scan and Iteration](../05_key-management/02_scan-and-iteration.md#real-world-recipes))

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Watching averages | Percentiles, especially p99 |
| Benchmarking with 3-byte values on one key | Realistic sizes and a random key space |
| Benchmarking from the Redis host | A separate host on the real network path |
| Blaming Redis for a blocked event loop | Measure event-loop delay |
| Trusting `SLOWLOG` for queueing and network delay | It shows execution time only |
| Tuning server settings first | Fix access patterns first |
| Ignoring `fork` stalls on big instances | Check `latest_fork_usec`, shard, and give headroom |
| Running Redis near its memory limit or swapping | Headroom, and no swap |
| Mixing latency-sensitive traffic with heavy batch jobs on one instance | Separate instances or replicas for batch work |
| Not re-measuring after a "fix" | Before and after numbers, every time |
| Closed-loop tests used as capacity proof | Push concurrency past the knee, and be wary of the tail |

## Key takeaways

- Latency is **client + network + queue + execution + system**. Find the layer before fixing
- Use **percentiles**, `SLOWLOG`, `INFO commandstats`, `LATENCY DOCTOR`, and compare against Node's **event-loop delay**
- Benchmark with **realistic payloads and a realistic client**, from a separate host, and read throughput and tail latency together
- **Round trips, slow commands and forks** explain most real-world Redis latency
- Change one thing at a time, and keep the numbers

**Next:** [Command Optimization](./02_command-optimization.md)