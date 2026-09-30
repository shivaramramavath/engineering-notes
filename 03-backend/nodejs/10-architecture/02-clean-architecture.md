# Clean Architecture

Structuring code so that your business rules depend on nothing: not Express, not PostgreSQL, not SendGrid. Frameworks and databases become replaceable details at the edge.

## The problem it solves

In the layered architecture from `01-mvc-and-layered-architecture.md`, dependencies flow **downward** — all the way to the database:

```
Controller → Service → Repository → Database driver
                                        ▲
        business logic depends (transitively) on this
```

Your most valuable code (the business rules) ends up depending on your most volatile code (libraries, drivers, ORMs, third-party APIs). Consequences:

- Changing ORMs, databases, or email providers ripples upward into business logic.
- Business rules can't be tested without faking infrastructure.
- Logic gets shaped by the framework's conventions (Mongoose documents leaking into rules, `req.user` passed everywhere).

**Clean architecture** (Robert C. Martin's name for a family of ideas that includes Hexagonal/"Ports & Adapters" and Onion architecture) **inverts the dependency direction**: infrastructure depends on the business rules, never the reverse.

---

## The Dependency Rule

The single idea everything else follows from:

> **Source code dependencies point only inward, toward the business rules.**

```
┌─────────────────────────────────────────────────────────────┐
│  Frameworks & Drivers     Express, pg, Mongoose, BullMQ,    │
│  (outermost, most volatile)   SendGrid, Stripe SDK          │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  Interface Adapters      controllers, repositories  │    │
│  │                          (implementations), DTOs,   │    │
│  │                          presenters                 │    │
│  │  ┌─────────────────────────────────────────────┐    │    │
│  │  │  Use Cases / Application                    │    │    │
│  │  │  PlaceOrder, CancelOrder, RegisterUser      │    │    │
│  │  │  ┌─────────────────────────────────────┐    │    │    │
│  │  │  │  Entities / Domain                  │    │    │    │
│  │  │  │  Order, Money, User: core rules     │    │    │    │
│  │  │  └─────────────────────────────────────┘    │    │    │
│  │  └─────────────────────────────────────────────┘    │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘

          Arrows of `import` point from outside → inside.
    Inner circles know NOTHING about outer circles.
```

| Circle | Contains | Knows about |
|---|---|---|
| **Entities (domain)** | Core business objects and rules that would exist even without software: "an order total can't be negative" | Nothing |
| **Use cases (application)** | One class/function per application action; orchestrates entities | Entities, and **interfaces** (ports) it needs |
| **Interface adapters** | Controllers, repository implementations, mappers | Use cases, entities |
| **Frameworks & drivers** | Express, database drivers, SDKs, config | Everything inward |

The inner circles contain **no `import` of Express, `pg`, Mongoose, or any SDK**. If you can't run your use cases in a plain Node script with no `npm install`, the rule is broken.

---

## Ports and adapters

How does an inner circle use a database without depending on it? It declares **what it needs** as an interface (a *port*), and the outer layer provides the implementation (an *adapter*).

```
   Use case (inner)                       Infrastructure (outer)
  ┌────────────────┐    needs a          ┌────────────────────────┐
  │   PlaceOrder   │ ─────────────────▶  │ PostgresOrderRepository│
  │                │   OrderRepository   │   (adapter: implements │
  │ depends on the │   PORT (interface)  │    the port with SQL)  │
  │ port, not on pg│ ◀ ─ ─ ─ ─ ─ ─ ─ ─ ─ │                        │
  └────────────────┘   implemented by    └────────────────────────┘
                     (dependency inversion)
```

JavaScript has no `interface` keyword, so a port is a **documented contract**: "an order repository has these methods with these shapes." In TypeScript it's a literal `interface` (`17-typescript/02-interfaces-and-generics.md`). The mechanism that delivers the adapter into the use case is **dependency injection**, covered in `04-dependency-injection.md`.

### Driving vs driven ports

| | Driving (primary) | Driven (secondary) |
|---|---|---|
| Direction | Something **calls into** your app | Your app **calls out** to something |
| Examples | HTTP request, CLI command, queue message, cron job | Database, email, payment gateway, clock, file storage |
| Port | The use case's own function signature | An interface the use case declares (`OrderRepository`, `Mailer`) |
| Adapter | Express controller, BullMQ worker | `PostgresOrderRepository`, `SendGridMailer` |

The same use case can be driven by an HTTP controller, a background worker, and a test, unchanged.

---

## Folder structure

```
src/
├── modules/
│   └── orders/
│       ├── domain/                       ← innermost: pure JavaScript, zero imports from outside
│       │   ├── Order.js                  entity + rules
│       │   ├── Money.js                  value object
│       │   └── errors.js                 domain errors (OutOfStockError, ...)
│       │
│       ├── application/                  ← use cases + ports
│       │   ├── placeOrder.js
│       │   ├── cancelOrder.js
│       │   └── ports.md                  (or ports.d.ts in TypeScript): contracts
│       │
│       ├── infrastructure/               ← adapters: the only place that imports pg, SDKs
│       │   ├── PostgresOrderRepository.js
│       │   ├── PostgresProductRepository.js
│       │   └── SendGridMailer.js
│       │
│       └── http/                         ← driving adapter: Express-specific
│           ├── orders.routes.js
│           ├── orders.controller.js
│           └── orders.schemas.js
│
├── shared/
├── composition-root.js                   ← wires adapters into use cases (see 04-dependency-injection.md)
├── app.js
└── server.js
```

The folder names are a convention; what matters is that **imports only go inward**. Some teams enforce it with a linter rule (`eslint-plugin-import` restrictions, `dependency-cruiser`, or `eslint-plugin-boundaries`) so a stray `import pg` in `domain/` fails CI.

---

## The running example, rebuilt

### Domain: entities and value objects

Pure logic, pure JavaScript, no I/O, trivially testable.

```js
// domain/Money.js: a value object: immutable, compared by value
export class Money {
  #cents;
  constructor(cents) {
    if (!Number.isInteger(cents)) throw new TypeError("Money must be integer cents");
    this.#cents = cents;
  }
  get cents() { return this.#cents; }
  add(other) { return new Money(this.#cents + other.cents); }
  times(n) { return new Money(this.#cents * n); }
  discount(rate) { return new Money(Math.round(this.#cents * (1 - rate))); }
}
```

```js
// domain/errors.js
export class DomainError extends Error {
  constructor(code, message) { super(message); this.name = "DomainError"; this.code = code; }
}
export class OutOfStockError extends DomainError {
  constructor(productName) { super("out_of_stock", `Not enough stock for ${productName}`); }
}
export class EmptyOrderError extends DomainError {
  constructor() { super("empty_order", "An order must contain at least one item"); }
}
```

```js
// domain/Order.js: the entity owns its rules
import { Money } from "./Money.js";
import { DomainError, EmptyOrderError, OutOfStockError } from "./errors.js";

const PREMIUM_DISCOUNT = 0.1;

export class Order {
  constructor({ id, userId, lines, total, status = "pending" }) {
    this.id = id;
    this.userId = userId;
    this.lines = lines;
    this.total = total;
    this.status = status;
  }

  // Factory enforcing the rules for creating a NEW order
  static place({ id, user, items, products }) {
    if (items.length === 0) throw new EmptyOrderError();

    let subtotal = new Money(0);
    const lines = items.map((item) => {
      const product = products.get(item.productId);
      if (!product) throw new DomainError("product_not_found", `Product ${item.productId} not found`);
      if (product.stock < item.quantity) throw new OutOfStockError(product.name);

      const lineTotal = new Money(product.priceCents).times(item.quantity);
      subtotal = subtotal.add(lineTotal);
      return { productId: product.id, quantity: item.quantity, unitPriceCents: product.priceCents };
    });

    const total = user.isPremium ? subtotal.discount(PREMIUM_DISCOUNT) : subtotal;
    return new Order({ id, userId: user.id, lines, total });
  }

  cancel() {
    if (this.status === "shipped") {
      throw new DomainError("cannot_cancel", "A shipped order cannot be cancelled");
    }
    this.status = "cancelled";
  }
}
```

The pricing rule now lives *inside the entity*, not scattered across services, and can be tested with zero setup.

### Application: a use case

One use case = one user-facing action. It **orchestrates**: loads things via ports, asks entities to enforce rules, saves, triggers side effects.

```js
// application/placeOrder.js
import { randomUUID } from "node:crypto";
import { Order } from "../domain/Order.js";
import { DomainError } from "../domain/errors.js";

/**
 * Ports this use case needs (contracts; implemented in infrastructure/):
 *   userRepo.findById(id)                    → { id, email, isPremium } | null
 *   productRepo.findByIds(ids)               → Map<productId, { id, name, priceCents, stock }>
 *   orderRepo.save(order)                    → void   (must atomically persist order + reduce stock)
 *   mailer.sendOrderConfirmation(email, order) → Promise<void>
 */
export function makePlaceOrder({ userRepo, productRepo, orderRepo, mailer, logger }) {
  return async function placeOrder({ userId, items }) {
    const user = await userRepo.findById(userId);
    if (!user) throw new DomainError("user_not_found", "User not found");

    const products = await productRepo.findByIds(items.map((i) => i.productId));

    const order = Order.place({ id: randomUUID(), user, items, products });   // rules live in the entity

    await orderRepo.save(order);

    mailer.sendOrderConfirmation(user.email, order).catch((err) =>
      logger.error({ err, orderId: order.id }, "Confirmation email failed")
    );

    return order;
  };
}
```

This file imports **nothing** from Express, `pg`, or SendGrid. Everything it touches arrives as an argument.

### Infrastructure: adapters

```js
// infrastructure/PostgresOrderRepository.js
import { Order } from "../domain/Order.js";
import { Money } from "../domain/Money.js";
import { DomainError } from "../domain/errors.js";

export function makePostgresOrderRepository({ pool }) {
  return {
    async save(order) {
      const client = await pool.connect();
      try {
        await client.query("BEGIN");
        await client.query(
          "INSERT INTO orders (id, user_id, total_cents, status) VALUES ($1, $2, $3, $4)",
          [order.id, order.userId, order.total.cents, order.status]
        );
        for (const line of order.lines) {
          const r = await client.query(
            "UPDATE products SET stock = stock - $1 WHERE id = $2 AND stock >= $1",
            [line.quantity, line.productId]
          );
          if (r.rowCount === 0) throw new DomainError("out_of_stock", "Stock changed during checkout");
          await client.query(
            "INSERT INTO order_items (order_id, product_id, quantity, unit_price_cents) VALUES ($1, $2, $3, $4)",
            [order.id, line.productId, line.quantity, line.unitPriceCents]
          );
        }
        await client.query("COMMIT");
      } catch (err) {
        await client.query("ROLLBACK");
        throw err;
      } finally {
        client.release();
      }
    },

    async findByIdAndUser(id, userId) {
      const { rows } = await pool.query("SELECT * FROM orders WHERE id = $1 AND user_id = $2", [id, userId]);
      if (!rows[0]) return null;
      return new Order({ id: rows[0].id, userId: rows[0].user_id, total: new Money(rows[0].total_cents), lines: [], status: rows[0].status });
    },
  };
}
```

The repository **maps between database rows and domain objects**. The entity never sees a row; SQL never leaks inward. To swap PostgreSQL for MongoDB you write another adapter with the same methods; no use case changes.

### The driving adapter: Express controller

```js
// http/orders.controller.js
export function makeOrdersController({ placeOrder }) {
  return {
    async create(req, res) {
      const order = await placeOrder({ userId: req.user.id, items: req.body.items });

      res.status(201).json({
        data: {
          id: order.id,
          status: order.status,
          total: { amount: order.total.cents, currency: "USD" },
        },
      });
    },
  };
}
```

### Mapping domain errors to HTTP

The use case throws `DomainError`; only the outermost layer knows about status codes:

```js
// shared/middleware/errorHandler.js (excerpt)
import { DomainError } from "../../modules/orders/domain/errors.js";

const STATUS_BY_CODE = {
  user_not_found: 404,
  product_not_found: 404,
  out_of_stock: 409,
  empty_order: 422,
  cannot_cancel: 409,
};

if (err instanceof DomainError) {
  return res.status(STATUS_BY_CODE[err.code] ?? 400).json({
    error: { code: err.code, message: err.message, requestId: req.id },
  });
}
```

The domain says *what* went wrong; HTTP decides *how to express it*. Another adapter (a CLI, a queue worker) could translate the same errors differently.

### Composition root: where everything is wired

The one place allowed to know about every concrete piece:

```js
// composition-root.js
import { pool } from "./config/db.js";
import { logger } from "./config/logger.js";
import { makePostgresOrderRepository } from "./modules/orders/infrastructure/PostgresOrderRepository.js";
import { makePostgresUserRepository } from "./modules/users/infrastructure/PostgresUserRepository.js";
import { makePostgresProductRepository } from "./modules/orders/infrastructure/PostgresProductRepository.js";
import { makeSendGridMailer } from "./modules/orders/infrastructure/SendGridMailer.js";
import { makePlaceOrder } from "./modules/orders/application/placeOrder.js";
import { makeOrdersController } from "./modules/orders/http/orders.controller.js";

const orderRepo = makePostgresOrderRepository({ pool });
const userRepo = makePostgresUserRepository({ pool });
const productRepo = makePostgresProductRepository({ pool });
const mailer = makeSendGridMailer({ apiKey: process.env.SENDGRID_API_KEY });

const placeOrder = makePlaceOrder({ userRepo, productRepo, orderRepo, mailer, logger });

export const ordersController = makeOrdersController({ placeOrder });
```

This is **dependency injection without a framework**: plain functions receiving their collaborators. More in `04-dependency-injection.md`.

---

## Testing: where the payoff is

```js
// Entity test: no mocks, no database, milliseconds
test("premium customers get 10% off", () => {
  const order = Order.place({
    id: "o1",
    user: { id: "u1", isPremium: true },
    items: [{ productId: "p1", quantity: 2 }],
    products: new Map([["p1", { id: "p1", name: "Pen", priceCents: 1000, stock: 10 }]]),
  });
  expect(order.total.cents).toBe(1800);
});

test("rejects orders that exceed stock", () => {
  expect(() =>
    Order.place({
      id: "o1",
      user: { id: "u1", isPremium: false },
      items: [{ productId: "p1", quantity: 99 }],
      products: new Map([["p1", { id: "p1", name: "Pen", priceCents: 1000, stock: 3 }]]),
    })
  ).toThrow(OutOfStockError);
});
```

```js
// Use case test: in-memory fakes for ports, no DB, no Express, no network
test("saves the order and emails the customer", async () => {
  const saved = [];
  const sent = [];

  const placeOrder = makePlaceOrder({
    userRepo: { findById: async () => ({ id: "u1", email: "a@b.com", isPremium: false }) },
    productRepo: { findByIds: async () => new Map([["p1", { id: "p1", name: "Pen", priceCents: 500, stock: 5 }]]) },
    orderRepo: { save: async (o) => saved.push(o) },
    mailer: { sendOrderConfirmation: async (to) => sent.push(to) },
    logger: { error() {} },
  });

  await placeOrder({ userId: "u1", items: [{ productId: "p1", quantity: 1 }] });

  expect(saved).toHaveLength(1);
  expect(sent).toEqual(["a@b.com"]);
});
```

| Layer | Test style | Speed |
|---|---|---|
| Domain entities | Plain unit tests | Instant |
| Use cases | Unit tests with in-memory fakes | Instant |
| Adapters (repositories) | Integration tests against a real test DB | Slower |
| HTTP | API tests with `supertest` | Slower |

The vast majority of your business logic ends up in the fast tiers. See `13-testing/`.

---

## Clean vs layered: an honest comparison

| | Layered (`01`) | Clean / Hexagonal (`02`) |
|---|---|---|
| Dependency direction | Top → bottom (toward the DB) | Always inward (toward the domain) |
| Business logic location | Services | Entities (rules) + use cases (orchestration) |
| Database swap | Ripples into services | Replace one adapter |
| Testing business logic | Mock repositories | Plain objects + simple fakes |
| Ceremony | Low–medium | Medium–high |
| Files per feature | ~5 | ~10+ |
| Best for | Most CRUD-heavy APIs | Complex domains, long-lived systems, multiple delivery channels |

### When clean architecture pays off

- The domain has **real rules** (pricing, scheduling, workflows, compliance) that change and need heavy testing.
- The system will live for years, through infrastructure changes.
- The same logic is triggered from multiple places: HTTP, queues, cron, CLI.

### When it's overkill

- Mostly CRUD: "save this JSON, read it back." There's little domain logic to protect, so the layers become pass-throughs.
- Prototypes, internal tools, short-lived projects.
- Small teams where the ceremony slows delivery more than it protects.

A middle path many teams like: **layered architecture + repository abstraction + constructor injection** (`03`, `04`). You get testability and swappable data access without a dedicated entity layer and five circles.

---

## Common mistakes

```js
// ❌ "Clean" folders, but the domain still imports the ORM
import mongoose from "mongoose";           // inside domain/Order.js: the dependency rule is broken

// ❌ Returning ORM objects from repositories into use cases
return await OrderModel.findById(id);       // leaks Mongoose documents (with save(), populate(), ...) inward

// ❌ Interfaces with exactly one implementation, created "just in case" for trivial code
// ❌ Use case that takes (req, res)
// ❌ Anemic entities: only getters/setters, with all rules in use cases (logic ends up procedural)
// ❌ One use case that does everything ("OrderManager") → split by user action
// ❌ Putting validation of HTTP input (shape, types) in the domain: that's an edge concern
// ❌ Adopting all of this for a 6-endpoint CRUD app
```

## Checklist

- [ ] `domain/` and `application/` import nothing from Express, DB drivers, SDKs, or env config
- [ ] Use cases receive their collaborators as arguments
- [ ] Repositories translate between rows/documents and domain objects
- [ ] Domain errors carry codes, not HTTP statuses; one outer layer maps them
- [ ] A single composition root wires concrete implementations
- [ ] Import direction is enforced (lint rule or `dependency-cruiser`), not just agreed on
- [ ] You can run a use case in a plain script with fakes

## Next

**`03-repository-and-service-pattern.md`** takes a closer look at the two workhorse patterns used in both architectures: what belongs in a repository vs a service, transactions across repositories, and common pitfalls.
