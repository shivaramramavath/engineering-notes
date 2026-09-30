# MVC & Layered Architecture

The pragmatic default for most Node.js APIs: split code into layers where each one has a single job and only talks to the layer beneath it.

## MVC: where it comes from

**Model–View–Controller** was designed for GUI and server-rendered web apps:

```
        ┌──────────────┐
        │   Request    │
        └──────┬───────┘
               ▼
        ┌──────────────┐    asks for data    ┌──────────┐
        │  Controller  │ ──────────────────▶ │  Model   │
        │ (handles     │ ◀────────────────── │ (data +  │
        │  the request)│    returns data     │  rules)  │
        └──────┬───────┘                     └──────────┘
               │ passes data to
               ▼
        ┌──────────────┐
        │     View     │  (HTML template, or JSON representation)
        └──────┬───────┘
               ▼
        ┌──────────────┐
        │   Response   │
        └──────────────┘
```

| Part | Responsibility |
|---|---|
| **Model** | Data and business rules |
| **View** | How the data is presented (an EJS/Handlebars template, or a JSON serializer) |
| **Controller** | Receives input, calls the model, chooses the view/response |

In a JSON API, the "view" shrinks to a response mapping (a DTO), so the pattern becomes **Route → Controller → Model**.

### The problem with plain MVC in APIs

"Model" ends up meaning *two different things*:

1. The **database entity** (a Mongoose schema, a Sequelize model)
2. The **business logic** (pricing, stock checks, workflows)

Teams then put business logic either into controllers (fat controllers) or into the ORM models (fat models, tightly bound to the database). Both hurt. That's why most real Node.js projects evolve toward **layered architecture**, which gives business logic its own home.

---

## Layered architecture

Split the application horizontally into layers. Each layer may only depend on the one directly beneath it.

```
┌──────────────────────────────────────────────┐
│  Routes            URL + middleware wiring   │  HTTP
├──────────────────────────────────────────────┤
│  Controllers       parse request, send reply │  HTTP
├──────────────────────────────────────────────┤
│  Services          business logic            │  Domain
├──────────────────────────────────────────────┤
│  Repositories/DAL  database queries          │  Data
├──────────────────────────────────────────────┤
│  Database / external APIs                    │
└──────────────────────────────────────────────┘

   Requests flow DOWN; data flows back UP.
   A layer never reaches upward, and normally doesn't skip a layer.
```

### What each layer does, and must NOT do

| Layer | Responsible for | Must not |
|---|---|---|
| **Routes** | Mapping URL + method to a controller; attaching middleware (auth, validation, rate limit) | Contain logic |
| **Controllers** | Reading `req` (params, body, user), calling **one** service method, choosing the status code, shaping the response | Contain business rules or SQL; know about the database |
| **Services** | Business rules, orchestration across repositories, transactions, calling external services | Touch `req`/`res`; know the word "HTTP"; write SQL |
| **Repositories** | Reading/writing data: queries, ORM calls, mapping rows to objects | Contain business decisions |
| **Models / entities** | The shape of the data (and sometimes simple invariants) | Do I/O |

The test for whether code is in the right place: **could you call the service from a CLI script or a queue worker?** If the service takes `req` or calls `res.status(...)`, the answer is no, and it's in the wrong layer.

---

## Folder structure: two ways to organize

### Option A: by layer (technical grouping)

```
src/
├── routes/
│   ├── orders.routes.js
│   └── users.routes.js
├── controllers/
│   ├── orders.controller.js
│   └── users.controller.js
├── services/
│   ├── orders.service.js
│   └── users.service.js
├── repositories/
│   ├── orders.repository.js
│   └── users.repository.js
├── models/
├── middleware/
├── utils/
├── config/
└── app.js
```

✅ Easy to understand at a glance; fine for small projects.
❌ One feature (orders) is scattered across five folders; every change touches all of them; the folders grow to dozens of files each.

### Option B: by feature (domain grouping): recommended as you grow

