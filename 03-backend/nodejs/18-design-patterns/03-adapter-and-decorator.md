# Adapter and Decorator

Both are **structural patterns** — they're about wrapping one thing in another. An **Adapter** wraps something to change its *interface*, so it fits where it's needed. A **Decorator** wraps something to add *behavior* while keeping the *same* interface.

The quick way to tell them apart:

| | Interface changes? | Purpose |
|---|---|---|
| **Adapter** | Yes — translates A's interface into B's | Make incompatible things work together |
| **Decorator** | No — same interface in and out | Add behavior (logging, caching, retries) |

---

# Part 1 — Adapter

## The problem

Your code expects one interface, but the thing you need to use offers another. This happens constantly with third-party services: every payment, email, or storage provider has its own method names, argument shapes, and response formats.

```js
// Stripe's style
const charge = await stripe.charges.create({ amount: 5000, currency: "usd", source: token });
// → { id: "ch_123", status: "succeeded", amount: 5000 }

// PayPal's style
const payment = await paypal.payments.execute(token, { total: "50.00", currency: "USD" });
// → { transactionId: "PAY-456", state: "approved" }
```

If your business logic calls these directly, it's welded to each vendor's quirks. Changing provider — or supporting two — means rewriting business logic.

## The pattern: translate at the boundary

Define the interface **your application wants**, then write a small adapter for each vendor that translates to it:

```js
// What our app expects of any payment provider:
//   charge(amountCents, currency, token) -> { id, status: "paid" | "failed" }

// payments/StripeAdapter.js
export class StripeAdapter {
  constructor(stripeClient) { this.stripe = stripeClient; }

  async charge(amountCents, currency, token) {
    const res = await this.stripe.charges.create({
      amount: amountCents,
      currency,
      source: token,
    });
    return {
      id: res.id,
      status: res.status === "succeeded" ? "paid" : "failed",
    };
  }
}

// payments/PayPalAdapter.js
export class PayPalAdapter {
  constructor(paypalClient) { this.paypal = paypalClient; }

  async charge(amountCents, currency, token) {
    const res = await this.paypal.payments.execute(token, {
      total: (amountCents / 100).toFixed(2),
      currency: currency.toUpperCase(),
    });
    return {
      id: res.transactionId,
      status: res.state === "approved" ? "paid" : "failed",
    };
  }
}
```

```js
// Business logic knows only our interface
class CheckoutService {
  constructor(paymentProvider) { this.payments = paymentProvider; }

  async checkout(order, token) {
    const result = await this.payments.charge(order.totalCents, order.currency, token);
    if (result.status !== "paid") throw new PaymentFailedError(result.id);
    return result;
  }
}
```

Notice what each adapter handled: **method names**, **units** (cents vs a decimal string), **casing**, and **status vocabulary** (`succeeded`/`approved` → `paid`). All the vendor-specific mess lives in two small files; everything else is clean.

The same shape appears in Strategy (previous file): the adapters *are* interchangeable strategies, and a factory can pick which to build. The difference in intent: **Strategy** is about choosing between algorithms; **Adapter** is about making an existing, incompatible API fit your contract.

## Other places adapters earn their keep

- **Storage** — `LocalDiskAdapter`, `S3Adapter`, `GCSAdapter`, each exposing `save(file)`, `get(key)`, `delete(key)` (`06-express/07-file-upload.md`, `19-system-design/05-file-upload-system.md`)
- **Email** — SES, SendGrid, SMTP behind one `send({ to, subject, body })`
- **Databases** — a repository hides whether data comes from MongoDB or PostgreSQL (`10-architecture/03-repository-and-service-pattern.md`)
- **Legacy APIs** — wrapping an old callback-style function in a promise interface:

```js
import { promisify } from "node:util";
import legacyLib from "legacy-lib";

// Adapts callback style -> promise style
export const fetchRecord = promisify(legacyLib.fetchRecord);
```

(`util.promisify` *is* an adapter: same capability, different calling convention.)

## Adapting data shapes, not just services

Anti-corruption at the boundary also applies to **data**. Map external responses into your own domain model immediately, rather than letting third-party field names leak through the codebase:

