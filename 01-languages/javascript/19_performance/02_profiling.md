# Profiling

Profiling answers "**where does the time go?**" with data instead of guesses. This file covers quick timing tools, microbenchmarks, browser DevTools, Node profilers, flame graphs, and tracing.

## Timing basics

```js
// Monotonic, high-resolution clock (browser and Node)
const t0 = performance.now();
doWork();
const ms = performance.now() - t0;

// Console timers
console.time('work');
doWork();
console.timeEnd('work');       // work: 12.345ms
```

Do not use `Date.now()` for measuring short durations: it has coarse resolution and can jump when the system clock changes.

For async work, `await` before stopping the timer:

```js
const t0 = performance.now();
await fetchAll();
console.log(`fetchAll: ${(performance.now() - t0).toFixed(1)} ms`);
```

### User Timing API: named marks and measures

```js
performance.mark('parse-start');
parse(data);
performance.mark('parse-end');
performance.measure('parse', 'parse-start', 'parse-end');

const [m] = performance.getEntriesByName('parse');
console.log(m.duration);
```

Marks and measures appear in the DevTools **Performance** panel under **Timings**, linking your own labels to the flame chart. In Node, import from `node:perf_hooks` if needed (`performance` is also global).

Wrap a function for repeated timing (Node):

```js
import { performance, PerformanceObserver } from 'node:perf_hooks';

const timed = performance.timerify(function heavy() { /* ... */ });

const obs = new PerformanceObserver((list) => {
  for (const e of list.getEntries()) console.log(e.name, e.duration.toFixed(2), 'ms');
});
obs.observe({ entryTypes: ['function'] });

timed();
```

## Microbenchmarks

A microbenchmark compares small pieces of code. They are easy to get wrong.

### Common traps

| Trap | Effect | Fix |
|------|--------|-----|
| No warm-up | Measures the interpreter and JIT compile time | Run many iterations before measuring |
| One run | Noise from GC, other processes | Many samples; report median and spread |
| Result unused | Engine removes dead code | Use or return the result |
| Constant input | Engine precomputes or specializes | Vary inputs realistically |
| Tiny input sizes | Not representative of real workloads | Use production-sized data |
| Measuring order effects | The first test pays warm-up costs | Randomize/alternate, or run in separate processes |
| Different shapes/types in test vs production | Different optimization paths | Same types as real use |

### A simple harness

```js
function bench(name, fn, { iterations = 1000, samples = 20 } = {}) {
  for (let i = 0; i < iterations; i++) fn();                 // warm-up

  const times = [];
  for (let s = 0; s < samples; s++) {
    const t0 = performance.now();
    for (let i = 0; i < iterations; i++) sink = fn();        // assign so the result is "used"
    times.push((performance.now() - t0) / iterations);
  }

  times.sort((a, b) => a - b);
  const median = times[times.length >> 1];
  const min = times[0];
  const max = times[times.length - 1];
  console.log(`${name}: median ${median.toFixed(4)} ms  (min ${min.toFixed(4)}, max ${max.toFixed(4)})`);
}
let sink;
```

Prefer a maintained tool:

| Tool | Notes |
|------|-------|
| **Tinybench** | Small, modern, works in Node and browsers |
| **mitata** | Accurate, good statistics |
| **Benchmark.js** | Classic, older |
| **`vitest bench`** | Built on Tinybench, integrates with Vitest |
| **jsbench / jsperf-style sites** | Quick browser comparisons (run in your target browsers) |

```js
import { Bench } from 'tinybench';

const bench = new Bench({ time: 500 });
bench
  .add('for loop', () => { let s = 0; for (let i = 0; i < arr.length; i++) s += arr[i]; return s; })
  .add('reduce',   () => arr.reduce((a, b) => a + b, 0));

await bench.run();
console.table(bench.table());
```

Microbenchmarks tell you about **one snippet in isolation**. Confirm with an end-to-end measurement before committing to a change.

## CPU profiling

A CPU profiler samples the call stack many times per second and reports where time was spent.

| Term | Meaning |
|------|---------|
| **Self time** | Time spent in the function itself |
| **Total time** (inclusive) | Self time plus everything it called |
| **Hot path** | The chain of calls responsible for most time |
| **Samples** | Snapshots of the stack at intervals |

Optimize functions with high **self time** or high **total time** that you can reduce (by calling them less, doing less per call, or using a better algorithm).

