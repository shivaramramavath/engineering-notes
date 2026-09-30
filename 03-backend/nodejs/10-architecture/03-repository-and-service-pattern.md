# Repository & Service Pattern

The two workhorse patterns behind both layered and clean architecture: repositories own *how data is stored*, services own *what the business does*.

## The two patterns in one sentence each

- **Repository:** a collection-like interface to your data. Callers ask for objects (`orders.findById(id)`); the repository hides whether that's SQL, MongoDB, a cache, or an in-memory array.
- **Service:** a home for business logic. It coordinates repositories, enforces rules, manages transactions, and triggers side effects, with no knowledge of HTTP.

Together they split a request's work into three questions:

| Question | Answered by |
|---|---|
| How do I talk to HTTP clients? | Controller |
| What should happen, and is it allowed? | **Service** |
| How is the data stored and fetched? | **Repository** |

---

## The Repository pattern

### Why not call the ORM/driver directly everywhere?

```js
// ❌ queries scattered through services and controllers
const user = await User.findOne({ email }).populate("orders");     // Mongoose everywhere
const rows = await pool.query("SELECT ... FROM users WHERE ...");  // SQL everywhere
```

Problems:

- **Duplication:** the same query is copy-pasted in five places; fix one, forget four.
- **Coupling:** changing the database, ORM, or column names means editing business logic.
- **Testing:** every service test needs a database or deep mocking of library internals.
- **Leaky abstractions:** ORM-specific objects (`doc.save()`, lazy-loaded relations) escape into code that shouldn't know about them.

A repository gives each aggregate (users, orders, products) **one place** for persistence.

### What a repository looks like

```js
// modules/users/users.repository.js (PostgreSQL, using pg)
import { pool } from "../../config/db.js";

// Map a database row → a plain application object
const toUser = (row) =>
  row && {
    id: row.id,
    email: row.email,
    name: row.name,
    isPremium: row.is_premium,
    createdAt: row.created_at,
  };

export const userRepository = {
  async findById(id) {
    const { rows } = await pool.query("SELECT * FROM users WHERE id = $1", [id]);
    return toUser(rows[0]) ?? null;
  },

  async findByEmail(email) {
    const { rows } = await pool.query("SELECT * FROM users WHERE email = $1", [email]);
    return toUser(rows[0]) ?? null;
  },

  async create({ email, name, passwordHash }) {
    const { rows } = await pool.query(
      "INSERT INTO users (email, name, password_hash) VALUES ($1, $2, $3) RETURNING *",
      [email, name, passwordHash]
    );
    return toUser(rows[0]);
  },

  async update(id, fields) {
    // column names come from OUR allow-list, never from the caller
    const COLUMNS = { name: "name", isPremium: "is_premium" };
    const sets = [];
    const values = [];
    for (const [key, value] of Object.entries(fields)) {
      if (!COLUMNS[key]) continue;
      values.push(value);
      sets.push(`${COLUMNS[key]} = $${values.length}`);
    }
    if (sets.length === 0) return this.findById(id);

    values.push(id);
    const { rows } = await pool.query(
      `UPDATE users SET ${sets.join(", ")} WHERE id = $${values.length} RETURNING *`,
      values
    );
    return toUser(rows[0]) ?? null;
  },

  async delete(id) {
    const { rowCount } = await pool.query("DELETE FROM users WHERE id = $1", [id]);
    return rowCount > 0;
  },
};
```

The **same contract** in MongoDB (Mongoose). Callers can't tell the difference:

```js
// users.repository.mongo.js
import { UserModel } from "./user.model.js";

const toUser = (doc) =>
  doc && {
    id: doc._id.toString(),
    email: doc.email,
    name: doc.name,
    isPremium: doc.isPremium,
    createdAt: doc.createdAt,
  };

export const userRepository = {
  async findById(id) {
    return toUser(await UserModel.findById(id).lean());         // .lean() → plain object, not a Mongoose doc
  },
  async findByEmail(email) {
    return toUser(await UserModel.findOne({ email }).lean());
  },
  async create({ email, name, passwordHash }) {
    const doc = await UserModel.create({ email, name, passwordHash });
    return toUser(doc.toObject());
  },
  // ...
};
```

### Repository design rules

