# Service and Repository Layers

As a backend grows, putting everything in route handlers stops working: business rules are tangled with HTTP details and SQL, nothing can be tested without a server and a database, and a change to one concern ripples through all of them. **Layering** separates concerns so each piece has one job and depends on the next one only through an interface. The common arrangement is controllers (HTTP), services (business logic), and repositories (persistence). This note explains what belongs in each layer, how types flow between them, and when layers are overkill.

**Prerequisites:**
- [Express](./02-express.md) and [middleware](./03-middleware.md)
- [Repository](../17-design-patterns/04-repository.md) and [dependency injection](../17-design-patterns/05-dependency-injection.md)
- [The DTO pattern](../16-type-safe-apis/02-dto-pattern.md)

---

## The layers

```text
   HTTP request
        |
        v
+-----------------+   parse + validate input, call a service, map result to a DTO and status
|   Controller    |   (knows HTTP: req, res, status codes, headers)
|   / route       |
+--------+--------+
         |  plain typed values (no req/res)
         v
+-----------------+   business rules, orchestration, transactions, permissions
|     Service     |   (knows the domain; knows nothing about HTTP or SQL)
+--------+--------+
         |  domain objects and queries expressed in domain terms
         v
+-----------------+   load and store domain objects
|   Repository    |   (knows SQL/ORM; knows nothing about HTTP or business rules)
+--------+--------+
         |
         v
     Database / external systems
```

The rule that keeps this useful is the **direction of dependencies**: each layer calls the one below, never the one above, and never skips ahead. A controller does not write SQL, and a repository does not know what an HTTP status is.

## What goes where

| Layer | Responsibilities | Does **not** do |
|---|---|---|
| **Controller** (route handler) | read `req`; validate and convert input; call a service; map the result to a DTO; choose the status code | business rules, SQL, anything reusable outside HTTP |
| **Service** | enforce business rules; decide what must happen; coordinate repositories and external services; manage transactions; throw domain errors | touch `req`/`res`; build SQL; format HTTP responses |
| **Repository** | translate between the database and domain objects; run queries | enforce business rules; know who the current user is; send emails |

## An example, layer by layer

### Domain types

```ts
// domain: what the business talks about
export interface User {
  id: string;
  email: string;
  passwordHash: string;
  role: "admin" | "member";
  createdAt: Date;
}

export interface NewUser { email: string; passwordHash: string }
```

### Repository: persistence in domain terms

```ts
export interface UserRepository {
  findById(id: string): Promise<User | null>;
  findByEmail(email: string): Promise<User | null>;
  insert(user: NewUser): Promise<User>;
}

export class PostgresUserRepository implements UserRepository {
  constructor(private readonly db: Queryable) {}

  async findByEmail(email: string): Promise<User | null> {
    const { rows } = await this.db.query<UserRow>(
      "select id, email, password_hash, role, created_at from users where email = $1",
      [email],
    );
    return rows[0] ? toUser(rows[0]) : null;           // mapping lives here
  }
  // findById, insert ...
}
```

It returns **domain objects** and speaks domain language (`findByEmail`), as in [repository](../17-design-patterns/04-repository.md). Rows never leave this class.

### Service: business rules

```ts
export class UserService {
  constructor(
    private readonly users: UserRepository,
    private readonly hasher: PasswordHasher,
    private readonly mailer: Mailer,
  ) {}

  async register(input: { email: string; password: string }): Promise<User> {
    const existing = await this.users.findByEmail(input.email);
    if (existing) throw new ConflictError("Email already registered");      // a business rule

    const user = await this.users.insert({
      email: input.email,
      passwordHash: await this.hasher.hash(input.password),
    });

    await this.mailer.send(user.email, "Welcome!");
    return user;
  }

  async getById(id: string): Promise<User> {
    const user = await this.users.findById(id);
    if (!user) throw new NotFoundError("User", id);
    return user;
  }
}
```

The service takes **plain values** and returns **domain objects**. It throws **domain errors** (`ConflictError`, `NotFoundError`) that do not mention HTTP. It depends on repositories and other services through **interfaces**, injected through the constructor ([dependency injection](../17-design-patterns/05-dependency-injection.md)).

