# Modular Monolith vs Microservices

How to split a growing system into pieces — and, just as important, when *not* to split it into separate deployable services.

## The spectrum

Architecture at the system level isn't a binary choice. It's a spectrum of how strongly you separate parts of the application:

```
 Big ball of mud ──▶ Layered monolith ──▶ Modular monolith ──▶ Service-based ──▶ Microservices
 (everything          (technical           (feature modules      (a few larger     (many small
  imports              layers, but          with enforced         services)         independent
  everything)          features tangled)    boundaries)                             services)

 ◀── simpler to build, run, debug                    more independence, more operational cost ──▶
```

| Term | Meaning |
|---|---|
| **Monolith** | One deployable unit: one codebase, one process (or N identical copies), usually one database |
| **Modular monolith** | A monolith whose *internals* are divided into well-bounded modules with explicit interfaces between them |
| **Microservices** | Many small services, each **independently deployable**, owning its own data, communicating over the network |
| **Distributed monolith** | The worst of both: separate services that are so tightly coupled they must be deployed and changed together |

"Monolith" is not an insult, and "microservices" is not a goal. The real question is: *what boundaries does my system need, and what does it cost to enforce each one?*

---

## The monolith

```
┌──────────────────────────────────────────────┐
│                 One process                  │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌───────┐  │
│  │ Users  │ │ Orders │ │Billing │ │ Email │  │
│  └───┬────┘ └───┬────┘ └───┬────┘ └───┬───┘  │
│      └──────────┴──────────┴──────────┘      │
│           function calls (in-memory)         │
└───────────────────────┬──────────────────────┘
                        ▼
                ┌──────────────┐
                │  One database │
                └──────────────┘
```

### What's good about it

- **Simple to develop:** one repo, one IDE window, one `npm start`.
- **Simple to deploy:** ship one artifact; rollback is one action.
- **Fast calls:** in-process function calls take nanoseconds and never fail because of the network.
- **Easy transactions:** a single database transaction spans any data; no distributed consistency headaches.
- **Easy debugging:** one stack trace, one log stream, step through code in a debugger.
- **Cheap to run:** fewer servers, no service mesh, no inter-service auth.
- **Easy refactoring:** move code between modules with an IDE rename.

### Where it hurts (at scale)

- Everyone deploys together: one team's bug blocks everyone's release.
- Build and test times creep up with codebase size.
- Without discipline, modules become entangled: a change in `billing` breaks `users`.
- One component's load problem (image processing, report generation) affects everything; you must scale the whole app.
- Large teams step on each other in the same code.

Most of these pains are about **coupling and team coordination**, not about the monolith itself, and a *modular* monolith addresses many of them.

---

## The modular monolith

Keep **one deployable**, but organize the code into **modules with real boundaries**, each aligned to a business capability (users, catalog, orders, billing), not a technical layer.

```
src/
├── modules/
│   ├── users/
│   │   ├── index.js              ← the module's PUBLIC API (the only file others may import)
│   │   ├── users.service.js
│   │   ├── users.repository.js
│   │   ├── users.routes.js
│   │   └── internal/             ← private details
│   ├── orders/
│   │   ├── index.js
│   │   └── ...
│   ├── billing/
│   │   ├── index.js
│   │   └── ...
│   └── notifications/
│       ├── index.js
│       └── ...
├── shared/                       ← truly cross-cutting: errors, logger, config, event bus
├── app.js
└── server.js
```

### The rules that make it "modular"

1. **Each module has a public interface** (`index.js`) exporting only what other modules may use, such as service functions and event names.
2. **No reaching into another module's internals.** `orders` imports `users`' public API, never `users.repository.js`.
3. **Each module owns its data.** Only `orders` reads and writes the `orders` tables. Others ask `orders` via its API.
4. **No cross-module joins or foreign-key reads in SQL** (or keep them deliberate and minimal). If `billing` needs a user's email, it calls `users.getEmail(id)` instead of `JOIN users`.
5. **Dependencies are acyclic.** If `orders` → `users`, then `users` must not import `orders`.
6. **Communicate by API call or by event,** not by sharing tables or objects.

```js
// modules/users/index.js: the public surface
export { makeUserService } from "./users.service.js";
export { USER_EVENTS } from "./events.js";
// NOT exported: repository, SQL, internal helpers
```