```js
function toUser(githubProfile) {
  return {
    id: String(githubProfile.id),
    name: githubProfile.name ?? githubProfile.login,
    email: githubProfile.email,
    avatarUrl: githubProfile.avatar_url,   // snake_case -> camelCase
  };
}
```

This is the same idea behind OAuth profile mapping in `08-authentication-security/04-oauth-and-oidc.md`.

## Testing benefit

Because the app depends on *your* interface, a test double is trivial:

```js
const fakePayments = {
  charge: async () => ({ id: "test-1", status: "paid" }),
};
const checkout = new CheckoutService(fakePayments);   // no network, no Stripe
```

## When *not* to use it

- When you're certain you'll only ever use one vendor *and* the integration is tiny — a thin wrapper is still often worth it, but a full adapter hierarchy is not
- When the "adapter" does nothing but forward calls unchanged — that's indirection with no benefit
- Don't build an abstraction around a provider you haven't used yet; design the interface from real needs, not from guessing what other vendors look like

---

# Part 2 — Decorator

## The problem

You want to add a cross-cutting behavior — logging, caching, retries, timing, authorization checks — to existing functionality **without modifying it** and without scattering that behavior through every function.

```js
// Logging, timing and retry code tangled into the business function
async function getUser(id) {
  const start = Date.now();
  logger.info("getUser called", { id });
  let attempts = 0;
  while (attempts < 3) {
    try {
      const user = await db.findUser(id);
      logger.info("getUser took", Date.now() - start);
      return user;
    } catch (err) {
      attempts++;
    }
  }
}
```

The actual job — fetch a user — is buried.

## Function decorators (higher-order functions)

In JavaScript, the lightest decorator is a function that takes a function and returns an enhanced function with the **same signature**:

```js
export function withTiming(fn, label) {
  return async (...args) => {
    const start = performance.now();
    try {
      return await fn(...args);
    } finally {
      logger.info({ label, ms: performance.now() - start });
    }
  };
}

export function withRetry(fn, { attempts = 3, delayMs = 200 } = {}) {
  return async (...args) => {
    let lastError;
    for (let i = 1; i <= attempts; i++) {
      try {
        return await fn(...args);
      } catch (err) {
        lastError = err;
        if (i < attempts) await new Promise((r) => setTimeout(r, delayMs * i));
      }
    }
    throw lastError;
  };
}

export function withCache(fn, ttlMs = 60_000) {
  const cache = new Map();
  return async (key) => {
    const hit = cache.get(key);
    if (hit && hit.expires > Date.now()) return hit.value;
    const value = await fn(key);
    cache.set(key, { value, expires: Date.now() + ttlMs });
    return value;
  };
}
```

Stack them like layers — the original function is untouched:

```js
const rawGetUser = (id) => db.findUser(id);

const getUser = withTiming(
  withCache(
    withRetry(rawGetUser, { attempts: 3 }),
    30_000
  ),
  "getUser"
);

const user = await getUser("42");   // same call as before, now timed, cached, and retried
```

Order matters: here the cache sits *outside* the retry (so cache hits skip the retry entirely), and timing wraps everything.

## Object decorators (wrapping a whole interface)

For objects with multiple methods, the decorator implements the **same interface** and delegates to the wrapped object:

```js
class UserRepository {
  async findById(id) { return db.users.findOne({ id }); }
  async create(data) { return db.users.insert(data); }
}

class CachedUserRepository {
  constructor(inner, cache) {
    this.inner = inner;       // the real repository
    this.cache = cache;
  }

  async findById(id) {
    const cached = await this.cache.get(`user:${id}`);
    if (cached) return JSON.parse(cached);

    const user = await this.inner.findById(id);
    if (user) await this.cache.set(`user:${id}`, JSON.stringify(user), { EX: 60 });
    return user;
  }

  async create(data) {
    const user = await this.inner.create(data);
    await this.cache.del(`user:${user.id}`); // keep the cache consistent on writes
    return user;
  }
}

const userRepo = new CachedUserRepository(new UserRepository(), redis);
```

The services using `userRepo` never learn that a cache is involved — a strong fit with the repository pattern (`10-architecture/03-repository-and-service-pattern.md`) and with Redis caching (`07-databases/redis/02-caching-and-sessions.md`). To remove caching you change the wiring, not the callers.

## You already use decorators: Express middleware