1. **Return plain objects** (or domain entities), never ORM documents, query builders, or cursors. The mapping function (`toUser`) is the seam that keeps database shape from leaking.
2. **Name methods for the business question,** not the query mechanism: `findActiveSubscribers()`, not `selectWhereStatusEquals()`. But keep them about *data access*, not decisions.
3. **One repository per aggregate/entity root,** not per table. An order and its line items are loaded and saved together through `orderRepository`.
4. **Return `null` for "not found"** on lookups; let the *service* decide that it's an error (a 404 vs "create one" vs "ignore"). Repositories rarely throw domain errors.
5. **No business rules.** "A premium user with 10+ orders is VIP" belongs in a service or entity. The repository may offer `countByUser(id)`, and the service applies the rule.
6. **Be explicit about what's loaded.** Avoid lazy loading that triggers surprise queries later (N+1). Offer `findByIdWithItems(id)` if callers need the items.
7. **Put ownership and tenancy filters in the query** (`WHERE id = $1 AND user_id = $2`) to prevent IDOR (`08-authentication-security/05-common-vulnerabilities.md`).

### What belongs in a repository

```js
// ✅ data access with a business-flavored name
findById, findByEmail, findByIds, listByUser({ userId, cursor, limit }), create, update, delete,
countByStatus, existsByEmail, findStaleSessions(olderThan)

// ❌ business decisions disguised as queries
isUserEligibleForDiscount(userId)       // a rule → service/entity
placeOrder(...)                          // a use case → service
sendPasswordResetEmail(...)              // side effect → service + mailer
```

### List queries: the contract for filtering and pagination

Connect this to `09-api-development/02-versioning-and-pagination.md` and `03-filtering-and-sorting.md`: the repository takes **already-validated, structured** options, never raw query strings.

```js
// orders.repository.js
async list({ userId, status, limit, cursor }) {
  const params = [userId];
  const where = ["user_id = $1"];

  if (status) { params.push(status); where.push(`status = $${params.length}`); }
  if (cursor) {
    params.push(cursor.createdAt, cursor.id);
    where.push(`(created_at, id) < ($${params.length - 1}, $${params.length})`);
  }

  params.push(limit + 1);
  const { rows } = await pool.query(
    `SELECT * FROM orders WHERE ${where.join(" AND ")}
     ORDER BY created_at DESC, id DESC LIMIT $${params.length}`,
    params
  );
  return rows.map(toOrder);
}
```

---

## The Service pattern

### The job of a service

A service implements **use cases**: the things your application *does*. It is where rules, orchestration, and cross-cutting business flow live.

```js
// modules/auth/auth.service.js
import bcrypt from "bcrypt";
import { userRepository } from "../users/users.repository.js";
import { AppError } from "../../shared/errors/AppError.js";

export async function register({ email, name, password }) {
  // business rule: emails are unique (the DB unique index is the real guarantee)
  const existing = await userRepository.findByEmail(email);
  if (existing) throw new AppError(409, "email_taken", "That email is already registered");

  const passwordHash = await bcrypt.hash(password, 12);
  const user = await userRepository.create({ email, name, passwordHash });

  return user;
}
```

### What services are responsible for

| Responsibility | Example |
|---|---|
| **Business rules** | Discounts, eligibility, state transitions ("can't cancel a shipped order") |
| **Orchestration** | Load user → load products → compute → save → notify |
| **Transactions** | Making several repository calls succeed or fail together |
| **Calling external services** | Payments, email, storage, other internal services |
| **Authorization at the business level** | "Only the project owner may archive it" |
| **Emitting events** | `order.placed` for other modules to react to |

### What services must not do