### Controller: HTTP in, HTTP out

```ts
export function createUserRouter(service: UserService): Router {
  const router = Router();

  router.post("/", async (req, res) => {
    const input = CreateUserSchema.parse(req.body);          // validate: boundary
    const user = await service.register(input);              // business logic
    res.status(201).json(toUserDto(user));                   // map: boundary
  });

  router.get("/:id", async (req, res) => {
    const user = await service.getById(req.params.id);
    res.json(toUserDto(user));
  });

  return router;
}
```

The controller is a thin adapter between HTTP and the service. Errors thrown by the service (`NotFoundError`, `ConflictError`) reach the central error handler, which maps them to status codes in one place ([middleware](./03-middleware.md), [error response types](../16-type-safe-apis/04-error-response-types.md)).

### Wiring it up

Create the real objects once, in the **composition root**:

```ts
const pool = createPool(config.db);
const users = new PostgresUserRepository(pool);
const service = new UserService(users, new ArgonHasher(), new SmtpMailer(config.smtp));
const app = createApp({ userService: service });
```

## Types across the layers

Each boundary has its own type, so a change on one side does not ripple through the others:

| Boundary | Type |
|---|---|
| Client to controller | **input DTO** (validated by a schema) |
| Controller to service | plain parameters or a command object |
| Service to repository | domain objects, `NewUser`, filters |
| Repository to database | rows (private to the repository) |
| Service to controller | domain objects |
| Controller to client | **output DTO** (mapped, no secrets, wire-format types) |

Do not pass `req`, `res`, ORM entities, or DTOs through the whole stack. In particular, **never let a service depend on `Request`**: that ties business logic to Express and makes it untestable without HTTP. If a service needs to know who is calling, pass the user (or a small context object) as a parameter:

```ts
async function cancelOrder(actor: AuthUser, orderId: string): Promise<void> {
  const order = await this.orders.getById(orderId);
  if (order.userId !== actor.id && actor.role !== "admin") throw new ForbiddenError();
  // ...
}
```

Passing the actor explicitly makes authorization rules visible and testable ([authentication and authorization](./07-authentication-and-authorization.md)).

## Transactions

An operation that changes several things must succeed or fail as a unit. The **service** decides what belongs in one transaction (it knows the business operation), and the **repositories** run inside it.

A common approach is a transaction helper that hands repositories bound to one connection:

```ts
export interface UnitOfWork {
  run<T>(work: (repos: { users: UserRepository; orders: OrderRepository }) => Promise<T>): Promise<T>;
}

async placeOrder(userId: string, items: Item[]): Promise<Order> {
  return this.uow.run(async ({ users, orders }) => {
    const user = await users.getById(userId);
    const order = await orders.insert({ userId: user.id, items });
    await users.recordPurchase(user.id, order.totalCents);
    return order;                          // commits on success, rolls back if anything throws
  });
}
```

The implementation (`BEGIN`, `COMMIT`/`ROLLBACK`, one connection) lives in infrastructure code. The service only describes **what** must be atomic. See [databases](./05-databases.md).

## Errors across layers

- **Repositories** translate storage failures into meaningful errors where the caller can act (a unique-constraint violation can become a `ConflictError`), and let unexpected ones propagate.
- **Services** throw **domain errors** with a `code`, or return a `Result` for expected failures ([custom errors](../11-error-handling/01-custom-errors.md), [the Result pattern](../11-error-handling/02-result-pattern.md)).
- **Controllers** do not catch and convert errors one by one. A central error handler maps them to responses and logs once ([error handling strategies](../11-error-handling/03-error-handling-strategies.md)).

Wrap lower-level errors with `{ cause }` when adding context, so logs keep the full chain.

## Project structure

Group by **feature**, with the layers inside each feature, rather than one folder per layer for the whole app:

```text
src/
├── users/
│   ├── user.routes.ts        controller
│   ├── user.service.ts
│   ├── user.repository.ts    interface + Postgres implementation
│   ├── user.schemas.ts       input schemas / DTOs
│   └── user.types.ts         domain types
├── orders/
│   └── ...
├── shared/                   errors, logging, db helpers
├── app.ts                    createApp(deps)
└── server.ts                 composition root and listen
```

Features that change together live together, and each has a small public surface ([project structure](../24-best-practices/02-project-structure.md), [barrel files and module organization](../08-modules/02-barrel-files-and-module-organization.md)).

## Testing each layer

| Layer | How to test |
|---|---|
| **Service** | unit tests with in-memory fake repositories and fake collaborators, covering business rules and error cases |
| **Repository** | integration tests against a real database, in the same suite as the in-memory fake (contract tests) |
| **Controller** | HTTP tests through the app with a fake or real service, checking status codes, validation, and mapping |
| **Whole flow** | a few end-to-end integration tests through HTTP and a real database |

Because services depend only on interfaces, the largest and most valuable test set (business rules) needs neither a server nor a database ([unit testing](../18-testing-and-debugging/00-unit-testing.md), [integration testing](../18-testing-and-debugging/01-integration-testing.md)).

## When layers are too much

Layering costs files, types, and indirection. It earns its keep when there is real business logic, several entry points to the same logic (HTTP, a queue consumer, a CLI), or tests that need to run without infrastructure. It is overkill when:

- The endpoint is a thin pass-through (read a row, return it) with no rules. A service that only forwards calls adds a file and no value.
- The app is small, internal, or a prototype.
- The ORM is a deliberate permanent choice and its types are acceptable everywhere.

A pragmatic rule: **start with the controller calling a service**, and add the repository interface when you want to test the service without a database or isolate storage. Do not create empty layers in advance. Merge them again if one carries no weight.

## Common anti-patterns

- **Fat controllers:** business rules, SQL, and mapping inside route handlers.
- **Anemic services that only forward calls** to repositories, with rules scattered in controllers.
- **God services** with dozens of methods covering unrelated features. Split by feature or use case.
- **Leaking `req`/`res`** into services, or HTTP status codes into services and repositories.
- **Leaking ORM entities** through service and controller layers into responses ([DTO pattern](../16-type-safe-apis/02-dto-pattern.md)).
- **Repositories containing business logic** or calling other services.
- **Circular dependencies** between services (A calls B, B calls A). Extract shared logic or introduce an interface.
- **Cross-layer shortcuts,** such as a controller calling a repository directly "just this once".
- **Hidden global singletons** (imported database clients) instead of injected dependencies.

## Common mistakes

- Putting validation only in the service and letting malformed input reach it, or only in the controller and trusting direct callers.
- Throwing HTTP-flavored errors (`HttpError(404)`) from services instead of domain errors.
- Opening a transaction in the controller.
- Passing the full authenticated `req.user` object everywhere instead of the few fields needed.
- Splitting code into layers by technical type with one huge folder per layer.
- Writing a repository interface that mirrors the ORM's API instead of domain queries.
- Mocking repositories with `jest.fn()` everywhere, instead of in-memory fakes that behave like storage.

## Debugging

- To find where a bug lives, test each layer separately: call the service directly with a fake repository, then the repository against the database, then the route with HTTP.
- If a business rule is not enforced, check whether it exists in the service and whether any path bypasses the service.
- If a response leaks a field, find which layer passed an entity instead of a DTO.
- If a transaction is not rolling back, check that every repository call inside it uses the transaction's connection.
- Log at layer boundaries with the request id to follow a request through controller, service, and repository.

## Quick summary

- Split concerns into **controller** (HTTP), **service** (business rules and transactions), and **repository** (persistence). Dependencies point downward through interfaces.
- Controllers validate input and map output. Services take plain values, return domain objects, and throw domain errors. Repositories translate between rows and domain objects.
- Each boundary has its own type: DTOs outside, domain types inside, rows only within repositories. Never pass `req`/`res` into services.
- Services own transaction boundaries. A central error handler maps domain errors to HTTP responses.
- Organize by feature, test each layer at the right level, and add layers only when they carry weight.

**Next:** [Databases](./05-databases.md)