Middleware is a decorator-like chain: each layer wraps the next, adds behavior, and passes control along (`06-express/02-middleware.md`):

```js
app.use(requestLogger);        // logs, then calls next()
app.use(helmet());             // adds security headers, then next()
app.use(rateLimiter);          // may reject, otherwise next()
app.use("/api", authenticate); // verifies identity, then next()
```

A wrapper that adds behavior around a handler is the same idea applied to a single route:

```js
export const asyncHandler = (fn) => (req, res, next) =>
  Promise.resolve(fn(req, res, next)).catch(next);

router.get("/:id", asyncHandler(getUser));   // getUser decorated with error forwarding
```

(Strictly, a middleware chain is closer to *chain of responsibility* — each link can short-circuit — but the "layered wrapping" intuition is the same.)

## Decorator syntax in TypeScript

TypeScript (and recent JavaScript proposals) have a dedicated `@decorator` syntax for classes and methods, used heavily by frameworks like NestJS:

```ts
@Controller("users")
class UserController {
  @Get(":id")
  @UseGuards(AuthGuard)
  findOne(@Param("id") id: string) { /* ... */ }
}
```

That's the same concept expressed as annotations, with the framework applying the wrapping for you. You don't need that syntax to use the pattern — plain higher-order functions, as above, work everywhere. (Syntax and tooling support for decorators has changed over time, so check your TypeScript version and `tsconfig` before relying on it — see `17-typescript/05-production-config.md`.)

## Adapter vs Decorator vs Strategy — keeping them straight

| Pattern | Question it answers | Interface |
|---------|--------------------|-----------|
| **Strategy** | "Which algorithm should do this?" | Same contract, different implementations to choose between |
| **Adapter** | "How do I make *this* look like *that*?" | Changes the interface |
| **Decorator** | "How do I add behavior around this?" | Preserves the interface |

They compose naturally: an **adapter** normalizes Stripe to your payment interface, a **decorator** adds retries and logging around it, and a **factory** picks which adapter to build based on config.

## When *not* to use Decorator

- When behavior is needed in exactly one place — just write it there
- When too many layers make behavior hard to trace ("why was this called twice?") — each wrapper is another stack frame and another place for bugs. Keep chains short and documented.
- When ordering is subtle and easy to get wrong (retry outside cache vs inside) — name and document the composition in one place

## Common mistakes

**Adapter**
- **Leaking vendor types past the adapter** — the point is that nothing outside it knows the vendor's field names.
- **Designing the interface around one vendor** — you'll end up with Stripe's shape forever. Define what *your app* needs.
- **Forgetting unit/format differences** — cents vs decimals, timezones, status vocabularies. These are the classic adapter bugs.
- **A pass-through adapter that translates nothing** — pure indirection.

**Decorator**
- **Changing the interface** — then it's an adapter, and callers can't treat it as a drop-in.
- **Wrong layer order** — caching outside vs inside retries (or auth outside vs inside logging) behaves very differently.
- **Retrying non-idempotent operations** — retrying a "charge card" call can double-charge. Retries only belong on safe or idempotent operations (`09-api-development/06-idempotency.md`).
- **In-memory cache decorators in multi-instance deployments** — each process has its own cache and they'll disagree; use Redis for shared caches. Also set a TTL and a size bound, or the `Map` grows forever.
- **Forgetting to invalidate on writes** — a cached read that outlives an update serves stale data.
- **Swallowing errors in a wrapper** — decorators should rethrow, not hide failures.

## Quick summary

- **Adapter** — translate one interface into the one your application expects; vendor quirks stay at the boundary
- Use adapters for payments, email, storage, and external APIs; map external data into your own model immediately
- **Decorator** — wrap something to add behavior while keeping the *same* interface
- In JS, higher-order functions (`withRetry`, `withCache`, `withTiming`) are the lightweight form
- Express middleware and `asyncHandler` are decorator-style layering you already use
- Mind the order of layers; only retry idempotent operations; remember in-memory caches are per-process
- Adapter changes the interface, Decorator preserves it, Strategy swaps implementations — and all three combine well

## Next

**`19-system-design/00-README.md`** moves on to system design — combining the building blocks from this whole repo into designs for scalable APIs, URL shorteners, chat systems, and more.
