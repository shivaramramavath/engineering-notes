# Adapter

An **adapter** converts one interface into another that your code expects. You use it at the seams where you do not control the shape of something: a third-party SDK, a legacy module, an awkward API, a callback-style library. The adapter keeps that foreign shape contained in one place, so the rest of your code talks to an interface you designed.

**Prerequisites:**
- [Interfaces](../04-objects-and-interfaces/00-interfaces.md)
- [Classes](../05-classes/00-classes.md) (`implements`)
- [The DTO pattern](../16-type-safe-apis/02-dto-pattern.md) (mapping at boundaries)

---

## The problem

Your application needs to take payments. A vendor provides an SDK, and you write code directly against it:

```ts
// scattered through the codebase
const charge = await acme.charges.create({
  amount_cents: order.total * 100,
  currency_code: "USD",
  source_token: token,
  meta: { order: order.id },
});
if (charge.state === "SETTLED") { /* ... */ }
```

Now vendor names, units, and status strings are all over your code. Switching vendors, upgrading the SDK, or testing without a network each means touching every call site.

## The pattern: your interface, their implementation

Define the interface **your application wants**, in your domain's terms. Then write an adapter that implements it using the vendor SDK.

```ts
// What the application needs (your language)
interface PaymentGateway {
  charge(request: ChargeRequest): Promise<ChargeResult>;
}

interface ChargeRequest {
  orderId: string;
  amountCents: number;
  currency: "USD" | "EUR";
  paymentToken: string;
}

type ChargeResult =
  | { status: "succeeded"; transactionId: string }
  | { status: "declined"; reason: string };
```

```ts
// The adapter: translates between your interface and the vendor's
class AcmePayGateway implements PaymentGateway {
  constructor(private readonly client: AcmePayClient) {}

  async charge(req: ChargeRequest): Promise<ChargeResult> {
    const result = await this.client.charges.create({
      amount_cents: req.amountCents,
      currency_code: req.currency,
      source_token: req.paymentToken,
      meta: { order: req.orderId },
    });

    switch (result.state) {
      case "SETTLED":
        return { status: "succeeded", transactionId: result.id };
      case "REJECTED":
        return { status: "declined", reason: result.reject_reason ?? "unknown" };
      default:
        throw new Error(`Unexpected AcmePay state: ${result.state}`);
    }
  }
}
```

Everything vendor-specific lives in this one class:

- **Names:** `amount_cents` / `source_token` versus `amountCents` / `paymentToken`.
- **Units and formats:** whatever the vendor expects, converted in one place.
- **Status vocabulary:** `"SETTLED"` and `"REJECTED"` become your `"succeeded"` and `"declined"`.
- **Errors:** vendor exceptions can be translated into your own error types here.

The rest of the application depends only on `PaymentGateway`:

```ts
class CheckoutService {
  constructor(private readonly payments: PaymentGateway) {}

  async pay(order: Order, token: string) {
    return this.payments.charge({
      orderId: order.id,
      amountCents: order.totalCents,
      currency: order.currency,
      paymentToken: token,
    });
  }
}
```

Switching vendors means writing another class that implements `PaymentGateway`. Nothing else changes. Testing means writing a tiny fake:

```ts
class FakeGateway implements PaymentGateway {
  async charge(): Promise<ChargeResult> {
    return { status: "succeeded", transactionId: "test-1" };
  }
}
```

Combine with [dependency injection](./05-dependency-injection.md) to supply the real adapter in production and the fake in tests.

## Function adapters

Adapters do not have to be classes. Often a function is enough:

```ts
// Adapt an API record to your domain model
function fromAcmeCustomer(c: AcmeCustomerRecord): Customer {
  return {
    id: c.cust_id,
    name: `${c.first} ${c.last}`,
    email: c.email_address.toLowerCase(),
    createdAt: new Date(c.created_ts * 1000),
  };
}
```

This is the same idea as a DTO mapper ([DTO pattern](../16-type-safe-apis/02-dto-pattern.md)), applied to inbound data from an external system. A group of such functions around one vendor is an **anti-corruption layer**: a boundary that stops another system's model from leaking into yours.

## Adapting callbacks to promises

A common, practical adapter: wrap a callback API in an interface that returns a promise.

```ts
function readFileP(path: string): Promise<string> {
  return new Promise((resolve, reject) => {
    legacyReadFile(path, "utf8", (err, data) => {
      if (err) reject(err);
      else resolve(data);
    });
  });
}
```

