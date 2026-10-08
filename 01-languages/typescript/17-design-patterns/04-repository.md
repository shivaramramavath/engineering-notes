# Repository

A **repository** is an interface that makes persistent data look like an in-memory collection of domain objects: `find`, `save`, `delete`, and queries phrased in domain terms. Application code asks the repository for a `User`. It does not write SQL, build ORM queries, or know whether the data lives in Postgres, a file, or memory. That separation makes business logic easier to test, and makes storage details replaceable.

**Prerequisites:**
- [Interfaces](../04-objects-and-interfaces/00-interfaces.md)
- [Adapter](./03-adapter.md) (a repository adapts a data store to a domain interface)
- [The DTO pattern](../16-type-safe-apis/02-dto-pattern.md)
- [async/await](../12-async-and-iteration/02-async-await.md)

---

## The problem

```ts
class OrderService {
  async cancel(orderId: string) {
    const row = await db.query("select * from orders where id = $1", [orderId]);
    if (row.rows.length === 0) throw new Error("not found");
    if (row.rows[0].status === "shipped") throw new Error("too late");
    await db.query("update orders set status = 'cancelled' where id = $1", [orderId]);
  }
}
```

Business rules (cannot cancel a shipped order) are tangled with SQL, column names, and row shapes. Testing this requires a database. Changing the schema means editing business code.

## The pattern

Define an interface in domain terms, and implement it per storage technology:

```ts
interface Order {
  id: string;
  status: "pending" | "shipped" | "cancelled";
  totalCents: number;
}

interface OrderRepository {
  findById(id: string): Promise<Order | null>;
  save(order: Order): Promise<void>;
}
```

The service now depends only on the interface:

```ts
class OrderService {
  constructor(private readonly orders: OrderRepository) {}

  async cancel(orderId: string): Promise<void> {
    const order = await this.orders.findById(orderId);
    if (!order) throw new NotFoundError("Order", orderId);
    if (order.status === "shipped") throw new ConflictError("Order already shipped");

    await this.orders.save({ ...order, status: "cancelled" });
  }
}
```

Two implementations:

```ts
// Production: SQL
class PostgresOrderRepository implements OrderRepository {
  constructor(private readonly db: Pool) {}

  async findById(id: string): Promise<Order | null> {
    const { rows } = await this.db.query("select id, status, total_cents from orders where id = $1", [id]);
    return rows[0] ? toOrder(rows[0]) : null;
  }

  async save(order: Order): Promise<void> {
    await this.db.query(
      "insert into orders (id, status, total_cents) values ($1, $2, $3) " +
        "on conflict (id) do update set status = excluded.status, total_cents = excluded.total_cents",
      [order.id, order.status, order.totalCents],
    );
  }
}

// Tests: in memory
class InMemoryOrderRepository implements OrderRepository {
  private readonly orders = new Map<string, Order>();

  async findById(id: string) {
    return this.orders.get(id) ?? null;
  }

  async save(order: Order) {
    this.orders.set(order.id, order);
  }
}
```

A test needs no database:

```ts
const repo = new InMemoryOrderRepository();
await repo.save({ id: "1", status: "shipped", totalCents: 500 });

await expect(new OrderService(repo).cancel("1")).rejects.toThrow("already shipped");
```

Supply the right implementation through [dependency injection](./05-dependency-injection.md): Postgres in production, in-memory in tests.

## Design guidelines

### Speak the domain's language

Name methods for what the business asks, not for how the store works:

```ts
interface UserRepository {
  findById(id: string): Promise<User | null>;
  findByEmail(email: string): Promise<User | null>;
  findInactiveSince(date: Date): Promise<User[]>;     // a domain question
  save(user: User): Promise<void>;
  delete(id: string): Promise<void>;
}
```

Avoid methods that leak storage: `runQuery(sql)`, `getCollection()`, `findWhere(rawFilter)`.

### Return domain types, not rows or ORM entities

The repository translates between storage shape and domain objects. Row mapping lives **inside** the repository:

```ts
interface OrderRow { id: string; status: string; total_cents: number }

function toOrder(row: OrderRow): Order {
  return { id: row.id, status: parseStatus(row.status), totalCents: row.total_cents };
}
```

If the database might contain unexpected values, validate while mapping instead of casting ([trust boundaries](../15-runtime-validation/00-trust-boundaries.md)). See the [DTO pattern](../16-type-safe-apis/02-dto-pattern.md) for the same idea at the API boundary.

### "Not found" is a normal outcome

Choose a convention and keep it consistent:

- `findById` returns `T | null` (absence is expected).
- A separate `getById` that **throws** a `NotFoundError` when the caller requires existence.

```ts
async getById(id: string): Promise<Order> {
  const order = await this.findById(id);
  if (!order) throw new NotFoundError("Order", id);
  return order;
}
```

Returning `null` forces callers to handle absence, since `strictNullChecks` rejects using it unchecked ([strict mode](../13-compiler-and-tsconfig/01-strict-mode.md)).

### Typed queries and pagination

Express filters as typed objects instead of strings:

```ts
interface OrderFilter {
  status?: Order["status"];
  placedAfter?: Date;
}

interface OrderRepository {
  list(filter: OrderFilter, page: CursorPageQuery): Promise<CursorPage<Order>>;
}
```