```
src/
├── modules/
│   ├── orders/
│   │   ├── orders.routes.js
│   │   ├── orders.controller.js
│   │   ├── orders.service.js
│   │   ├── orders.repository.js
│   │   ├── orders.schemas.js        ← validation
│   │   ├── orders.dto.js            ← response mapping
│   │   └── orders.test.js
│   ├── users/
│   │   └── ...
│   └── products/
│       └── ...
├── shared/
│   ├── middleware/
│   ├── errors/
│   └── utils/
├── config/
├── app.js
└── server.js
```

✅ Everything about "orders" is in one place; easy to find, change, test, and eventually extract (`05-modular-monolith-vs-microservices.md`).
❌ Needs discipline about what goes in `shared/` and which modules may import which.

Inside each feature you still have the layers; you've just changed the *top-level* grouping.

---

## The running example, layer by layer

### Routes: wiring only

```js
// modules/orders/orders.routes.js
import { Router } from "express";
import { requireAuth } from "../../shared/middleware/auth.js";
import { validate } from "../../shared/middleware/validate.js";
import { createOrderSchema, orderIdParams } from "./orders.schemas.js";
import * as controller from "./orders.controller.js";

const router = Router();

router.use(requireAuth);

router.post("/", validate({ body: createOrderSchema }), controller.create);
router.get("/:id", validate({ params: orderIdParams }), controller.getById);

export default router;
```

### Controller: translate HTTP ↔ service calls

```js
// modules/orders/orders.controller.js
import * as orderService from "./orders.service.js";
import { toOrderDto } from "./orders.dto.js";

export async function create(req, res) {
  // req.body is already validated by middleware (see 09-api-development/04-validation.md)
  const order = await orderService.placeOrder({
    userId: req.user.id,
    items: req.body.items,
  });

  res.status(201).json({ data: toOrderDto(order) });
}

export async function getById(req, res) {
  const order = await orderService.getOrderForUser(req.params.id, req.user.id);
  res.json({ data: toOrderDto(order) });
}
```

Notice what the controller **doesn't** do: no `if (stock < qty)`, no SQL, no email. It reads input, calls a service, and formats output. A good controller method is 3–10 lines.

### Service: the business logic

```js
// modules/orders/orders.service.js
import * as orderRepo from "./orders.repository.js";
import * as productRepo from "../products/products.repository.js";
import * as userRepo from "../users/users.repository.js";
import * as mailer from "../../shared/mailer.js";
import { AppError } from "../../shared/errors/AppError.js";

const PREMIUM_DISCOUNT = 0.1;

export async function placeOrder({ userId, items }) {
  const user = await userRepo.findById(userId);
  if (!user) throw new AppError(404, "user_not_found", "User not found");

  const products = await productRepo.findByIds(items.map((i) => i.productId));

  // business rules
  let total = 0;
  for (const item of items) {
    const product = products.find((p) => p.id === item.productId);
    if (!product) throw new AppError(404, "product_not_found", `Product ${item.productId} not found`);
    if (product.stock < item.quantity) {
      throw new AppError(409, "out_of_stock", `Not enough stock for ${product.name}`);
    }
    total += product.priceCents * item.quantity;
  }
  if (user.isPremium) total = Math.round(total * (1 - PREMIUM_DISCOUNT));

  // persistence: one atomic operation (order + stock decrement)
  const order = await orderRepo.createWithStockUpdate({ userId, items, totalCents: total });

  // side effect: don't let an email failure fail the order
  mailer.sendOrderConfirmation(user.email, order).catch((err) => {
    console.error("Confirmation email failed", err);
  });

  return order;
}

export async function getOrderForUser(orderId, userId) {
  const order = await orderRepo.findByIdAndUser(orderId, userId);   // ownership in the query
  if (!order) throw new AppError(404, "order_not_found", "Order not found");
  return order;
}
```

Note: services throw **domain-meaningful errors** (`AppError`), which the central error handler turns into HTTP responses (`09-api-development/05-error-responses.md`). The service never calls `res.status(409)`; it just says what went wrong.

