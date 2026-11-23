# Logging and Observability in TypeScript: Pino and OpenTelemetry

> Production guide to architecting type-safe structured logging, distributed tracing, and metrics collection in TypeScript using Pino, OpenTelemetry SDK, and typed context propagation.

| Metadata | Details |
| :--- | :--- |
| **Category** | Production Tooling |
| **Difficulty** | Advanced |
| **Prerequisites** | [01-fundamentals/06-type-widening-narrowing.md](../01-fundamentals/06-type-widening-narrowing.md), [20-nodejs-backend/00-node-types.md](../20-nodejs-backend/00-node-types.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In distributed cloud environments, unstructured logging via `console.log("Error occurred: " + err)` is catastrophic:
- **Missing Context**: Logs lack correlation IDs, tenant IDs, and machine hostnames, making it impossible to trace user requests across microservices.
- **Untyped Metadata**: Log indexing systems (Datadog, ElasticSearch, CloudWatch) fail to parse inconsistent schema types (e.g. `userId` logged as an integer in one service and a UUID string in another).
- **Severe Performance Bottlenecks**: Synchronous logging blocks the Node.js event loop, degrading high-concurrency throughput.

Modern observability requires:
1. **High-Performance Structured Logging (Pino)**: Generates JSON logs with minimal event loop overhead using worker threads.
2. **Distributed Tracing (OpenTelemetry)**: Correlates requests across service boundaries using standard trace contexts (`traceparent` header).
3. **Type-Safe Context Propagation**: Leverages Node's `AsyncLocalStorage` and TypeScript discriminated interfaces to guarantee that required context fields are attached to every telemetry event.

```
+-------------------------------------------------------------------------+
|                  Type-Safe Telemetry Architecture                       |
+-------------------------------------------------------------------------+
|                                                                         |
|  Incoming HTTP Request (traceparent: 00-4bf92f3577b34da6a3ce929d0e0e4736)
|                                |                                        |
|                                v                                        |
|                  [ AsyncLocalStorage Context ]                          |
|                  traceId, spanId, tenantId, userId                      |
|                                |                                        |
|                +---------------+---------------+                        |
|                |                               |                        |
|                v                               v                        |
|      [ Pino Structured Logger ]      [ OpenTelemetry Tracing ]          |
|      logger.info({ orderId })        tracer.startActiveSpan(...)        |
|                |                               |                        |
|                v                               v                        |
|      stdout (ndjson)                 OTLP Exporter                      |
|      (Datadog / CloudWatch)          (Honeycomb / Jaeger)               |
|                                                                         |
+-------------------------------------------------------------------------+
```

---

## 2. Core Concepts

### Structured JSON Logging with Pino
Pino formats log records directly as newline-delimited JSON (`ndjson`). Pino provides first-class TypeScript support, allowing developers to type custom log bindings and child loggers.

### OpenTelemetry (OTel) Tracing
OpenTelemetry provides open standards for traces, metrics, and logs. In TypeScript:
- **Tracer**: Generates Spans representing units of work.
- **Span**: Contains timestamps, status, attributes, and events.
- **Context**: Carried across asynchronous function boundaries using `AsyncLocalStorage`.

### Type-Safe Context Propagation
Using `AsyncLocalStorage<RequestContext>`, request metadata is implicitly accessible throughout deep call stacks without manual parameter drilling.

---

## 3. How It Works Internally

1. **Pino Serialization**: Pino avoids `JSON.stringify` bottlenecks by using pre-compiled string builders (via `fast-json-stringify`). Child loggers cache serialized static key-value pairs.
2. **Async Context Tracking**: Node's `AsyncLocalStorage` hooks into V8's internal async promise lifecycle (`init`, `before`, `after`, `destroy`). The active context remains bound to the execution chain of `Promise.then` and `await`.
3. **OpenTelemetry SDK**: Wraps global network libraries (`http`, `pg`, `fetch`) via dynamic monkey-patching, extracting W3C trace headers and automatically establishing parent-child span hierarchies.

---

## 4. Syntax & API Reference

### Typed Pino Logger Initialization
```typescript
import pino, { Logger } from "pino";

export interface LogBindings {
  service: string;
  environment: "development" | "production" | "test";
  version: string;
}

export interface RequestLogContext {
  traceId: string;
  userId?: string;
  requestId: string;
}

export const logger: Logger<LogBindings> = pino({
  level: process.env.LOG_LEVEL ?? "info",
  base: {
    service: "payment-gateway",
    environment: (process.env.NODE_ENV as LogBindings["environment"]) ?? "development",
    version: "2.1.0",
  },
  timestamp: pino.stdTimeFunctions.isoTime,
});
```

### OpenTelemetry Tracer Setup (`tracer.ts`)
```typescript
import { trace, Span, Tracer } from "@opentelemetry/api";

const TRACER_NAME = "acme.payment-service";
export const tracer: Tracer = trace.getTracer(TRACER_NAME);

export async function withSpan<T>(
  name: string,
  fn: (span: Span) => Promise<T>
): Promise<T> {
  return tracer.startActiveSpan(name, async (span) => {
    try {
      const result = await fn(span);
      span.setStatus({ code: 1 }); // OK
      return result;
    } catch (error) {
      span.setStatus({ code: 2, message: error instanceof Error ? error.message : "Error" });
      span.recordException(error as Error);
      throw error;
    } finally {
      span.end();
    }
  });
}
```

---

## 5. Practical Examples

### Example 1: AsyncLocalStorage Typed Request Context
```typescript
import { AsyncLocalStorage } from "node:async_hooks";
import { logger } from "./logger.js";

export interface RequestContext {
  readonly correlationId: string;
  readonly userId?: string;
  readonly path: string;
}

export const contextStorage = new AsyncLocalStorage<RequestContext>();

export function getContextLogger() {
  const store = contextStorage.getStore();
  return store ? logger.child(store) : logger;
}
```

### Example 2: Express Observability Middleware
```typescript
import type { Request, Response, NextFunction } from "express";
import crypto from "node:crypto";
import { contextStorage } from "./context.js";
import { getContextLogger } from "./context.js";

export function observabilityMiddleware(req: Request, res: Response, next: NextFunction): void {
  const correlationId = (req.headers["x-correlation-id"] as string) ?? crypto.randomUUID();
  const context = {
    correlationId,
    path: req.path,
  };

  contextStorage.run(context, () => {
    const log = getContextLogger();
    const start = performance.now();

    res.on("finish", () => {
      const duration = (performance.now() - start).toFixed(2);
      log.info({ statusCode: res.statusCode, durationMs: duration }, "HTTP Request Completed");
    });

    next();
  });
}
```

### Example 3: Edge Case - Error Serialization in Pino
```typescript
// Standard JSON.stringify converts Errors into empty objects `{}`!
// Pino requires explicit error serializers or passing error as first argument:

const log = getContextLogger();

try {
  throw new Error("Database connection timed out");
} catch (err) {
  // Correct usage: pass err object to logger
  log.error({ err }, "Database query failure");
}
```

### Example 4: Compiler Error & Typed Log Key Validation
```typescript
interface AuditLogPayload {
  action: "LOGIN" | "LOGOUT" | "TRANSFER";
  actorId: string;
  amount?: number;
}

function writeAuditLog(payload: AuditLogPayload): void {
  logger.info(payload, "Audit event emitted");
}

// @ts-expect-error - TS2322: Type '"DELETE_USER"' is not assignable to type '"LOGIN" | "LOGOUT" | "TRANSFER"'.
writeAuditLog({ action: "DELETE_USER", actorId: "usr_99" });
```

### Example 5: Distributed Tracing with Custom OTel Attributes
```typescript
import { withSpan } from "./tracer.js";

export async function processPayment(orderId: string, amount: number): Promise<string> {
  return withSpan("processPayment", async (span) => {
    span.setAttribute("payment.order_id", orderId);
    span.setAttribute("payment.amount", amount);

    // Call downstream microservice
    const transactionId = `txn_${Date.now()}`;
    span.setAttribute("payment.transaction_id", transactionId);
    return transactionId;
  });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **V8 Async Context Loss**:
   Certain legacy callback APIs or unhandled event emitters can detach the `AsyncLocalStorage` store. Wrap non-standard callbacks with `AsyncLocalStorage.bind(fn)` to preserve trace context.
2. **PII and Secret Leakage**:
   Always configure Pino redact paths (`redact: ['req.headers.authorization', '*.password', '*.creditCard']`) to guarantee sensitive credentials are never written to log stores.
3. **Span Leakage**:
   Failing to invoke `span.end()` inside a `finally` block causes memory leaks and leaves spans open indefinitely in your tracing dashboard.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Using `console.log`** | `console.log("User logged in")` | `logger.info({ userId }, "User logged in")` | Unstructured text, blocks event loop |
| **Empty Error Objects** | `logger.error("Failed: " + err)` | `logger.error({ err }, "Failed")` | Logs missing stack traces |
| **Logging Sensitive Data** | Logging raw request headers | Configure `redact` in Pino options | GDPR / PCI DSS compliance violation |
| **Missing `span.end()`** | Omitting `finally { span.end(); }` | Always end spans in `finally` | Tracing memory leak |
| **Untyped Log Payloads** | Accepting `record: any` | Enforce typed schemas on loggers | Inconsistent telemetry indexing |
| **Drilling Context Manually** | Passing `traceId` to 20 function args | Use `AsyncLocalStorage` | Architectural pollution |

---

## 8. Performance & Type-Checker Complexity

- **Pino Performance**: Pino is up to **5x faster** than Winston and Bunyan due to string interpolation optimization.
- **AsyncLocalStorage Overhead**: Minimal overhead (~1-2% throughput impact under extreme concurrency).
- **Sampling in OTel**: Configure head-based or tail-based sampling (e.g. record 5% of healthy requests and 100% of error requests) to reduce OTLP network bandwidth in high-volume production services.

---

## 9. Compiler Internals

When using TypeScript generics with Pino and OpenTelemetry:
1. `Logger<CustomBindings>` binds custom interface definitions to the compiler's `MethodSignature` mapping.
2. The TypeChecker validates object literal arguments against `LogFn` overloads (`(obj: object, msg?: string, ...args: any[]): void`).
3. Return types like `Promise<T>` in `withSpan<T>` are inferred from the return type of the wrapped callback using contextual typing (`ts.getContextualType`).

---

## 10. Best Practices & Design Guidelines

1. **Always Use Structured JSON**: Ensure logs can be ingested and filtered natively by log aggregators.
2. **Centralize Context in AsyncLocalStorage**: Eliminate parameter drilling for tracing IDs.
3. **Instrument External Calls**: Ensure HTTP, Redis, and SQL calls run inside active OpenTelemetry spans.
4. **Redact Sensitive Information**: Implement global redaction rules for passwords, tokens, and PII.

---

## 11. Real-World Case Study

A fintech payment orchestrator processing 40,000 transactions/minute suffered from intermittent authorization timeouts. Standard logs were flooded with unindexed text strings, making it impossible to diagnose root causes across 8 downstream banking adapters.

**Remediation**:
1. Standardized all logging on **Pino** with `x-correlation-id` stored in `AsyncLocalStorage`.
2. Instrumented HTTP calls using **OpenTelemetry**.
3. Configured Datadog trace-to-log correlation.

**Outcome**: Mean Time to Detect (MTTD) dropped from 4 hours to **90 seconds**. Engineers identified a slow downstream bank endpoint within two clicks on the distributed trace waterfall.

---

## 12. Interview Questions

<details>
<summary>1. Why is Pino significantly faster than traditional Node.js loggers like Winston?</summary>

Pino minimizes event loop blocking by pre-compiling JSON schemas, batching writes, and optionally delegating serialization and I/O writes to separate worker threads (`pino/file` or transport worker threads).
</details>

<details>
<summary>2. How does `AsyncLocalStorage` work in Node.js?</summary>

`AsyncLocalStorage` provides thread-local-like storage for asynchronous execution flows. It hooks into Node's async_hooks engine, ensuring that context initialized at the entry of a request persists across promise resolutions, timeouts, and callbacks without manual passing.
</details>

<details>
<summary>3. What is the W3C `traceparent` header format?</summary>

It is a standardized header format: `version-traceId-parentSpanId-traceFlags` (e.g. `00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01`), enabling distributed tracing across microservices.
</details>

<details>
<summary>4. Why does `console.log(new Error("fail"))` print a stack trace, but `JSON.stringify(new Error("fail"))` outputs `{}`?</summary>

In JavaScript, `message`, `stack`, and `name` are non-enumerable properties on the `Error` prototype. `JSON.stringify` only serializes enumerable own properties. Pino provides a custom serializer (`pino.stdSerializers.err`) that explicitly extracts these fields.
</details>

<details>
<summary>5. What is the difference between Tracing and Logging?</summary>

Logging records discrete timestamped events with metadata. Tracing tracks the end-to-end journey of a request as it moves through various components, measuring durations and parent-child causality across distributed networks.
</details>

<details>
<summary>6. How can you redact sensitive keys in Pino?</summary>

By passing the `redact` option to the Pino constructor: `pino({ redact: ['req.headers.authorization', 'user.password'] })`.
</details>

<details>
<summary>7. Why must `span.end()` always be executed in a `finally` block?</summary>

If an unhandled exception occurs inside a span, omitting the `finally` block leaves the span open permanently in memory, leaking resources and skewing latency metrics in telemetry dashboards.
</details>

<details>
<summary>8. How do child loggers enhance observability in Pino?</summary>

Child loggers inherit configuration from the parent logger while attaching static, pre-serialized contextual metadata (e.g. `module: "billing"` or `requestId: "123"`), eliminating repetitive key-value typing on every log call.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Create a typed child logger</summary>

**Task**: Create a child logger from an existing Pino logger that attaches a static `component: "AuthService"` property.

**Solution**:
```typescript
import pino from "pino";

const rootLogger = pino();
const authLogger = rootLogger.child({ component: "AuthService" });

authLogger.info("Auth service initialized");
```
</details>

<details>
<summary>Challenge 2 (Easy): Redact authorization headers in Pino</summary>

**Task**: Configure a Pino instance that automatically redacts `authorization` and `cookie` headers.

**Solution**:
```typescript
import pino from "pino";

export const secureLogger = pino({
  redact: ["headers.authorization", "headers.cookie"],
});
```
</details>

<details>
<summary>Challenge 3 (Medium): Typed AsyncLocalStorage Wrapper</summary>

**Task**: Implement a utility class `TraceContextHolder` using `AsyncLocalStorage` that stores and retrieves a typed `TraceContext` object.

**Solution**:
```typescript
import { AsyncLocalStorage } from "node:async_hooks";

export interface TraceContext {
  traceId: string;
  spanId: string;
}

export class TraceContextHolder {
  private static storage = new AsyncLocalStorage<TraceContext>();

  public static run<R>(ctx: TraceContext, fn: () => R): R {
    return this.storage.run(ctx, fn);
  }

  public static get(): TraceContext | undefined {
    return this.storage.getStore();
  }
}
```
</details>

<details>
<summary>Challenge 4 (Medium): Safe Error Logging Serializer</summary>

**Task**: Write a function that safely formats unknown caught exceptions into serializable log objects containing name, message, and stack trace.

**Solution**:
```typescript
export interface SerializedError {
  type: string;
  message: string;
  stack?: string;
}

export function serializeException(error: unknown): SerializedError {
  if (error instanceof Error) {
    return {
      type: error.name,
      message: error.message,
      stack: error.stack,
    };
  }
  return {
    type: "UnknownError",
    message: String(error),
  };
}
```
</details>

<details>
<summary>Challenge 5 (Hard): Type-Safe Method Decorator for OpenTelemetry Spans</summary>

**Task**: Write a TypeScript method decorator `@Trace(spanName)` that automatically wraps class method execution in an OpenTelemetry active span.

**Solution**:
```typescript
import { trace } from "@opentelemetry/api";

export function Trace(spanName?: string) {
  return function (
    target: any,
    propertyKey: string,
    descriptor: PropertyDescriptor
  ) {
    const originalMethod = descriptor.value;
    const tracer = trace.getTracer("default");

    descriptor.value = async function (...args: any[]) {
      const name = spanName ?? `${target.constructor.name}.${propertyKey}`;
      return tracer.startActiveSpan(name, async (span) => {
        try {
          const result = await originalMethod.apply(this, args);
          span.setStatus({ code: 1 });
          return result;
        } catch (error) {
          span.setStatus({ code: 2, message: error instanceof Error ? error.message : "Error" });
          span.recordException(error as Error);
          throw error;
        } finally {
          span.end();
        }
      });
    };
    return descriptor;
  };
}
```
</details>

<details>
<summary>Challenge 6 (Hard): High-Throughput OTel Batch Metric Exporter</summary>

**Task**: Design an asynchronous metrics accumulator class in TypeScript that batches counter increments in memory and flushes them periodically to an OTLP metrics endpoint.

**Solution**:
```typescript
export class MetricAccumulator {
  private counters = new Map<string, number>();
  private timer: NodeJS.Timeout;

  constructor(private flushIntervalMs = 5000) {
    this.timer = setInterval(() => this.flush(), this.flushIntervalMs);
  }

  public increment(metric: string, count = 1): void {
    const current = this.counters.get(metric) ?? 0;
    this.counters.set(metric, current + count);
  }

  private flush(): void {
    if (this.counters.size === 0) return;
    const snapshot = new Map(this.counters);
    this.counters.clear();

    // Transmit snapshot asynchronously
    console.log("Flushing metrics batch:", Object.fromEntries(snapshot));
  }

  public dispose(): void {
    clearInterval(this.timer);
    this.flush();
  }
}
```
</details>