### Flame graphs

A **flame graph** shows the call stack as stacked bars: width is proportional to time (or samples), the root is at the bottom (or top in "icicle" views).

```
          ┌──────────────┐
          │  JSON.parse  │   ← wide box at the top of a stack = a hot leaf function
┌─────────┴──────────────┴──────────┐
│          parseRequest             │
├────────────────────────┬──────────┤
│       handleRequest    │  render  │
└────────────────────────┴──────────┘
```

How to read it:

- **Wide** boxes cost a lot of time
- Look for wide boxes at the **top** (self time) and plateaus
- Ignore tall but narrow towers: depth is not cost
- Compare before and after, using the same scenario

## Chrome DevTools (browser)

### Performance panel

1. Open DevTools → **Performance**
2. Optionally throttle CPU (4x or 6x slowdown) and network to simulate average devices
3. Click **Record**, perform the interaction (or reload with the reload button), then stop
4. Inspect:

| Area | What it shows |
|------|---------------|
| **Summary / Bottom-Up / Call Tree / Event Log** | Time by activity and function |
| **Main** track | Flame chart of the main thread; red corners or striped bars mark long tasks (over 50 ms) |
| **Frames** | Frame timing, dropped frames |
| **Network** | Request timeline |
| **Timings** | LCP, FCP, user timing marks |
| **Interactions** | Input events and their processing time |
| **Memory** checkbox | JS heap, DOM node and listener counts |

Typical findings: long scripting tasks, forced reflow (purple "Layout" blocks inside JS), excessive style recalculation, large paint, and third-party scripts.

### Other panels

| Panel | Use |
|-------|-----|
| **Lighthouse** | Automated audit with scores and suggestions (lab data) |
| **Network** | Waterfall, payload sizes, caching, blocking requests, throttling |
| **Coverage** | How much shipped JS/CSS is actually used |
| **Rendering** | FPS meter, paint flashing, layout shift regions |
| **Memory** | Heap snapshots and allocation sampling (see [Memory Leaks](../18_memory-and-garbage-collection/03_memory-leaks.md)) |
| **Performance insights / Recorder** | Guided analysis and scripted user flows |

Use an **incognito window** with extensions disabled: extensions skew profiles.

### Lab vs field data

| | Lab (Lighthouse, local DevTools) | Field (real users) |
|---|----------------------------------|--------------------|
| Conditions | Controlled, repeatable | Varied devices, networks, locations |
| Good for | Debugging, regressions in CI | Knowing what users actually experience |
| Tools | Lighthouse, WebPageTest, DevTools | CrUX (Chrome UX Report), RUM libraries, `web-vitals` |

Use lab data to **find and fix**; use field data to **decide what matters** and to verify.

## Profiling Node.js

### Built-in CPU profile

```bash
node --cpu-prof app.js                        # writes CPU.<timestamp>.cpuprofile on exit
node --cpu-prof --cpu-prof-dir=./profiles app.js
node --cpu-prof-interval 500 app.js           # sampling interval in microseconds
```

Open the `.cpuprofile` file in Chrome DevTools: **Performance** panel → **Load profile** (or the **JavaScript Profiler**/Memory tooling depending on the DevTools version), or in VS Code.

### Attach DevTools to a running process

```bash
node --inspect app.js                         # then open chrome://inspect → Inspect
node --inspect-brk app.js                     # pause on the first line
```

In DevTools use the **Profiler** / **Performance** panels and the **Memory** panel against your server while you generate load. In production, expose the inspector only on localhost or through a secure tunnel, never on a public interface.

### Programmatic profiling

```js
import inspector from 'node:inspector/promises';
import fs from 'node:fs/promises';

const session = new inspector.Session();
session.connect();

await session.post('Profiler.enable');
await session.post('Profiler.start');

await runWorkload();

const { profile } = await session.post('Profiler.stop');
await fs.writeFile('workload.cpuprofile', JSON.stringify(profile));
```

### Tools

| Tool | Use |
|------|-----|
| `node --cpu-prof` | Built-in sampling CPU profile |
| `node --prof` + `node --prof-process` | Older V8 tick profiler with text output |
| **Clinic.js** (`clinic doctor`, `flame`, `bubbleprof`, `heapprofiler`) | Diagnoses CPU, event-loop, async, and memory issues |
| **0x** | Generates interactive flame graphs |
| **`perf` + flame graphs** (Linux) | System-level profiling, including native code |
| **`--trace-gc`**, **`--trace-deopt`** | GC behavior and deoptimizations |
| **APM tools** (Datadog, New Relic, Sentry, Elastic APM, OpenTelemetry) | Continuous production profiling and tracing |
| **`autocannon`**, **`wrk`**, **`k6`** | Load generation to profile under realistic traffic |

