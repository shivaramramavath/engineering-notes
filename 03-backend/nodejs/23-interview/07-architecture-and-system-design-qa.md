# Node.js Technical Interview: Architecture & System Design Q&A

> Staff-level architectural questions on Hexagonal Architecture, CQRS, Circuit Breaker resilience, and distributed idempotency patterns in enterprise Node.js systems.

---

### Q1: What is Hexagonal Architecture (Ports and Adapters) in Node.js, and how does it prevent vendor lock-in and enable testability?

#### In-Depth Answer
Hexagonal Architecture decouples the core domain and business rules from external technology concerns (frameworks, databases, message brokers, external APIs).

```
+----------------------------------------------------------------------------------------------------+
|                                    Hexagonal Architecture in Node.js                               |
|                                                                                                    |
|  [ Inbound Adapters ]                                                         [ Outbound Adapters ]|
|                                                                                                    |
|  +─────────────────────────+                                         +──────────────────────────+  |
|  | Express 5 Controller    |                                         | PostgreSQL (Drizzle/pg)  |  |
|  +───────────┬─────────────+                                         +─────────────▲────────────+  |
|              │ Implements                                                          │ Implements    |
|              ▼                                                                     │               |
|  +──────────────────────────────────+                       +──────────────────────┴────────────+  |
|  |     Inbound Port (Use Case)      |                       |    Outbound Port (Repository)     |  |
|  | interface CreateOrderUseCase {..}|                       |  interface OrderRepository {..}   |  |
|  +──────────────────┬───────────────+                       +──────────────────────▲────────────+  |
|                     │                                                              │               |
|                     ▼ Calls                                                        │ Injected Into |
|         +──────────────────────────────────────────────────────────────────────────┴────+          |
|         |                     Domain Core (Entities & Business Rules)                   |          |
|         |  - Pure TypeScript/JavaScript with zero third-party dependencies              |          |
|         |  - Class Order { calculateTax(), applyDiscount() }                            |          |
|         +───────────────────────────────────────────────────────────────────────────────+          |
+----------------------------------------------------------------------------------------------------+
```

1. **Domain Core**: Contains pure business rules, entities, and value objects. Contains **zero dependencies** on Express, Postgres, AWS, or npm database packages.
2. **Ports**: Contracts (TypeScript interfaces) defining how the domain receives input (Inbound Ports / Use Cases) and how it persists or emits side-effects (Outbound Ports / Repositories).
3. **Adapters**: Concrete implementations of the ports:
   - *Inbound Adapters*: Express controllers, Fastify routes, CLI commands, AWS Lambda handlers.
   - *Outbound Adapters*: Drizzle ORM PostgreSQL repository, SendGrid email client, Stripe payment adapter.

#### Why it matters in interviews:
- **Testability**: The entire business domain can be unit-tested in 5 milliseconds by passing in an in-memory mock repository (`InMemoryOrderRepository`) without starting PostgreSQL or Docker.
- **Swappability**: Migrating from Express to Fastify, or from PostgreSQL to DynamoDB, requires rewriting only the adapter layer; the domain business logic remains 100% untouched.

---

### Q2: How do you design and implement an Idempotent API in Node.js to handle duplicate network retries?

#### In-Depth Answer
When a mobile client calls `POST /api/v1/payments`, a network timeout may occur after the server charged the card but before the client received the HTTP 200 response. If the client retries, the user must not be charged twice.

#### The Distributed Idempotency Pattern (IETF Draft `Idempotency-Key`):
1. Client generates a unique v4 UUID and sends it in the header: `Idempotency-Key: 7b8a-92c1-...`.
2. The Node.js server extracts the key and attempts an atomic lock in Redis:
   ```javascript
   const lockAcquired = await redis.set(
     `idempotency:${key}`,
     JSON.stringify({ status: 'PROCESSING' }),
     'NX', // Only set if key does not exist
     'EX', 120 // 2-minute lock timeout
   );
   ```
3. **Branch 1: Lock Failed (Key Already Exists)**:
   - Read the existing record from Redis:
   - If `status === 'PROCESSING'`: Another concurrent request is currently handling this exact payment. Return HTTP `409 Conflict` or poll until finished.
   - If `status === 'COMPLETED'`: The operation was already processed! **Return the exact cached response payload and status code immediately** without executing the business logic again.
4. **Branch 2: Lock Acquired**:
   - Execute the database transaction and charge the credit card.
   - Update Redis with the final response:
     ```javascript
     await redis.set(
       `idempotency:${key}`,
       JSON.stringify({ status: 'COMPLETED', statusCode: 201, body: responsePayload }),
       'EX', 86400 // Keep cache for 24 hours
     );
     ```
   - Return HTTP 201 to the client.

---

### Q3: What is the Circuit Breaker pattern? How does it protect a Node.js microservice from cascading failures?

#### In-Depth Answer
When downstream services (e.g. payment gateway or recommendation engine) fail or suffer extreme latency, calling services continue to send requests, piling up awaiting sockets, consuming V8 memory, and eventually crashing the entire microservice ecosystem.

#### The 3 Circuit Breaker States:
1. **Closed (Normal Operation)**:
   - All requests pass through to the downstream service.
   - Successes and failures are recorded in a rolling time window (e.g. last 10 seconds).
   - If failure rate exceeds threshold (e.g. 50% errors), the breaker trips to **Open**.
2. **Open (Failing Fast)**:
   - The breaker immediately rejects all incoming calls without even attempting network I/O (`Error: Circuit Breaker Open`).
   - Prevents overwhelming the struggling downstream service and saves local event loop cycles.
   - Executes an optional fallback (e.g. returning cached stale data or default empty recommendations).
   - A reset timer starts (e.g. 30 seconds).
3. **Half-Open (Testing Upstream Health)**:
   - When the reset timer expires, the breaker transitions to Half-Open.
   - A small, limited number of trial requests are permitted through.
   - If the trial requests succeed, the breaker resets to **Closed**.
   - If any trial request fails, it immediately reverts to **Open** for another 30 seconds.

```javascript
// Implementation using opossum library:
import CircuitBreaker from 'opossum';

const options = {
  timeout: 3000,           // If operation takes longer than 3s, trigger failure
  errorThresholdPercentage: 50, // When 50% of requests fail, trip breaker
  resetTimeout: 30000,      // Wait 30s before trying Half-Open
};

export const paymentBreaker = new CircuitBreaker(callStripeApi, options);
paymentBreaker.fallback(() => ({ status: 'queued', message: 'Payment queued for later processing' }));
```