Node's `util.promisify` does this mechanically for standard callback signatures ([promises](../12-async-and-iteration/01-promises.md)).

## Adapting events to async iteration

An event-based source can be adapted to an `AsyncIterable`, so consumers use `for await`:

```ts
import { on } from "node:events";

async function* messages(socket: EventEmitter): AsyncGenerator<string> {
  for await (const [data] of on(socket, "message")) {
    yield String(data);
  }
}
```

Here the adapter changes the *consumption style*, not the data. See [iterators and generators](../12-async-and-iteration/04-iterators-and-generators.md).

## Adapting types you cannot change

For untyped or loosely typed libraries, the adapter is also where you restore type safety ([third-party types](../09-declaration-files/04-third-party-types.md)):

```ts
// The library returns `any`; the adapter validates and returns your type
class WeatherAdapter implements WeatherService {
  async current(city: string): Promise<Weather> {
    const raw: unknown = await legacyWeatherLib.fetch(city);
    const parsed = WeatherSchema.parse(raw);        // runtime validation at the boundary
    return { tempC: parsed.temp, summary: parsed.desc };
  }
}
```

Contain `any` and assertions in the adapter, and expose only validated, domain-typed values.

## Class adapter vs object adapter

- **Object adapter (composition):** the adapter *holds* the foreign object and delegates (`AcmePayGateway` above). This is the usual choice in TypeScript.
- **Class adapter (inheritance):** the adapter *extends* the foreign class. It is tighter coupling, exposes the foreign class's members, and breaks if the vendor changes its class. Avoid it unless you truly want to extend.

## Designing the target interface

The adapter is only as good as the interface it targets:

- **Design for your use cases,** not as a mirror of the vendor's API. A "wrapper" that merely renames methods does not isolate you from vendor concepts.
- **Keep it small.** Include the operations you need today.
- **Do not leak vendor types.** If `ChargeResult` exposes an `AcmeError` or an `AcmeCharge`, the vendor has escaped.
- **Define error semantics** in your own terms: which failures are returned as values (declined card) and which are thrown (network down) ([error handling strategies](../11-error-handling/03-error-handling-strategies.md)).
- **Choose the right level.** Too thin, and you adapt nothing. Too thick, and the adapter contains business logic.

## Adapter vs related patterns

| Pattern | Purpose |
|---|---|
| **Adapter** | make an existing, incompatible interface fit the one you want |
| **Facade** | simplify a complex subsystem behind one easier interface (you design both sides) |
| **Decorator** | add behavior while keeping the **same** interface |
| **Proxy** | control access to an object, with the same interface |
| **[Repository](./04-repository.md)** | adapt a data store to a collection-like domain interface |

A repository is, in effect, an adapter for persistence. Many patterns in this section amount to "put an interface of your own between your code and something you do not control".

## When to use an adapter

Good fits:

- A third-party SDK that you want to be able to replace, upgrade, or fake.
- A legacy module with an unsuitable interface.
- External data formats that differ from your domain model.
- Different providers of the same capability (payments, email, storage, logging) behind one interface.

Skip it for a small, stable, widely used library that you have no plan to replace (a date formatter, a UUID generator), where the wrapper would add nothing but indirection.

## Common mistakes

- Defining the target interface as a copy of the vendor's API, so nothing is actually isolated.
- Leaking vendor types or error classes through the interface.
- Putting business rules in the adapter instead of the domain layer.
- Creating an adapter per method call instead of one cohesive adapter.
- Using class inheritance to adapt, then breaking on vendor updates.
- Letting `any` from an untyped library flow past the adapter.
- Translating only the happy path, so vendor-specific errors escape in production.

## Debugging

- If vendor concepts appear outside the adapter, search for imports of the SDK outside it. There should be one place.
- When behavior differs between the real adapter and a fake, add a **contract test** that runs the same test suite against both implementations.
- Log the raw vendor request and response at the adapter boundary (without secrets) to diagnose mapping problems.
- If conversions are wrong (units, time zones, currency), test the mapping functions with edge cases directly.

## Quick summary

- An adapter makes an incompatible interface fit the one your code expects, isolating a vendor, a legacy module, or an external format.
- Define the target interface in **your** terms, then implement it with composition around the foreign object. Do not leak vendor types.
- Adapters can be classes, mapper functions, or small wrappers that turn callbacks or events into promises and async iterables.
- Use the adapter as the place to validate data and contain `any`.
- Pair it with dependency injection and fakes to swap and test implementations. Skip it where the wrapper would add nothing.

**Next:** [Repository](./04-repository.md)
