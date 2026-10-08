# Dependency Injection

**Dependency injection** (DI) means a piece of code receives the things it depends on from the outside, instead of creating or locating them itself. A service that needs a database, a mailer, and a clock is *given* those, typically through its constructor. The effect is that dependencies become explicit, replaceable, and easy to fake in tests. DI is a design technique, not a library: you can do it with plain functions and constructors, and containers are an optional convenience.

**Prerequisites:**
- [Interfaces](../04-objects-and-interfaces/00-interfaces.md) and [classes](../05-classes/00-classes.md)
- [Repository](./04-repository.md) and [Adapter](./03-adapter.md) (the usual things you inject)
- [Type erasure and runtime](../14-type-system-internals/00-type-erasure-and-runtime.md) (explains why DI containers need runtime tokens)

---

## The problem

```ts
class UserService {
  private readonly db = new PostgresClient(process.env.DATABASE_URL!);
  private readonly mailer = new SmtpMailer("smtp.example.com");

  async register(email: string) {
    await this.db.insert("users", { email, createdAt: new Date() });
    await this.mailer.send(email, "Welcome!");
  }
}
```

`UserService` creates its own collaborators. As a result:

- **It cannot be tested** without a real database and mail server.
- **It is tied to concrete classes,** so switching a vendor means editing the service.
- **Its dependencies are hidden.** You must read the body to learn what it needs.
- **Configuration and global state** (`process.env`) are buried inside.

## The fix: receive dependencies

```ts
interface UserRepository { insert(user: NewUser): Promise<void> }
interface Mailer { send(to: string, subject: string): Promise<void> }

class UserService {
  constructor(
    private readonly users: UserRepository,
    private readonly mailer: Mailer,
    private readonly now: () => Date = () => new Date(),
  ) {}

  async register(email: string) {
    await this.users.insert({ email, createdAt: this.now() });
    await this.mailer.send(email, "Welcome!");
  }
}
```

Now the service depends on **interfaces**, says exactly what it needs in its constructor, and does not know about Postgres or SMTP. Even the clock is injected, so tests control time.

Tests use fakes:

```ts
const sent: string[] = [];
const service = new UserService(
  { insert: async () => {} },
  { send: async (to) => { sent.push(to); } },
  () => new Date("2024-01-01"),
);

await service.register("a@b.com");
expect(sent).toEqual(["a@b.com"]);
```

See [mocking](../18-testing-and-debugging/02-mocking.md).

## Where the wiring happens: the composition root

Something has to create the real objects and connect them. Do that **once, near the program's entry point**, not scattered through the code. This is the **composition root**:

```ts
// main.ts
const db = new Pool({ connectionString: config.databaseUrl });
const users = new PostgresUserRepository(db);
const mailer = new SmtpMailer(config.smtpHost);

const userService = new UserService(users, mailer);
const app = createApp({ userService });

app.listen(config.port);
```

Everything above the composition root receives its dependencies. Only the root knows concrete classes. This is **manual DI**, and for many applications it is all you need.

## Constructor injection vs alternatives

| Style | How | Notes |
|---|---|---|
| **Constructor injection** | dependencies are constructor parameters | the default: required dependencies are explicit, and an object is never half-built |
| **Function parameter** | a factory function takes dependencies and returns functions or an object | idiomatic TypeScript, no classes or `this` |
| **Method injection** | passed to the method that needs it | for dependencies that vary per call |
| **Property/setter injection** | assigned after construction | allows partly-built objects. Avoid unless a framework requires it |

### Function-style DI

```ts
interface Deps {
  users: UserRepository;
  mailer: Mailer;
  now?: () => Date;
}

function createUserService({ users, mailer, now = () => new Date() }: Deps) {
  return {
    async register(email: string) {
      await users.insert({ email, createdAt: now() });
      await mailer.send(email, "Welcome!");
    },
  };
}

type UserService = ReturnType<typeof createUserService>;
```

A closure holds the dependencies, so there is no class and no `this`. The `Deps` interface documents what the module needs, and `ReturnType` derives the service type ([function and class utilities](../07-utility-types/03-function-and-class-utilities.md)).

## Depend on abstractions you own

Inject **interfaces defined by the consumer**, expressing only what it needs, not full vendor types:

```ts
// the service needs only these two operations
interface Mailer { send(to: string, subject: string): Promise<void> }
```

An [adapter](./03-adapter.md) implements that interface around a vendor SDK. Small interfaces make fakes trivial: you implement two lines instead of mocking a whole library. This is the "dependency inversion" idea: high-level code defines the interface, and low-level code conforms to it.

## DI containers

