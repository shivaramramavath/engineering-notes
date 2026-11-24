# Node.js Technical Interview: Architecture Case Studies & Outage Post-Mortems

> Staff & Principal-level interview discussion guides covering real-world production outages, post-mortem incident analyses, architectural migration strategies, and technical trade-off evaluations.

---

### Case Study 1: The High-Traffic Memory Leak Outage (Post-Mortem Analysis)

#### The Scenario:
*"During a Black Friday traffic surge, your Node.js microservice started throwing HTTP 502 Bad Gateway errors every 45 minutes. Kubernetes killed containers due to OOMKilled (Exit Code 137). How did you triage, identify the root cause, and permanently remediate it?"*

#### The Staff-Level Engineering Response:
1. **Immediate Stabilization (Incident Response)**:
   - Temporarily doubled pod memory limits (`resources.limits.memory: 4Gi`) to widen the crash window.
   - Reduced pod restart frequency by configuring horizontal pod autoscaling (HPA) based on memory utilization thresholds (75%) rather than CPU.
2. **Triaging & Reproduction in Staging**:
   - Deployed a single staging container with the Node.js V8 inspector enabled: `node --inspect=0.0.0.0:9229 server.js`.
   - Simulated production traffic using `autocannon` or `k6` to reproduce the linear memory ramp.
3. **Root Cause Analysis (Heap Profiling)**:
   - Collected three Heap Snapshots using Chrome DevTools at 10-minute intervals.
   - Compared Snapshot 3 to Snapshot 1.
   - **The Culprit Found**: A custom logging interceptor was registering an event listener on every inbound HTTP request:
     ```javascript
     // The bug:
     app.use((req, res, next) => {
       eventBus.on('telemetry', (data) => logTelemetry(req.headers, data)); // LEAK!
       next();
     });
     ```
     Because `eventBus` was a long-lived process singleton, each request appended an anonymous closure holding references to `req.headers` and the entire `IncomingMessage` object. Millions of closures were pinned to the Old Generation and could never be garbage-collected.
4. **Remediation & Architectural Safeguards**:
   - Fixed the listener registration using request-scoped disposers.
   - Added `eslint-plugin-node` rules to block event listener creation inside middleware.
   - Integrated continuous memory profiling in CI/CD using Vitest and `process.memoryUsage().heapUsed` bounds.

---

### Case Study 2: The Migration from Express to Fastify

#### The Scenario:
*"We have an Express 4 REST API handling 15,000 req/sec that is CPU-bottlenecked. Walk us through how you would architect and execute a migration to Fastify 5 without downtime."*

#### The Staff-Level Engineering Response:
1. **Business Justification & Benchmarking**:
   - Profiled the existing Express service with clinic.js (Flame & Bubble).
   - Proved that 40% of CPU time was consumed by regex route matching in `path-to-regexp` and runtime JSON serialization via `res.json()`. Fastify's `find-my-way` radix tree and `fast-json-stringify` offered a projected 3x throughput increase on existing hardware.
2. **Strangler Fig Pattern Migration Plan**:
   - Rather than a risky big-bang rewrite, deployed Fastify as an API Gateway in front of Express.
   - Routes were migrated domain by domain (e.g., `/api/v1/catalog` migrated first, while complex `/api/v1/checkout` remained on Express).
3. **Handling Breaking API Incompatibilities**:
   - Middleware signature differences: Express uses `(req, res, next)`, whereas Fastify uses lifecycle hooks (`onRequest`, `preHandler`, `onResponse`) with `reply.send()`.
   - Replaced custom validation middleware with standard Fastify JSON Schemas (`ajv`), unlocking automated OpenAPI spec generation and fast serialization simultaneously.
4. **Validation & Canary Deployment**:
   - Used shadow traffic (traffic mirroring) via Envoy/NGINX to replay 10% of production traffic to the Fastify service in read-only mode, comparing status codes and JSON diffs to ensure 100% behavioral parity.
   - Promoted Fastify to production via a weighted Canary rollout (10% $\rightarrow$ 25% $\rightarrow$ 50% $\rightarrow$ 100%).

---

### Case Study 3: The CommonJS to Native ESM Migration

#### The Scenario:
*"Our large monolithic backend is written in CommonJS. Upstream npm libraries (like `node-fetch`, `execa`, `chalk`, `p-limit`) are now pure ESM packages. How do you plan and execute the migration to ECMAScript Modules?"*

#### The Staff-Level Engineering Response:
1. **The Modern Shortcut (Node.js 22 LTS `require(esm)`)**:
   - First evaluated whether a full rewrite was even necessary: in Node.js 22.12+, CommonJS can synchronously `require()` synchronous ESM packages without breaking.
2. **Full Migration Strategy**:
   - Added `"type": "module"` to root `package.json`.
   - **Replacement of Injected CJS Globals**:
     - `__dirname` $\rightarrow$ `import.meta.dirname` (available natively in modern Node.js) or `fileURLToPath(new URL('.', import.meta.url))`.
     - `__filename` $\rightarrow$ `import.meta.filename`.
   - **Strict Mandatory File Extensions**: In ESM, relative imports require explicit extensions: `import { helper } from './helper.js';`. Automated codemods using `jscodeshift` to append extensions across 500+ source files.
   - **Mocking & Testing Adaptations**: CommonJS tools like `mock-require` or `proxyquire` hijack `require.cache`. Migrated test suites to Vitest / Node.js native test runner (`node:test`), which natively support ESM module mocking via `vi.mock()` or module loader hooks (`module.register()`).
