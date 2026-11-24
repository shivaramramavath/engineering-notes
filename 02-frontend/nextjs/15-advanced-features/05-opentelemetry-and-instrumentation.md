# OpenTelemetry & Observability (`opentelemetry-and-instrumentation`)

> End-to-end tracing and observability in Next.js using `instrumentation.ts`, OpenTelemetry SDK, distributed tracing across Server Actions, and custom metrics.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Observability**| OpenTelemetry, Prometheus, Jaeger, Datadog |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The Need for Distributed Tracing
Modern Next.js applications involve complex asynchronous execution graphs: edge proxies, server components, nested Suspense boundaries, Server Actions, database queries, and external API requests.
When a user reports that a checkout action took 4.2 seconds, where was the latency spent?
- **`instrumentation.ts`**: The official Next.js hook for bootstrapping tracing and monitoring tools when the server process starts.
- **OpenTelemetry (OTel)**: Vendor-neutral open standard for collecting distributed traces and metrics, exporting seamlessly to Datadog, New Relic, Honeycomb, or self-hosted Jaeger/Prometheus.

```
[ Inbound HTTP Request ] ──> [ proxy.ts Span ]
                                    │
                                    ▼
       [ App Router Root Span (Server Component Render) ]
            ├── Span: verifySession() [12ms]
            ├── Span: db.query("SELECT * FROM products") [45ms]
            └── Span: Redis cache lookup [2ms]
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Manual `console.time` Logging
```tsx
// app/api/checkout/route.ts (Naive)
export async function POST() {
  console.time('checkout-total');
  // Manual un-correlated logs, no context propagation, drops under load
  console.timeEnd('checkout-total');
}
```

### Idiomatic: Standardized OpenTelemetry Tracing
```ts
// instrumentation.ts (Idiomatic)
export async function register() {
  if (process.env.NEXT_RUNTIME === 'nodejs') {
    await import('./instrumentation.node');
  }
}
```
```ts
// instrumentation.node.ts
import { NodeSDK } from '@opentelemetry/sdk-node';
import { OTLPTraceExporter } from '@opentelemetry/exporter-trace-otlp-http';
import { Resource } from '@opentelemetry/resources';
import { SemanticResourceAttributes } from '@opentelemetry/semantic-conventions';

const sdk = new NodeSDK({
  resource: new Resource({
    [SemanticResourceAttributes.SERVICE_NAME]: 'nextjs-saas-core',
  }),
  traceExporter: new OTLPTraceExporter(),
});

sdk.start();
```

---

## 3. Five Progressive Examples

### Example 1: Creating Custom Spans in Server Actions
```ts
// actions/payment.ts
'use server';

import { trace } from '@opentelemetry/api';

const tracer = trace.getTracer('payment-service');

export async function chargeCustomer(amountCents: number) {
  return tracer.startActiveSpan('charge-stripe', async (span) => {
    try {
      span.setAttribute('payment.amount', amountCents);
      const charge = await stripeClient.charges.create({ amount: amountCents });
      span.setStatus({ code: 1 }); // OK
      return { success: true, id: charge.id };
    } catch (err: any) {
      span.recordException(err);
      span.setStatus({ code: 2, message: err.message }); // Error
      throw err;
    } finally {
      span.end();
    }
  });
}
```

### Example 2: Correlating Database Queries with Traces
```ts
// lib/prisma.ts
import { PrismaClient } from '@prisma/client';

export const prisma = new PrismaClient({
  // Built-in OpenTelemetry integration in Prisma
  log: [
    { emit: 'event', level: 'query' },
    { emit: 'stdout', level: 'error' },
  ],
});
```

### Example 3: Context Propagation across External HTTP Calls
```ts
// dal/externalService.ts
import { trace, context, propagation } from '@opentelemetry/api';

export async function fetchWithTracePropagation(url: string) {
  const headers: Record<string, string> = {};
  // Injects traceparent header: 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01
  propagation.inject(context.active(), headers);

  return fetch(url, { headers });
}
```

### Example 4: Recording Metrics with OpenTelemetry
```ts
// lib/metrics.ts
import { metrics } from '@opentelemetry/api';

const meter = metrics.getMeter('nextjs-app-metrics');
const checkoutCounter = meter.createCounter('checkout_requests_total', {
  description: 'Total completed checkout operations',
});

export function recordCheckout() {
  checkoutCounter.add(1, { status: 'success' });
}
```

### Example 5: Health Check & Liveness Endpoint
```ts
// app/api/health/route.ts
import { NextResponse } from 'next/server';
import { db } from '@/db';