```js
// modules/orders/orders.service.js
import { userApi } from "../users/index.js";          // ✅ public API

// import { userRepository } from "../users/users.repository.js";   // ❌ private
```

### Enforce it, don't just agree on it

Boundaries erode unless a tool fails the build when they're crossed:

- **`dependency-cruiser`:** declarative rules (`modules/orders` may not import `modules/users/internal/**`).
- **`eslint-plugin-boundaries`** / `eslint-plugin-import` (`no-restricted-paths`).
- **Node's `package.json` `"exports"`** if each module is a workspace package (npm workspaces):

```jsonc
// modules/users/package.json
{
  "name": "@app/users",
  "type": "module",
  "exports": { ".": "./index.js" }       // deep imports like @app/users/internal/x.js now FAIL
}
```

Run the check in CI (`16-production/05-ci-cd.md`).

### Communicating between modules

**Synchronous: call the public API** (simple; use for queries and things that must happen now):

```js
const user = await userApi.getById(order.userId);
```

**Asynchronous: in-process events** (decouples; use for "something happened" reactions):

```js
// shared/eventBus.js
import { EventEmitter } from "node:events";
export const eventBus = new EventEmitter();
```

```js
// orders publishes; it doesn't know who listens
eventBus.emit("order.placed", { orderId: order.id, userId: order.userId, totalCents: order.totalCents });

// notifications subscribes
eventBus.on("order.placed", async ({ orderId, userId }) => {
  await notificationService.sendOrderConfirmation(userId, orderId);
});
```

This uses the `events` module (`02-core-modules/04-events.md`). Caveats: listener errors must be caught (an unhandled rejection in a listener can crash the process), handlers run in the same process (no durability if it crashes mid-way), and event order/side effects need care. For anything that must survive a crash, write the event to an **outbox table** in the same transaction and have a worker publish it (`11-async-processing/`).

### Why a modular monolith is often the best default

- You get **most of the organizational benefits** of microservices (clear ownership, limited blast radius of changes, parallel team work) with **none of the distributed-systems cost**.
- Modules are **pre-cut seams**: if one genuinely needs to become a service later, the boundary already exists and the API is already defined.
- You can **defer the expensive decision** until you have real evidence (traffic numbers, team size, pain) instead of guessing.

---

## Microservices

```
   ┌────────┐     HTTP/gRPC      ┌────────┐
   │ Users  │◀──────────────────▶│ Orders │
   │ service│                    │ service│
   └───┬────┘                    └───┬────┘
       │ owns                        │ owns              ┌──────────────┐
   ┌───▼────┐                    ┌───▼────┐   events     │ Message      │
   │Users DB│                    │Orders DB│◀───────────▶│ broker       │
   └────────┘                    └────────┘              │ (Kafka/Rabbit)│
                                                          └──────▲───────┘
   ┌────────┐     ┌─────────┐   ┌──────────┐                     │
   │Billing │     │ API     │   │ Notif.   │─────────────────────┘
   │ service│     │ gateway │   │ service  │
   └────────┘     └─────────┘   └──────────┘
```

### Defining characteristics

- **Independently deployable:** deploy `orders` without touching `users`
- **Owns its data:** each service has its own database; no shared tables
- **Communicates over the network:** HTTP/REST, gRPC, or asynchronous messaging
- **Organized around business capabilities,** typically owned by one small team ("you build it, you run it")
- **Technology freedom:** each service can choose its language, framework, and datastore (used with restraint)

### What you genuinely gain

- **Independent deployment and release cadence:** teams ship without coordinating
- **Independent scaling:** scale the image-processing service to 50 instances and leave billing at 2
- **Fault isolation:** a crash or memory leak in one service doesn't necessarily take down others (if designed well)
- **Team autonomy at large scale:** dozens or hundreds of engineers working without merge conflicts
- **Technology fit:** a Python ML service beside Node APIs

### What you pay (the part people underestimate)