- Touch `req`, `res`, `next`, or know HTTP status codes (throw typed errors instead)
- Contain SQL or ORM calls (delegate to repositories)
- Parse or validate raw HTTP input shape (that's the edge: `09-api-development/04-validation.md`)
- Format JSON responses (controller/DTO job)

### Naming and granularity

```js
// ✅ named for the business action, one per use case
placeOrder, cancelOrder, refundOrder, registerUser, verifyEmail, requestPasswordReset

// ❌ vague or catch-all
handleOrder, processData, UserManager, OrderHelper, doStuff
```

If a service file passes ~300–400 lines or a function needs many unrelated dependencies, split it by use case (`placeOrder.js`, `cancelOrder.js`) or by sub-domain.

### Services calling services

Sometimes `orders` needs something from `inventory`. Prefer calling the other module's **public service API**, never its repository or tables:

```js
// orders.service.js
import { inventoryService } from "../inventory/index.js";    // the module's public surface

await inventoryService.reserve(items);                        // ✅
// import { inventoryRepository } from "../inventory/inventory.repository.js";   ❌ reaching into internals
```

Watch for **circular dependencies** (`orders` → `users` → `orders`). If two modules need each other, extract the shared concept, or communicate with events. More in `05-modular-monolith-vs-microservices.md`.

---

## Transactions across repositories

Placing an order touches orders, order items, and products. All must commit together or not at all. Whose job is the transaction?

The **service** knows *what* must be atomic; the **repository** knows *how* a transaction is implemented in this database. The standard approach: the service opens a unit of work and passes the transaction handle to repository calls.

### Option 1: Repository method that owns the whole atomic operation

Simplest, and good when one aggregate is involved:

```js
// the repository hides the transaction details behind a business-shaped method
await orderRepository.createWithStockUpdate({ userId, items, totalCents });
```

(This is what `01-mvc-and-layered-architecture.md` does.)

### Option 2: Pass a transaction/client through

When a service coordinates **several repositories**, let each accept an optional transaction context:

```js
// db/transaction.js
import { pool } from "../config/db.js";

export async function withTransaction(work) {
  const client = await pool.connect();
  try {
    await client.query("BEGIN");
    const result = await work(client);       // `client` is the transaction handle
    await client.query("COMMIT");
    return result;
  } catch (err) {
    await client.query("ROLLBACK");
    throw err;
  } finally {
    client.release();
  }
}
```

```js
// repositories accept an executor: the pool by default, or a transaction client
export const productRepository = {
  async decrementStock(productId, quantity, db = pool) {
    const { rowCount } = await db.query(
      "UPDATE products SET stock = stock - $1 WHERE id = $2 AND stock >= $1",
      [quantity, productId]
    );
    return rowCount === 1;
  },
};

export const orderRepository = {
  async insert(order, db = pool) {
    await db.query(
      "INSERT INTO orders (id, user_id, total_cents) VALUES ($1, $2, $3)",
      [order.id, order.userId, order.totalCents]
    );
  },
};
```

```js
// orders.service.js: the service decides WHAT is atomic
import { withTransaction } from "../../db/transaction.js";

export async function placeOrder({ userId, items }) {
  // ... load data, apply business rules ...

  return withTransaction(async (tx) => {
    await orderRepository.insert(order, tx);
    for (const item of items) {
      const ok = await productRepository.decrementStock(item.productId, item.quantity, tx);
      if (!ok) throw new AppError(409, "out_of_stock", "Stock changed during checkout");
    }
    return order;
  });
}
```

Note: don't hold a transaction open across slow external calls (HTTP to a payment provider). Keep transactions short, do external I/O before or after, and use idempotency keys (`09-api-development/06-idempotency.md`) for the external side. With MongoDB the equivalent is `session.withTransaction(...)` (`07-databases/mongodb/03-transactions.md`).

---

## Pitfalls

### 1. The generic repository

```js
// ❌ a "universal" repository with find(filter), save(entity)
class Repository {
  find(filter) { return this.model.find(filter); }     // callers build raw queries again
}
```

A generic CRUD base class re-exposes the ORM to everyone: callers pass arbitrary filters, and you've gained an extra layer with none of the isolation. A small shared base for genuinely common operations (`findById`, `delete`) is OK; don't hide the ORM *behind* it while leaking its filter language *through* it.

### 2. Repository on top of an ORM that's already a repository

ORMs like Prisma, TypeORM, and Sequelize already give you a query API. Wrapping them still has value: isolating them from services, returning plain objects, and encoding business-named queries, but **don't** write a repository that merely renames `prisma.user.findUnique` to `findUnique`. Make each method carry meaning.

### 3. Anemic, pass-through services

```js
export const getUser = (id) => userRepository.findById(id);     // adds nothing
```

Acceptable for trivial reads (keeps controllers uniform), wasteful everywhere else. If 80% of your services look like this, your app is mostly CRUD and a lighter structure is fine. Logic tends to arrive later, and the seam is ready when it does.

### 4. Business logic leaking into repositories or controllers

```js
// ❌ repository deciding who's "active"
findActiveUsers() { /* WHERE last_login > now() - interval '30 days' AND is_verified AND ... */ }
// ✅ service defines "active"; repository offers a precise query
const since = daysAgo(ACTIVE_WINDOW_DAYS);
const users = await userRepository.findLoggedInSince(since);
```

### 5. N+1 queries hidden behind clean interfaces

```js
// ❌ looks clean, runs 1 + N queries
for (const order of orders) {
  order.customer = await userRepository.findById(order.userId);
}

// ✅ batch
const users = await userRepository.findByIds(orders.map((o) => o.userId));
```

Repositories should offer **batch methods** (`findByIds`). See `15-performance/03-database-optimization.md`.

### 6. Leaking database errors upward

A raw unique-violation (`23505`, or Mongo `11000`) reaching a controller couples it to the database. Translate at the repository boundary:

```js
async create(data) {
  try {
    /* INSERT ... */
  } catch (err) {
    if (err.code === "23505") throw new AppError(409, "duplicate", "Already exists");
    throw err;
  }
}
```

### 7. Forgetting concurrency

Service-level checks ("stock is enough") can race. Back every important rule with a **database-level guarantee**: unique indexes, `CHECK` constraints, conditional updates (`WHERE stock >= $1`), or optimistic locking (a `version` column).

---

## Testing with this structure

### Service tests: fake the repositories

```js
// with module-level imports you'd mock modules; with injection (next file) you just pass fakes
function makeFakeUserRepo(users = []) {
  return {
    async findByEmail(email) { return users.find((u) => u.email === email) ?? null; },
    async create(data) { const u = { id: String(users.length + 1), ...data }; users.push(u); return u; },
  };
}

test("register rejects a duplicate email", async () => {
  const service = makeAuthService({ userRepository: makeFakeUserRepo([{ id: "1", email: "a@b.com" }]) });

  await expect(service.register({ email: "a@b.com", name: "A", password: "x".repeat(12) }))
    .rejects.toMatchObject({ code: "email_taken" });
});
```

An **in-memory repository** is a great test double: a simple class with the same methods as the real one, backed by an array or `Map`. It's faster than mocks to write for multi-step flows and catches misuse.

### Repository tests: use a real database

Mocking `pool.query` proves nothing about your SQL. Test repositories against a real PostgreSQL/MongoDB (a container, or Testcontainers), ideally with each test wrapped in a transaction that rolls back, or a clean schema per run (`13-testing/03-test-database-and-coverage.md`).

```js
test("findByEmail returns null for unknown emails", async () => {
  expect(await userRepository.findByEmail("nobody@example.com")).toBeNull();
});

test("create then findById round-trips", async () => {
  const created = await userRepository.create({ email: "x@y.com", name: "X", passwordHash: "h" });
  expect(await userRepository.findById(created.id)).toMatchObject({ email: "x@y.com" });
});
```

---

## Quick reference

| | Repository | Service |
|---|---|---|
| Purpose | Persistence access | Business logic & orchestration |
| Knows about | The database/ORM | Repositories, other services, external APIs |
| Doesn't know about | Business rules, HTTP | SQL, HTTP |
| Returns | Plain objects / entities, `null` if missing | Domain results; throws domain/app errors |
| Transactions | Implements the mechanics | Decides what's atomic |
| Tested with | Real database | Fake/in-memory repositories |
| Named after | Data concepts (`findByEmail`) | Business actions (`placeOrder`) |

## Checklist

- [ ] One repository per aggregate; methods named for data needs
- [ ] Repositories return plain objects, not ORM documents
- [ ] No business rules in repositories; no SQL in services
- [ ] Services named for use cases, free of `req`/`res`
- [ ] Transactions scoped in the service, implemented via a transaction handle
- [ ] Batch methods exist to avoid N+1
- [ ] Critical invariants enforced by DB constraints as well as service checks
- [ ] Repositories tested against a real DB; services tested with fakes

## Next

**`04-dependency-injection.md`** answers the question this file kept hinting at: how do services *receive* their repositories so you can swap real ones for fakes, without a framework?