### Repository: data access only

```js
// modules/orders/orders.repository.js
import { pool } from "../../config/db.js";

export async function findByIdAndUser(orderId, userId) {
  const { rows } = await pool.query(
    "SELECT * FROM orders WHERE id = $1 AND user_id = $2",
    [orderId, userId]
  );
  return rows[0] ?? null;
}

export async function createWithStockUpdate({ userId, items, totalCents }) {
  const client = await pool.connect();
  try {
    await client.query("BEGIN");

    const { rows } = await client.query(
      "INSERT INTO orders (user_id, total_cents, status) VALUES ($1, $2, 'pending') RETURNING *",
      [userId, totalCents]
    );
    const order = rows[0];

    for (const item of items) {
      const res = await client.query(
        "UPDATE products SET stock = stock - $1 WHERE id = $2 AND stock >= $1",
        [item.quantity, item.productId]
      );
      if (res.rowCount === 0) throw new Error("Stock changed during checkout");   // race protection
      await client.query(
        "INSERT INTO order_items (order_id, product_id, quantity) VALUES ($1, $2, $3)",
        [order.id, item.productId, item.quantity]
      );
    }

    await client.query("COMMIT");
    return order;
  } catch (err) {
    await client.query("ROLLBACK");
    throw err;
  } finally {
    client.release();
  }
}
```

Transactions and the `stock >= $1` guard come from `07-databases/postgresql/02-transactions-and-indexing.md`. The repository details are expanded in `03-repository-and-service-pattern.md`.

### Wiring it up

```js
// app.js
import express from "express";
import ordersRouter from "./modules/orders/orders.routes.js";
import usersRouter from "./modules/users/users.routes.js";
import { errorHandler } from "./shared/middleware/errorHandler.js";

const app = express();
app.use(express.json());

app.use("/api/v1/orders", ordersRouter);
app.use("/api/v1/users", usersRouter);

app.use(errorHandler);
export default app;
```

```js
// server.js: separate from app.js so tests can import `app` without starting a server
import app from "./app.js";
app.listen(process.env.PORT ?? 3000);
```

Splitting `app.js` from `server.js` is a small habit with a big payoff for testing (`13-testing/02-api-testing-and-mocking.md`).

---

## Rules for layered architecture

1. **Dependencies only point down.** Controllers import services; services import repositories. Repositories never import services; services never import controllers.
2. **No layer skipping (usually).** Controllers shouldn't query the database directly. The occasional read-only shortcut may be acceptable, but make it a conscious exception.
3. **Services are framework-free.** No `req`, `res`, `next`, no Express imports.
4. **Return plain data from services and repositories.** Not HTTP concepts, not raw ORM internals that leak.
5. **Throw, don't respond.** Lower layers throw typed errors; one central handler translates them.
6. **One use case per service function**, named for the business action: `placeOrder`, `cancelOrder`, `refundPayment`, not `handleOrder`.
7. **Cross-cutting concerns go in middleware** (auth, logging, rate limiting, validation), not repeated in every layer.

### Common anti-patterns

```js
// ❌ Fat controller: business logic in the HTTP layer
export async function create(req, res) {
  const user = await User.findById(req.user.id);
  if (user.isPremium) total *= 0.9;           // pricing rule stuck in a controller
  ...
}

// ❌ Service that knows HTTP
export async function placeOrder(req, res) {     // takes req/res → can't reuse from a job
  ...
  res.status(201).json(order);
}

// ❌ Anemic pass-through layers: a service that just forwards to the repository
export const getUser = (id) => userRepo.findById(id);      // adds nothing; fine for trivial CRUD, noise if everywhere

// ❌ Repository with business rules
export async function findActiveUsers() {
  return db.query("... WHERE is_premium AND orders_count > 10 AND ...");  // "what is an active user?" is a business question
}

// ❌ God service: one 2,000-line users.service.js doing everything
// ❌ Circular imports: orders.service ↔ users.service
```