```bash
# Generate load while profiling a server
npx autocannon -c 100 -d 30 http://localhost:3000/api/items
```

### Event loop health

```js
import { monitorEventLoopDelay } from 'node:perf_hooks';

const h = monitorEventLoopDelay({ resolution: 10 });
h.enable();

setInterval(() => {
  console.log({
    meanMs: (h.mean / 1e6).toFixed(2),
    p99Ms: (h.percentile(99) / 1e6).toFixed(2),
    maxMs: (h.max / 1e6).toFixed(2),
  });
  h.reset();
}, 10_000).unref();
```

A rising p99 delay means something is blocking the loop; take a CPU profile during that time. Also see `performance.eventLoopUtilization()` for a 0 to 1 measure of how busy the loop is.

## Profiling async code

CPU profilers show where the CPU is busy; they do not show **waiting**. A slow endpoint that spends 800 ms awaiting a database will show little CPU time. Use:

- **Tracing** (OpenTelemetry spans around each downstream call)
- **Timing logs** at boundaries (database, HTTP calls, cache)
- **Async hooks / AsyncLocalStorage** to attach a request id and measure per-request stages
- The browser **Network** panel waterfall

```js
async function timedStage(name, fn, log = console) {
  const t0 = performance.now();
  try {
    return await fn();
  } finally {
    log.info(`${name} took ${(performance.now() - t0).toFixed(1)} ms`);
  }
}

const user = await timedStage('db.user', () => db.users.find(id));
const items = await timedStage('db.items', () => db.items.byUser(id));
```

Server-side, the standard `Server-Timing` header lets browsers show backend phases in DevTools:

```js
res.setHeader('Server-Timing', `db;dur=${dbMs}, render;dur=${renderMs}`);
```

## Reading results: a checklist

1. **Is the problem reproducible?** Same scenario, same data, repeated runs
2. **Is it CPU, waiting, memory, or rendering?** Choose the right tool
3. **What is the biggest block?** Start with the widest or longest item
4. **Is that cost necessary?** Remove, reduce, defer, cache, or parallelize it
5. **What changed?** Compare profiles before and after one fix
6. **Does it hold under real conditions?** Check slower devices, bigger data, concurrent load

## Performance testing in CI

- Track bundle size (`size-limit`, `bundlewatch`) and fail a PR that exceeds the budget
- Run **Lighthouse CI** for key pages
- Run a small load test or benchmark suite on a schedule and chart the trend
- Alert on regressions in p95 latency, error rates, and event-loop delay in production

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Profiling a dev build or unminified, unoptimized setup | Not representative of production | Profile production-like builds |
| Profiling with DevTools open and extensions on | Overhead and noise | Clean profile, incognito |
| Reading one run | Noise | Repeat; compare medians |
| Trusting microbenchmark winners blindly | Often irrelevant end to end | Verify in the real workload |
| Looking only at CPU for a slow async request | Misses waiting time | Add tracing and timing |
| Profiling on a fast developer machine | Hides problems of slow devices | CPU/network throttling; test on real devices |
| Exposing `--inspect` publicly | Remote code execution risk | Bind to localhost; use a tunnel |
| Using `Date.now()` for timings | Coarse, can jump | `performance.now()` |
| Optimizing before profiling | Wrong target | Profile, then fix the top item |
| Ignoring field data | Lab results miss real-world variety | Use RUM and CrUX |

## Key takeaways

- Use `performance.now()`, `performance.mark/measure`, and `console.time` for quick timing
- Microbenchmarks need warm-up, realistic inputs, many samples, and used results
- CPU profilers sample stacks: read flame graphs by **width**, and focus on high self-time and hot paths
- Chrome DevTools covers browser profiling; `--cpu-prof`, `--inspect`, Clinic, and 0x cover Node
- CPU profiles do not show waiting: add tracing for I/O-bound slowness
- Combine lab tools (to debug) with field data (to prioritize and verify)

**Next:** [Browser Performance](./03_browser-performance.md)