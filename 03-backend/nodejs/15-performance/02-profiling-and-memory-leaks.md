# Profiling & Memory Leak Diagnostics in Node.js

> Profiling CPU flamegraphs with Clinic.js and 0x, inspecting V8 garbage collection traces, and analyzing heap dumps.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Profiling Tools** | `0x`, `clinic doctor`, `clinic flame`, V8 flags |
| **Level** | Production Diagnostics |

---

## 1. Generating CPU Flamegraphs with `0x`

A flamegraph visualizes which functions consume the most CPU cycles:
- **Width of a bar**: Represents the percentage of CPU time spent in that function.
- **Towers / Spikes**: Deep call stacks. Wide plateaus indicate CPU bottlenecks (e.g. expensive regex, synchronous compression).

```bash
# Run application under 0x profiler
npx 0x -- node server.js

# Generate load using autocannon in another terminal
npx autocannon -c 50 -d 15 http://localhost:3000/api/heavy-endpoint
```
When stopped, `0x` outputs an interactive HTML flamegraph in your browser.

---

## 2. Using `clinic doctor`

Clinic.js diagnoses health issues automatically:
```bash
npx clinic doctor --on-port 'autocannon -c 50 -d 15 http://localhost:3000' -- node server.js
```
`clinic doctor` graphs:
- **CPU Usage**: Detects underutilization or saturation.
- **Memory (RSS / Heap)**: Detects memory leaks (monotonic climbing graph that never returns to baseline after GC).
- **Event Loop Delay**: Pinpoints blocking synchronous tasks.

---

## 3. Useful V8 Memory Diagnostic Flags

```bash
# Log every garbage collection event and execution duration
node --trace-gc server.js

# Trigger heap dump automatically before Out-Of-Memory termination
node --heapsnapshot-near-heap-limit=3 server.js

# Set maximum old space heap size (e.g. 4GB for memory-heavy services)
node --max-old-space-size=4096 server.js
```