A **container** automates the wiring: you register how to build things, and it constructs the graph and manages lifetimes. Popular TypeScript options include the DI system built into NestJS, and libraries such as tsyringe, InversifyJS, and awilix (check each project's current documentation for APIs and status).

### Why containers need runtime tokens

TypeScript interfaces are erased, so a container cannot look up "whatever implements `Mailer`" at runtime ([type erasure and runtime](../14-type-system-internals/00-type-erasure-and-runtime.md)). Containers use a **token** that exists at runtime:

```ts
// 1. a Symbol or string as the token
const MAILER = Symbol("Mailer");
container.register(MAILER, { useClass: SmtpMailer });

// 2. an abstract class as both the type and the token
abstract class Mailer {
  abstract send(to: string, subject: string): Promise<void>;
}
```

Decorator-based containers (NestJS, tsyringe, Inversify) also rely on `experimentalDecorators` and `emitDecoratorMetadata`, which makes the compiler emit constructor parameter types for decorated classes. That works for **classes**, which exist at runtime, but not for interfaces, which are why you need tokens or abstract classes. See [decorators](../05-classes/07-decorators.md) and [NestJS](../20-nodejs-backend/08-nestjs.md).

### Lifetimes

| Lifetime | Meaning | Typical use |
|---|---|---|
| **Singleton** | one instance for the whole app | database pools, config, loggers |
| **Scoped** (per request) | one instance per request or unit of work | request context, transactions, current user |
| **Transient** | a new instance each time | lightweight, stateless helpers |

Lifetime bugs are classic DI problems: a **singleton that depends on a scoped service** captures one request's instance and shares it across all requests. Keep per-request data out of singletons.

### Do you need a container?

| Manual DI is enough when | A container helps when |
|---|---|
| The graph is small to medium | Many services with deep dependency chains |
| You can wire it in one composition root | You need scopes, lifecycle hooks, or module systems |
| You want the least magic and best traceability | The framework you use (NestJS) is built around one |

Containers trade explicitness for convenience. Wiring errors move from compile time to **runtime** (a missing registration fails when resolving), so start with manual DI and add a container only if the wiring becomes a burden.

## Service locator: the anti-pattern to avoid

```ts
class UserService {
  register(email: string) {
    const mailer = container.resolve(Mailer);      // reaching out to a global container
    // ...
  }
}
```

A **service locator** looks like DI but hides dependencies: the constructor does not say what the class needs, any code can pull anything from the container, and tests must configure global state. Pass dependencies in explicitly. The container should appear only in the composition root (and framework glue).

## Designing for injection

- **Inject things that vary or that tests need to control:** I/O (database, network, filesystem), time, randomness, ids, configuration, logging.
- **Do not inject everything.** Pure helper functions and value objects do not need injection. Importing `Math.max` directly is fine.
- **Too many constructor parameters** (more than about four or five) usually means the class does too much. Split it.
- **Avoid circular dependencies.** If A needs B and B needs A, introduce an interface, an event, or a third piece that both use.
- **Keep construction free of side effects.** Connect and start things in explicit `init`/`start` steps or factories ([factory](./00-factory.md)), not in constructors.
- **Make configuration injected data,** parsed and validated once at startup ([config and environment](../20-nodejs-backend/01-config-and-environment.md)), not read from `process.env` all over.

## DI in practice

- **Express and similar:** build services in the composition root and pass them to route factories (`createApp({ userService })`), rather than importing singletons in handlers. See [Express](../20-nodejs-backend/02-express.md) and [service and repository layers](../20-nodejs-backend/04-service-and-repository-layers.md).
- **React:** context is a form of DI for components ([context](../19-react-and-frontend/03-context.md)). Provide a typed client or service through a provider and fake it in tests.
- **Tests:** fakes and hand-written stubs for injected interfaces, usually clearer than module-level mocking tricks.

## Important rules and misconceptions

- **DI is not a framework.** It is passing dependencies in. Constructors and function parameters are enough.
- **An interface alone does not give you DI.** Something must still supply the implementation from outside.
- **Using `new` is not forbidden.** It belongs in the composition root and in factories, not scattered through business code.
- **Containers do not remove coupling.** They move wiring to configuration, and failures to runtime.
- **Singletons are fine as a lifetime** when injected. A hidden global singleton imported directly is the problem.

## Common mistakes

- Creating collaborators with `new` inside classes that should receive them.
- Using a service locator and calling it DI.
- Injecting concrete classes everywhere instead of small interfaces.
- Using TypeScript `interface` as a container token and wondering why resolution fails at runtime.
- Enabling decorator-based DI without `emitDecoratorMetadata` and getting `undefined` dependencies.
- A singleton capturing a request-scoped dependency.
- Constructors with ten parameters.
- Circular dependencies solved with property injection or lazy hacks instead of fixing the design.
- Injecting trivial pure helpers.

## Debugging

- **"Cannot resolve dependency" or `undefined` dependency:** check the token, the registration, and the decorator metadata configuration.
- **Circular dependency errors:** draw the graph. Break the cycle with an interface or by extracting shared logic.
- **State leaking between requests:** find a singleton holding per-request data.
- **A test is hard to write:** list what the unit creates or reads globally (`new`, `Date.now()`, `process.env`, imported singletons). Each is something to inject.
- Print or log the composition root's wiring at startup in development to confirm what was built.

## Quick summary

- DI means receiving dependencies instead of creating them, which makes them explicit, swappable, and testable.
- Use constructor or function-parameter injection, depend on small interfaces you define, and wire everything in one composition root.
- Manual DI is often enough. Containers add lifetimes and automation but need runtime tokens (interfaces are erased) and move errors to runtime.
- Avoid the service locator, constructors with side effects, and singletons capturing scoped data.
- Inject I/O, time, randomness, and configuration, not every function.

**Next:** [State machines](./06-state-machines.md)