**On pass-through services:** for simple CRUD (`GET /tags`), a service that merely forwards is boilerplate. Many teams accept thin services for trivial operations, because consistency (always controller → service → repository) is worth more than saving five lines, and logic tends to arrive later. Decide once and be consistent.

---

## Keeping the DTO boundary clean

Controllers map between the **API shape** and the **internal shape**:

```js
// modules/orders/orders.dto.js
export function toOrderDto(order) {
  return {
    id: order.id,
    status: order.status,
    total: { amount: order.total_cents, currency: "USD" },
    createdAt: order.created_at.toISOString(),
  };
}
```

Snake_case columns, internal flags, and ORM metadata stay inside. If you rename a column, only the repository and DTO change: the public API doesn't (`09-api-development/01-rest-api-design.md`).

---

## Where does each kind of validation go?

| Kind | Where | Example |
|---|---|---|
| **Shape/format** (types, required fields, lengths) | Middleware/schema at the edge | "`items` must be a non-empty array" |
| **Business rules** (depend on state) | Service | "Insufficient stock", "Order already shipped" |
| **Data integrity** (last line of defense) | Database constraints | `UNIQUE`, `NOT NULL`, `CHECK (stock >= 0)`, foreign keys |

All three matter. Business rules checked in the service can race under concurrency; the database constraint is what truly guarantees correctness.

---

## Testing each layer

Layers give you a testing pyramid for free:

| Layer | Test type | What you need |
|---|---|---|
| Services | **Unit tests** | Fake/mock repositories: fast, no DB |
| Repositories | **Integration tests** | A real test database |
| Routes + controllers | **API tests** | `supertest` against `app`, services often stubbed or a test DB |

```js
// orders.service.test.js: pure business logic, no database
import { jest } from "@jest/globals";

test("premium users get 10% off", async () => {
  // Needs the service to receive its dependencies → see 04-dependency-injection.md
  const service = createOrderService({
    userRepo: { findById: async () => ({ id: "u1", isPremium: true, email: "a@b.com" }) },
    productRepo: { findByIds: async () => [{ id: "p1", name: "Pen", priceCents: 1000, stock: 10 }] },
    orderRepo: { createWithStockUpdate: async (o) => ({ id: "o1", ...o }) },
    mailer: { sendOrderConfirmation: async () => {} },
  });

  const order = await service.placeOrder({ userId: "u1", items: [{ productId: "p1", quantity: 2 }] });

  expect(order.totalCents).toBe(1800);      // 2000 - 10%
});
```

The service above is written as a *factory that receives its dependencies*, which is precisely what `04-dependency-injection.md` explains. With the module-import style shown earlier (`import * as orderRepo ...`), you'd need module mocking (`jest.unstable_mockModule`) instead, which works but is clumsier.

See `13-testing/01-unit-and-integration-testing.md`.

---

## Limits of layered architecture

Layered architecture is a strong default, but know its weak points:

- **Dependencies flow toward the database.** Services import repositories, which import the DB driver, so the business logic is transitively coupled to data-access choices. Swapping an ORM can ripple upward. (Clean architecture in `02` inverts this.)
- **Layers can become ceremony.** Five files for a trivial CRUD endpoint is overhead.
- **"Service" can become a dumping ground.** Without care, it accumulates every rule in the app and becomes a procedural script (the anemic domain model problem).
- **It says little about boundaries between features.** Nothing stops `orders` from reaching into `users`' internals, which is where modular structure (`05`) comes in.

## Checklist

- [ ] Controllers are thin (read input → call service → format output)
- [ ] No `req`/`res` in services; no SQL in controllers
- [ ] Business rules in services; queries in repositories
- [ ] One central error handler; services throw typed errors
- [ ] Feature-based folders once the project has more than a handful of resources
- [ ] `app.js` and `server.js` separated
- [ ] DTOs at the API boundary; no raw DB rows returned

## Next

**`02-clean-architecture.md`** takes the dependency problem above and solves it properly: making business rules depend on nothing, so the database and framework become replaceable details.
