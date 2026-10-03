# Strategy and Observer

Both are **behavioral patterns** — they're about how pieces of code *interact* and who is responsible for what. Strategy lets you swap *how* something is done. Observer lets you broadcast *that* something happened without knowing who cares.

---

# Part 1 — Strategy

## The problem

You have one task with several interchangeable ways to do it: calculating shipping cost, charging a payment, authenticating a user, compressing a file. The naive approach is a growing `if/else` chain:

```js
function calculateShipping(order, method) {
  if (method === "standard") {
    return order.weightKg * 1.2;
  } else if (method === "express") {
    return order.weightKg * 2.5 + 10;
  } else if (method === "overnight") {
    return order.weightKg * 4 + 25;
  } else if (method === "pickup") {
    return 0;
  }
  throw new Error("Unknown method");
}
```

Every new method means editing this function; the function slowly accumulates every rule in the business; testing one rule means going through the whole chain.

## The pattern: one interchangeable function per behavior

A **strategy** is just a self-contained algorithm behind a common interface. The code that *uses* it (the "context") doesn't know which one it has.

In JavaScript, a strategy is usually just a **function** — no classes required:

```js
// shipping/strategies.js
export const shippingStrategies = {
  standard:  (order) => order.weightKg * 1.2,
  express:   (order) => order.weightKg * 2.5 + 10,
  overnight: (order) => order.weightKg * 4 + 25,
  pickup:    () => 0,
};

export function calculateShipping(order, method) {
  const strategy = shippingStrategies[method];
  if (!strategy) throw new Error(`Unknown shipping method: ${method}`);
  return strategy(order);
}
```

Adding `"drone"` delivery means adding one entry. `calculateShipping` never changes, and each strategy can be unit tested alone (`13-testing/01-unit-and-integration-testing.md`).

## Strategy with classes — when strategies carry state or setup

If a strategy needs configuration or multiple methods, use objects sharing the same method names:

```js
class StripePayment {
  constructor(apiKey) { this.client = new StripeClient(apiKey); }
  async charge(amountCents, token) {
    const res = await this.client.charges.create({ amount: amountCents, source: token });
    return { id: res.id, status: res.status };
  }
}

class PayPalPayment {
  constructor(clientId, secret) { this.client = new PayPalClient(clientId, secret); }
  async charge(amountCents, token) {
    const res = await this.client.payments.execute(token, amountCents / 100);
    return { id: res.transactionId, status: res.state };
  }
}

class CheckoutService {
  constructor(paymentStrategy) {
    this.payment = paymentStrategy;        // injected — any object with charge()
  }
  async checkout(order, token) {
    return this.payment.charge(order.totalCents, token);
  }
}

const checkout = new CheckoutService(new StripePayment(process.env.STRIPE_KEY));
```

`CheckoutService` depends on a *contract* (`charge(amount, token) → { id, status }`), not on Stripe. Swap in PayPal, or a fake for tests, with no change to the service. This is exactly **dependency injection** (`10-architecture/04-dependency-injection.md`) in action.

Note that both classes return the **same shape** even though the underlying APIs differ. Normalizing mismatched third-party APIs into one contract is the job of the Adapter pattern, in the next file — the two patterns are close relatives.

## Choosing the strategy at runtime

Strategies are often selected from data — a request, a user setting, an environment variable:

```js
app.post("/orders/:id/ship", async (req, res, next) => {
  try {
    const order = await orderService.get(req.params.id);
    const cost = calculateShipping(order, req.body.method);   // method chosen per request
    res.json({ cost });
  } catch (err) {
    next(err);
  }
});
```

Combined with a factory (previous file), you can build the appropriate strategy from configuration:

```js
const payment = createPaymentStrategy(process.env.PAYMENT_PROVIDER);
```

## Real-world example: Passport.js

The authentication library Passport (`08-authentication-security/`) is built around this pattern. Each login mechanism — local username/password, Google OAuth, GitHub, JWT — is a "strategy" plugged into the same `passport.authenticate(...)` call:

```js
passport.use(new LocalStrategy(verifyPassword));
passport.use(new GoogleStrategy(googleConfig, verifyGoogleProfile));

app.post("/login", passport.authenticate("local"));
app.get("/auth/google/callback", passport.authenticate("google"));
```

Same entry point, different algorithms behind it.

## Strategy vs plain callbacks

If you've passed a comparison function to `Array.sort()`, you've used Strategy:

```js
users.sort((a, b) => a.name.localeCompare(b.name));   // strategy: by name
users.sort((a, b) => b.createdAt - a.createdAt);      // strategy: by date
```

Higher-order functions *are* the lightweight form of this pattern in JavaScript.

## When *not* to use it

- When there are only two options that will never grow — a plain `if` is clearer
- When the "strategies" differ in only one value — pass the value as a parameter instead
- When you'd write a class with a single method and no state — use a function

---

# Part 2 — Observer

## The problem

When something happens (an order is placed, a user signs up), several unrelated things might need to react: send an email, update analytics, notify the warehouse, invalidate a cache. If the code that creates the order calls all of them directly, it becomes tightly coupled to every concern:

```js
async function createOrder(data) {
  const order = await Order.create(data);

  await sendConfirmationEmail(order);      // email concern
  await analytics.track("order", order);   // analytics concern
  await warehouse.notify(order);           // fulfillment concern
  await cache.invalidate("orders");        // caching concern

  return order;
}
```

Every new reaction means modifying `createOrder`, and a failure in analytics might break order creation.

## The pattern: publish an event, let subscribers react

The **subject** announces "this happened"; any number of **observers** subscribe and react. The subject doesn't know who's listening.

Node ships this pattern built-in as `EventEmitter` (`02-core-modules/04-events.md`):

```js
// events/orderEvents.js
import { EventEmitter } from "node:events";
export const orderEvents = new EventEmitter();
```

```js
// services/orderService.js
import { orderEvents } from "../events/orderEvents.js";

export async function createOrder(data) {
  const order = await Order.create(data);
  orderEvents.emit("order.created", order);     // announce — don't call anyone directly
  return order;
}
```

```js
// listeners/index.js — each concern registers independently
orderEvents.on("order.created", (order) => sendConfirmationEmail(order));
orderEvents.on("order.created", (order) => analytics.track("order", order));
orderEvents.on("order.created", (order) => warehouse.notify(order));
```

`createOrder` now knows nothing about email, analytics, or warehouses. Adding a new reaction means adding a listener — `createOrder` is untouched.

## A minimal Observer from scratch

Seeing the mechanism makes `EventEmitter` less magical:

```js
class Subject {
  #observers = new Map();     // eventName -> Set of callbacks

  subscribe(event, callback) {
    if (!this.#observers.has(event)) this.#observers.set(event, new Set());
    this.#observers.get(event).add(callback);
    return () => this.#observers.get(event).delete(callback);   // unsubscribe function
  }

  notify(event, payload) {
    for (const callback of this.#observers.get(event) ?? []) {
      callback(payload);
    }
  }
}

const orders = new Subject();
const unsubscribe = orders.subscribe("created", (o) => console.log("Got", o.id));
orders.notify("created", { id: 42 });
unsubscribe();
```

## Pitfalls specific to `EventEmitter`

**1. Listeners run synchronously, in registration order.**
`emit()` doesn't return until every listener has finished its *synchronous* work. A slow synchronous listener blocks the event loop and delays the caller (`03-javascript-for-node/02-event-loop.md`).

**2. Async listeners are fire-and-forget — and their errors vanish.**
`emit()` doesn't await anything. A rejected promise inside an `async` listener becomes an **unhandled rejection** that can crash the process:

```js
// Dangerous: if sendConfirmationEmail rejects, nobody catches it
orderEvents.on("order.created", async (order) => {
  await sendConfirmationEmail(order);
});

// Safe: handle errors inside the listener
orderEvents.on("order.created", async (order) => {
  try {
    await sendConfirmationEmail(order);
  } catch (err) {
    logger.error({ err, orderId: order.id }, "Confirmation email failed");
  }
});
```

**3. An unhandled `"error"` event throws.**
Emitting `"error"` with no `"error"` listener crashes the process. Always register one on emitters that might emit it.

**4. Listener leaks.**
Registering listeners inside a function that runs repeatedly (per request) without removing them causes steady memory growth, and Node prints `MaxListenersExceededWarning` at 10+ listeners per event (`15-performance/02-profiling-and-memory-leaks.md`). Register once at startup, or remove with `off()` / `once()`.

**5. In-process only.**
An `EventEmitter` event is visible **only inside that one Node process**. With multiple instances behind a load balancer, instance B never sees events emitted in instance A. And if the process crashes, in-flight events are lost.

## When Observer isn't enough: reliable, cross-process events

For events that must not be lost, or must reach other services, the *same idea* moves to external infrastructure:

| Need | Tool |
|------|------|
| Fan-out between instances | Redis Pub/Sub (`07-databases/redis/03-pub-sub.md`) |
| Retries, persistence, dead-letter handling | Queues — BullMQ (`11-async-processing/01-queues-and-bullmq.md`) |
| High-volume event streams | Kafka (`11-async-processing/04-kafka.md`) |
| Pushing updates to browsers | WebSockets / Socket.IO (`12-realtime/`) |
| Notifying external systems | Webhooks (`09-api-development/07-webhooks.md`) |

Rule of thumb: `EventEmitter` for **in-process, best-effort decoupling**; a queue or broker when delivery **must be reliable** or **crosses process boundaries**. A critical email that must be sent belongs in a queue with retries, not in an in-memory listener.

## Observer and Strategy together

They solve different problems and combine well: an event ("payment requested") triggers a handler that picks the right strategy for the customer's chosen provider.

## When *not* to use Observer

- When the caller needs the result or must know if the reaction succeeded — call the function directly (observers are fire-and-forget)
- When order or atomicity matters — a transaction (`07-databases/mongodb/03-transactions.md`) can't span loosely coupled listeners
- When there's exactly one reaction and it will always be one — a direct call is easier to read and debug (event-driven flow is harder to trace: "who handles this?" requires searching for listeners)

## Common mistakes

**Strategy**
- **Growing `if/else` chains instead of a lookup object** — the exact thing Strategy replaces.
- **Strategies with inconsistent return shapes** — the whole point is a shared contract.
- **Making a class for what could be a function** — in JS, functions are first-class.
- **Not validating the strategy key** — `strategies[userInput]` on an object can hit inherited properties like `constructor`; use a `Map` or `Object.hasOwn()` for untrusted input.

**Observer**
- **Async listeners with no `try/catch`** — rejections become unhandled and can kill the process.
- **Expecting `emit()` to wait for async listeners** — it doesn't.
- **Registering listeners per request** — memory leak.
- **No `"error"` listener** — an emitted `"error"` crashes the process.
- **Using `EventEmitter` for critical, must-deliver work** — events vanish on crash and don't cross instances.
- **Event spaghetti** — so many events that nobody can follow the flow. Keep event names documented and few.

## Quick summary

- **Strategy** — interchangeable algorithms behind one contract; in JS, usually a lookup of functions
- Replaces growing `if/else` chains; each strategy is independently testable
- Passport.js and `Array.sort` comparators are strategies you already use
- **Observer** — a subject announces events; any number of subscribers react, decoupled from the source
- Node's `EventEmitter` *is* the pattern, but it's synchronous, in-process, and fire-and-forget
- Handle errors inside async listeners; avoid listener leaks
- Use queues/brokers when delivery must be reliable or cross processes

## Next

**`03-adapter-and-decorator.md`** covers structural patterns: making incompatible interfaces fit together (Adapter) and layering extra behavior onto existing objects (Decorator).