| Cost | Why it hurts |
|---|---|
| **Network is unreliable** | Every call can time out, fail, arrive twice, or be slow. You need timeouts, retries, circuit breakers, idempotency (`09-api-development/06-idempotency.md`) |
| **No cross-service transactions** | "Reduce stock and create order atomically" becomes a **saga**: a sequence of local transactions with compensating actions if a step fails. Eventual consistency everywhere |
| **Data can't be joined** | Reports spanning services need API composition, event-driven replicated read models, or a data warehouse |
| **Operational overhead** | CI/CD per service, container orchestration (Kubernetes), service discovery, config, secrets, API gateway, per-service monitoring |
| **Observability is mandatory** | A single request touches 6 services; without distributed tracing and correlation IDs you can't debug (`14-logging-observability/02-correlation-id.md`, `04-tracing-and-opentelemetry.md`) |
| **Testing is harder** | Integration tests across services, contract testing, local dev needs many things running |
| **Versioning between services** | Every service API is a contract (`09-api-development/02-versioning-and-pagination.md`); you must support old and new consumers during rollouts |
| **Latency** | What was a nanosecond function call is now milliseconds over a network, multiplied along call chains |
| **Organizational overhead** | Ownership, on-call, documentation, API governance |

### The fallacies of distributed computing

Microservices turn every function call into a distributed-systems problem. The classic false assumptions: *the network is reliable; latency is zero; bandwidth is infinite; the network is secure; topology doesn't change; there is one administrator; transport cost is zero; the network is homogeneous.* Every one of them is false, and each one bites.

---

## Handling distributed transactions: the saga pattern

In the monolith, placing an order is one database transaction. Across services, it isn't.

```
PlaceOrder saga (choreography via events)

 Orders service         Inventory service        Payment service
      │                        │                        │
  create order (pending) ──▶ order.created             │
      │                  reserve stock ──▶ stock.reserved ──▶
      │                        │                   charge card
      │                        │                        │
      │ ◀──────────────────────┼──────── payment.succeeded
  mark order confirmed         │                        │

 If payment FAILS:
      │                        │ ◀── payment.failed ────┤
      │                  release stock (compensate)     │
  mark order cancelled  ◀── stock.released
```

- **Choreography:** services react to each other's events (no central coordinator). Simple to start; hard to see the whole flow.
- **Orchestration:** a central orchestrator tells each service what to do next. The flow is explicit and easier to reason about.
- Every step and compensation must be **idempotent**, and messages are **at-least-once**. The **outbox pattern** (write the event in the same local transaction as the state change) avoids lost messages.

This is considerably more complex than `BEGIN … COMMIT`. If your business operations mostly need strong consistency across the data, that's a strong signal that those pieces belong **in the same service**.

---

## Comparison

| Aspect | Monolith | Modular monolith | Microservices |
|---|---|---|---|
| Deployment units | 1 | 1 | Many |
| Team size sweet spot | 1–10 | 3–40 | 20+ (multiple teams) |
| Inter-component calls | In-process | In-process via public APIs | Network (HTTP/gRPC/events) |
| Transactions | Trivial (one DB) | Easy (one DB, module-owned tables) | Sagas, eventual consistency |
| Data ownership | Shared tables | Module-owned tables/schemas | Separate databases |
| Independent scaling | Whole app only | Whole app only | Per service |
| Independent deploys | No | No | Yes |
| Debugging | Easiest | Easy | Hard: needs tracing |
| Operational complexity | Low | Low | High |
| Boundary enforcement | Convention | Tooling + discipline | Network (physically enforced) |
| Risk | Tangled code | Boundaries eroding | Distributed monolith, ops overload |

---

## How to decide

### Start from the problem, not the trend

Microservices solve **organizational scaling** and **independent scaling/deployment** problems. If you don't have those problems, you'll pay the costs and receive none of the benefits.

| If your situation is... | Then consider... |
|---|---|
| New product, small team (1–10), unclear domain | **Monolith** (modular from day one) |
| Growing team, code getting tangled, single deploy still OK | **Modular monolith** with enforced boundaries |
| One component has very different scaling/resource needs (video transcoding, ML inference, PDF rendering) | **Extract just that piece** as a separate service or worker (`11-async-processing/`) |
| Multiple teams blocked by each other's release schedules | Extract services along team/domain lines |
| Different compliance/security boundaries (payments, PII) | Isolate that capability |
| Different technology requirements (Python ML, Go proxy) | Separate service for that piece |
| "Netflix does it" | Not a reason |