export async function GET() {
  try {
    await db.execute('SELECT 1');
    return NextResponse.json({ status: 'healthy', timestamp: new Date().toISOString() });
  } catch (err) {
    return NextResponse.json({ status: 'unhealthy', error: 'Database disconnected' }, { status: 503 });
  }
}
```

---

## 4. Under the Hood & Internals
Next.js has native OpenTelemetry hooks baked directly into its internal HTTP server and React reconciler.
- When an HTTP request enters, Next.js initiates a root span `GET /products/[id]`.
- As Server Components execute, sub-spans are created for `render route (app)`, `resolve page`, and individual dynamic fetches.
- Custom spans created inside your application code attach directly as children of the active Next.js trace context via Node.js `AsyncLocalStorage`.

---

## 5. Security & Observability Hygiene
- **Never Log Secrets or PII**: Ensure OpenTelemetry attributes do not record credit card numbers, passwords, authorization tokens, or personally identifiable information (PII).
- **Sanitize Error Spans**: When catching exceptions, record standard error messages rather than raw database error dumps that might leak internal schema names or connection strings.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Forgetting to call `span.end()` | Premature return | Traces hang and never complete | Always wrap in `try...finally { span.end() }` |
| Importing OTel SDK on Edge | Node-specific OTel modules | Build error on edge runtime | Check `process.env.NEXT_RUNTIME === 'nodejs'` |
| High cardinality attribute | Adding user IDs to metric names | Exploding Prometheus memory/cost | Use bounded dimensions, put IDs in traces |
| Missing traceparent header | Manual fetch without injection | Broken distributed traces across microservices | Use `propagation.inject()` |

---

## 7. Performance & Optimization Guide
Keep trace sampling rates balanced: In high-traffic production systems (10,000+ RPS), set sampling to 5% or 10% (`ParentBasedSampler`) to minimize telemetry bandwidth and serialization overhead on server CPU.

---

## 8. TypeScript & Strict Typing Recipes
```ts
import type { Span } from '@opentelemetry/api';

export type TracedFunction<T> = (span: Span) => Promise<T>;
```

---

## 9. Testing & Quality Verification

```ts
import { describe, it, expect } from 'vitest';
import { register } from './instrumentation';

describe('Instrumentation Registration', () => {
  it('registers without crashing', async () => {
    await expect(register()).resolves.not.toThrow();
  });
});
```

---

## 10. When NOT to Use This Pattern
- For small local hobby prototypes or static sites where server metrics add unnecessary infrastructure overhead.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. What is the role of `instrumentation.ts` in Next.js?</summary>
`instrumentation.ts` is the official hook that executes once when the Next.js server instance boots up, used to initialize monitoring, distributed tracing (OpenTelemetry), and logging agents before handling requests.
</details>

<details>
<summary>2. How does OpenTelemetry correlate sub-spans across Server Components?</summary>
Using Node.js `AsyncLocalStorage`. Next.js binds the active trace context to the asynchronous execution chain of the request, allowing child components and functions to attach spans without manual context passing.
</details>

<details>
<summary>3. What is distributed trace context propagation?</summary>
It is the process of passing the trace identifier (W3C `traceparent` header) across network calls to backend microservices, allowing an entire distributed transaction to be viewed as a single unified trace.
</details>

<details>
<summary>4. Why should trace sampling be used in production?</summary>
Tracing every single request in high-throughput systems generates massive data volumes and increases network overhead. Sampling collects a representative percentage of traces while capturing 100% of errors.
</details>

<details>
<summary>5. How does Next.js handle OpenTelemetry across Node.js vs Edge runtimes?</summary>
In `instrumentation.ts`, you inspect `process.env.NEXT_RUNTIME` to conditionally load the full `@opentelemetry/sdk-node` in Node runtimes while avoiding unsupported native modules on Edge isolates.
</details>

<details>
<summary>6. What is the difference between Traces, Metrics, and Logs?</summary>
Traces track the execution path and timing of a single request across services. Metrics are aggregated numerical measurements over time (e.g. CPU load, request count). Logs are discrete textual records of specific events.
</details>

<details>
<summary>7. How do you track database query timing in Next.js traces?</summary>
By enabling tracing in your ORM (e.g. Prisma's `previewFeatures = ["tracing"]`), which automatically emits spans for every SQL query into the active OpenTelemetry trace.
</details>

<details>
<summary>8. How do you implement a health check endpoint for Kubernetes?</summary>
Create a Route Handler at `app/api/health/route.ts` that queries the database and cache, returning HTTP 200 on success and 503 on service failure.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a health check route</summary>
Build an endpoint returning system uptime and database connectivity status.
</details>

<details>
<summary>Exercise 2 (Medium): Set up instrumentation.ts</summary>
Configure `instrumentation.ts` to log server boot time and runtime environment.
</details>

<details>
<summary>Exercise 3 (Hard): Trace a Server Action with custom spans</summary>
Wrap a multi-step checkout Server Action in an OpenTelemetry active span recording custom attributes.
</details>

---

## 13. Cross-References & Further Reading
- [Production: Self-Hosting & Standalone Deployments](file:///E:/Learning/engineering-notes/02-frontend/nextjs/22-production/README.md)
- [Official Next.js OpenTelemetry Guide](https://nextjs.org/docs/app/building-your-application/optimizing/open-telemetry)
