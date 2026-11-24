# Cheatsheet 12: Debugging & Profiling Reference Card

> Production syntax reference for Node.js debugging, V8 CPU profiling, heap snapshot generation, and event loop latency monitoring.

---

### 1. Chrome DevTools & Node Inspector Flags
```bash
# Start process with inspector listening on default port 9229:
node --inspect server.js

# Break immediately at the first line of code (ideal for debugging startup bugs):
node --inspect-brk server.js

# Bind to all interfaces (required inside Docker containers):
node --inspect=0.0.0.0:9229 server.js
```
Open Chrome browser $\rightarrow$ navigate to `chrome://inspect` $\rightarrow$ click **"Open dedicated DevTools for Node"**.

---

### 2. Built-in V8 CPU Profiling
Generate flamegraphs without external tools:
```bash
# Starts Node and writes CPU profile on process termination:
node --cpu-prof --cpu-prof-interval=1000 server.js

# Load test the server:
autocannon -c 50 -d 20s http://localhost:3000

# Terminate process (Ctrl+C). A file named CPU.YYYYMMDD.xxx.cpuprofile is generated.
```
Load the `.cpuprofile` file into Chrome DevTools **Performance** tab to visualize execution bottlenecks.

---

### 3. Programmatic Heap Snapshot Generation
Take a heap snapshot from code or an HTTP debug endpoint during high memory usage:
```javascript
import v8 from 'node:v8';
import path from 'node:path';

// Endpoint to generate heap snapshot on demand:
app.post('/admin/debug/heapdump', (req, res) => {
  const filename = path.join(process.cwd(), `heap-${Date.now()}.heapsnapshot`);
  const snapshotPath = v8.writeHeapSnapshot(filename);
  res.json({ message: 'Heap snapshot created', path: snapshotPath });
});
```

---

### 4. Node.js Diagnostic Reports (`process.report`)
Generates a comprehensive diagnostic report containing native stack traces, libuv handles, OS memory, and environment variables:
```bash
# Automatically write diagnostic report on uncaught exceptions or fatal errors:
node --report-uncaught-exception --report-on-signal=SIGUSR2 server.js
```

Or trigger programmatically:
```javascript
process.report.writeReport('diagnostic-report.json');
```

---

### 5. Event Loop Lag Monitoring (`perf_hooks`)
```javascript
import { monitorEventLoopDelay } from 'node:perf_hooks';

const histogram = monitorEventLoopDelay({ resolution: 20 });
histogram.enable();

setInterval(() => {
  const p99 = histogram.percentile(99) / 1e6; // Convert nanoseconds to ms
  console.log(`Event Loop Delay (P99): ${p99.toFixed(2)}ms`);
  histogram.reset();
}, 5000);
```