### Questions to ask before splitting

1. **Is the boundary real?** Can you describe this capability in one sentence, with a clear owner and its own data? If two candidates always change together, they're one thing.
2. **Do we need independent deploys or scaling for it,** with evidence (metrics, incident history), not speculation?
3. **Can we operate it?** CI/CD, monitoring, tracing, on-call, secrets, and infrastructure per service. Do we have people and platform for that?
4. **What happens when it's down?** Define fallback behavior (cache, queue, degraded mode).
5. **How will data be handled?** Who owns it? How do other services get what they need without coupling to its schema?
6. **Is the team big enough** that the coordination saved exceeds the overhead added?

### A common rule of thumb

> "Don't start with microservices. Build a well-structured modular monolith, and extract services when a specific, measurable pain demands it." (the "monolith first" approach)

The reverse path (merging badly-cut microservices back together) is much more painful than extracting a service from a clean modular monolith.

---

## Extracting a module into a service (the strangler approach)

If and when you must split, do it incrementally:

1. **Make the boundary clean inside the monolith first:** public API only, owned tables, no cross-module joins. (If this is hard inside one codebase, it will be much worse across a network.)
2. **Put an interface in front of the module** so callers use an abstraction (`userApi.getById`), not its internals.
3. **Replace the implementation behind the interface** with a network client that calls the new service:

```js
// Before: in-process
export const userApi = makeUserService({ userRepository });

// After: same interface, but implemented by an HTTP client
export const userApi = {
  async getById(id) {
    const res = await fetch(`${env.USERS_SERVICE_URL}/users/${id}`, {
      signal: AbortSignal.timeout(2000),             // ALWAYS a timeout on network calls
      headers: { "X-Request-Id": currentRequestId() },
    });
    if (res.status === 404) return null;
    if (!res.ok) throw new UpstreamError("users-service", res.status);
    return res.json();
  },
};
```

4. **Migrate the data:** the new service gets its own database; move or replicate the tables; dual-write or use change-data-capture during the transition.
5. **Route traffic gradually** (feature flag, percentage rollout, gateway routing), and keep the old path until you trust the new one.
6. **Remove the old code** once traffic is fully shifted.

Because your callers already talk to an interface, most of the application never notices.

### Shared code and libraries

Microservices tempt teams to share "common" code via internal packages. Shared **utilities** (logger, config loader) are fine; shared **domain models and database schemas** re-couple the services so they must all be upgraded together, the signature of a distributed monolith. Prefer versioned API contracts over shared code.

---

## Communication styles between services

| Style | How | Use when | Watch out for |
|---|---|---|---|
| **Synchronous HTTP/REST** | Request/response | Queries, user-facing flows needing an immediate answer | Latency chains, cascading failures; use timeouts, retries with backoff, circuit breakers |
| **gRPC** | Binary RPC over HTTP/2, typed contracts | Internal high-throughput calls | Tooling, browser support needs a gateway |
| **Async messaging (queues)** | Producer → broker → consumer (RabbitMQ, SQS, BullMQ) | Commands that can be processed later; work distribution | At-least-once delivery → idempotent consumers (`11-async-processing/02-workers-retry-dlq.md`) |
| **Event streaming** | Publish facts to a log (Kafka) | Many consumers, replayable history, data pipelines | Ordering per partition, schema evolution (`11-async-processing/04-kafka.md`) |

**Prefer asynchronous events for notifying**, and **synchronous calls only when you truly need the answer now.** A chain of synchronous calls (`A → B → C → D`) has the availability of the *product* of all four, which is worse than any single one.

### Resilience basics for any network call

```js
// timeouts on everything
const res = await fetch(url, { signal: AbortSignal.timeout(2000) });

// retry only idempotent calls; exponential backoff + jitter
// circuit breaker: stop calling a failing dependency for a cool-down period (library: opossum)
// bulkheads: limit concurrency per dependency so one slow service can't exhaust your connections
// fallbacks: cached or degraded responses when a dependency is down
```

---

## Shared concerns in a multi-service world