Reuse the page types from [pagination types](../16-type-safe-apis/03-pagination-types.md). Whitelist sortable fields with a union so callers cannot inject column names.

### A generic base repository: use with caution

It is tempting to write one generic CRUD interface for everything:

```ts
interface Repository<T, ID = string> {
  findById(id: ID): Promise<T | null>;
  findAll(): Promise<T[]>;
  save(entity: T): Promise<void>;
  delete(id: ID): Promise<void>;
}
```

This is fine as a **starting point**, but resist forcing every aggregate into it:

- Not every entity supports every operation (an audit log is append-only, so `delete` makes no sense).
- `findAll` on a large table is a bug waiting to happen.
- Domain-specific queries still need specific methods.

Prefer **small, specific interfaces** per aggregate. Share implementation code through helper functions rather than a rigid base interface.

### Transactions and consistency

Operations that must succeed together need a transaction boundary. Options:

- Let the repository method be the transaction (`saveOrderWithItems(order)`), keeping one aggregate per transaction.
- Pass a transaction handle in (`repo.withTransaction(tx)`), or use a **unit of work** that groups repositories and commits once.
- Use optimistic concurrency with a `version` field: `save` fails if the stored version has changed since the read.

```ts
interface Order { id: string; version: number; /* ... */ }

async save(order: Order): Promise<void> {
  const result = await this.db.query(
    "update orders set status = $1, version = version + 1 where id = $2 and version = $3",
    [order.status, order.id, order.version],
  );
  if (result.rowCount === 0) throw new ConflictError("Order was modified by someone else");
}
```

## Repositories and ORMs

ORMs and query builders (Prisma, TypeORM, Drizzle, and others) already offer collection-like APIs. Whether to wrap them depends on what you gain:

| Wrap the ORM in a repository when | Use the ORM directly when |
|---|---|
| You want business logic testable without a database | The app is small CRUD, and tests can use a real test database |
| You may change storage or want to isolate vendor types | The ORM is a deliberate, permanent choice |
| Queries are domain-specific and worth naming | Operations are generic and simple |
| ORM entities should not leak into services or APIs | The ORM's types are acceptable throughout |

A thin repository over an ORM mostly earns its keep by returning **domain types** and exposing **domain-named queries**. If every method just forwards to the ORM unchanged, it adds indirection with no value.

See [service and repository layers](../20-nodejs-backend/04-service-and-repository-layers.md) and [databases](../20-nodejs-backend/05-databases.md).

## Testing

- **Unit tests** for services use an in-memory repository.
- **Integration tests** for the real repository run against an actual database (a local container or a test schema), because SQL, constraints, and mapping are where bugs live.
- **Contract tests:** run the same test suite against both implementations, so the in-memory fake cannot drift from real behavior. If they differ, tests passing against the fake prove nothing.

```ts
function describeOrderRepository(name: string, create: () => OrderRepository) {
  describe(name, () => {
    it("returns null for a missing order", async () => {
      expect(await create().findById("missing")).toBeNull();
    });
    // the same behavioral tests run against every implementation
  });
}

describeOrderRepository("in-memory", () => new InMemoryOrderRepository());
describeOrderRepository("postgres", () => new PostgresOrderRepository(testPool));
```

See [integration testing](../18-testing-and-debugging/01-integration-testing.md) and [mocking](../18-testing-and-debugging/02-mocking.md).

## Important rules and misconceptions

- **A repository is not a DAO with a new name.** It presents a collection of **domain objects** and speaks domain language. A DAO mirrors tables and operations.
- **Repositories do not contain business rules.** They load and store. Rules live in services or the domain model.
- **An in-memory fake must behave like the real thing,** including "not found", uniqueness, and ordering.
- **A repository per table is not the goal.** It is one per aggregate or consistency boundary.
- **Returning ORM entities defeats the purpose,** since storage details reach every consumer.

## Common mistakes

- A generic `Repository<T>` forced onto everything, with `findAll()` used on large tables.
- Leaking ORM entities, query builders, or SQL through the interface.
- Business logic inside repository methods.
- An in-memory fake that is more forgiving than the real database.
- Repositories calling other repositories in loops (N+1 queries) instead of a purpose-built query.
- No strategy for concurrent updates.
- Mapping rows with `as` instead of validating or converting.
- Throwing on "not found" in `findById`, then catching it everywhere to mean "does not exist".

## Debugging

- Log the SQL and parameters in the real repository to see what is actually executed.
- If a service passes tests with the fake but fails in production, add a contract test that runs against the real implementation.
- If data has the wrong shape in the domain, check the row-to-domain mapping and column types (dates, numbers returned as strings).
- For slow code paths, look for repositories called inside loops, and add a batch method (`findByIds`).
- If updates are lost, check transactions and add optimistic concurrency.

## Quick summary

- A repository is a collection-like interface over persistence, expressed in domain terms and returning domain objects.
- Services depend on the interface. Implementations (Postgres, in-memory) are swappable, and tests use the in-memory one.
- Keep interfaces small and specific per aggregate. Be wary of a generic CRUD base, and name queries after domain questions.
- Mapping from rows lives inside the repository, with validation where data may be unexpected.
- Decide how "not found", transactions, and concurrency work, and verify fakes and real implementations with the same contract tests.

**Next:** [Dependency injection](./05-dependency-injection.md)