| Concern | Approach |
|---|---|
| **Entry point** | API gateway / reverse proxy: routing, TLS, rate limiting (`16-production/04-nginx.md`) |
| **Authentication** | Verify tokens at the gateway or each service; propagate identity with signed JWTs (`08-authentication-security/02-jwt-and-tokens.md`) |
| **Service-to-service auth** | mTLS or signed service tokens; never trust "internal" traffic blindly |
| **Config & secrets** | Central secret manager, per-service config (`16-production/01-environment-management.md`) |
| **Observability** | Structured logs with correlation IDs, metrics, distributed traces (`14-logging-observability/`) |
| **Contracts** | OpenAPI/AsyncAPI specs, consumer-driven contract tests (Pact) |
| **Deployment** | Containers, CI/CD per service (`16-production/03-docker-and-compose.md`, `05-ci-cd.md`) |
| **Data** | Database per service; replicate via events, not shared tables |

---

## Anti-patterns

```
❌ Distributed monolith      Services share a database or must deploy together; all the cost, no independence
❌ Nano-services             A service per function/table; network overhead dwarfs the benefit
❌ Shared database           Two services read/write the same tables → hidden coupling, schema changes break both
❌ Synchronous call chains   A → B → C → D on every request; one slow link slows everything
❌ Premature splitting       Microservices on day one, before the domain boundaries are understood
❌ Ignoring ops cost         No tracing/CI/monitoring, but 12 services
❌ Shared domain library     A "common" package containing models every service imports and must upgrade in lockstep
❌ Boundaries by technical layer   "auth-service, database-service, validation-service" instead of business capabilities
❌ Modular monolith without enforcement   Folders that look like modules but import each other freely
```

### Drawing boundaries: use the domain

Split along **business capabilities / bounded contexts** (Domain-Driven Design): *Catalog, Ordering, Billing, Shipping, Identity*, each with its own language and rules. "Customer" in Billing (payment methods, invoices) differs from "Customer" in Shipping (addresses, delivery windows), which is a healthy sign the contexts should own their own models.

Good seams tend to be: separate teams, separate rates of change, separate data, different scaling needs, and low chattiness between the two sides. If two parts talk to each other constantly, the boundary is probably in the wrong place.

---

## A practical evolution path

```
Stage 1: Small team, new product
    → Monolith with layered structure + feature folders (01, 03, 04)

Stage 2: Codebase/team growing
    → Modular monolith: public module APIs, owned tables, enforced by tooling

Stage 3: Specific pain points appear
    → Offload heavy/slow work to background workers (queues: 11-async-processing/)
    → Extract the one or two modules with distinct scaling/team needs into services

Stage 4: Many teams, large platform
    → Service-oriented / microservices with platform support (gateway, tracing, CI/CD templates, contracts)
```

Most successful systems spend most of their life in stages 1–3, and plenty of large, profitable products run on well-structured monoliths.

---

## Checklist

**Before choosing microservices**
- [ ] Clear, evidence-backed need for independent deploys or scaling
- [ ] Team size and organization justify it
- [ ] Platform capabilities exist: CI/CD, monitoring, tracing, secrets, orchestration
- [ ] Domain boundaries understood (you've already lived with them as modules)
- [ ] Plan for data ownership and eventual consistency

**For a modular monolith**
- [ ] Modules aligned to business capabilities, not technical layers
- [ ] Each module has a public `index.js`; internals are private
- [ ] Each module owns its tables; no cross-module joins
- [ ] Dependency graph is acyclic
- [ ] Boundaries enforced in CI (`dependency-cruiser`, ESLint rules)
- [ ] Cross-module communication via public API or events

**For any network call between services**
- [ ] Timeouts, bounded retries with backoff, idempotency
- [ ] Circuit breaker / fallback behavior defined
- [ ] Correlation ID propagated; traces enabled
- [ ] Contract documented and versioned

## Wrap-up

This completes `10-architecture/`. The through-line: **separate what changes for different reasons** (HTTP vs rules vs storage), **point dependencies toward the business rules**, **inject what varies**, and **draw system boundaries only where they earn their cost**.

## Next

Section **`11-async-processing/`**: queues, workers, retries, scheduled jobs, and Kafka. These are the tools you'll use to move slow and unreliable work out of the request cycle, and to connect modules and services without tight coupling.
