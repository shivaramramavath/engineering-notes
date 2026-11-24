# InversifyJS in Node.js — Basic to Advanced

A learning guide for InversifyJS, the Inversion of Control (IoC) / Dependency Injection container for TypeScript and Node.js. Intended for the nodejs-mastery notes.

> **Version note:** the official docs currently show the 8.x line, with the same core getting-started API as 7.x. Container module and some lifecycle APIs changed between 5/6 and 7+, so older blog posts may not match. Check the official docs at inversify.io for the version you install.

---

## Level 0 — Concepts

- **Dependency Injection (DI):** a class receives its dependencies from outside instead of creating them with `new`.
- **Inversion of Control (IoC):** a container, not your class, decides which implementation is created and when.
- **Container:** the registry that maps a *service identifier* (class, string or symbol) to an implementation.
- **Binding:** one mapping inside the container, for example `Weapon -> Katana`.
- **Why use it:** loose coupling, easy testing (swap real services for mocks), one place that wires the whole app, controlled lifetimes (singleton vs new instance).

| Term | Meaning |
| --- | --- |
| `Container` | Holds bindings and resolves dependencies |
| `@injectable()` | Marks a class as resolvable by the container |
| `@inject(id)` | Tells the container what to pass into a constructor parameter |
| Service identifier | Class, string or `Symbol` used as the binding key |
| Scope | Lifetime of a resolved instance (singleton, transient, request) |
| Composition root | The single file where all bindings are declared |

Without DI:

```ts
class UserService {
  private repo = new UserRepository(); // hard-wired, hard to test
}
```

With DI:

```ts
class UserService {
  constructor(private repo: UserRepository) {} // supplied from outside
}
```

---

## Level 1 — Setup and first container

### 1.1 Install

```bash
npm install inversify reflect-metadata
npm install -D typescript tsx @types/node
```

### 1.2 tsconfig.json

Decorators need these options:

```json
{
  'compilerOptions': {
    'target': 'ES2022',
    'module': 'commonjs',
    'strict': true,
    'experimentalDecorators': true,
    'emitDecoratorMetadata': true,
    'esModuleInterop': true
  }
}
```

(Use normal double quotes in your real JSON file.)

### 1.3 Import reflect-metadata once, first

```ts
// src/index.ts — the very first line of the entry file
import 'reflect-metadata';
```

If it is missing or imported late, you get errors about missing metadata.

### 1.4 Hello container

```ts
import 'reflect-metadata';
import { Container, inject, injectable } from 'inversify';

@injectable()
class Katana {
  public readonly damage: number = 10;
}

@injectable()
class Ninja {
  constructor(
    @inject(Katana)
    public readonly katana: Katana,
  ) {}
}

const container: Container = new Container();

container.bind(Ninja).toSelf();
container.bind(Katana).toSelf();

const ninja: Ninja = container.get(Ninja);
console.log(ninja.katana.damage); // 10
```

What happens:

1. `@injectable()` allows `Katana` and `Ninja` to be container bindings.
2. `@inject(Katana)` records that the first constructor argument of `Ninja` is a `Katana`.
3. `bind(...).toSelf()` registers the class as its own implementation.
4. `container.get(Ninja)` builds `Katana` first, then `Ninja`, and returns it.

**Checkpoint:** run it with `npx tsx src/index.ts` and see `10`.

---

## Level 2 — Programming to interfaces

Interfaces do not exist at runtime, so you need a runtime key. Use a `Symbol`.

```ts
// src/types.ts
export const TYPES = {
  UserRepository: Symbol.for('UserRepository'),
  UserService: Symbol.for('UserService'),
  Logger: Symbol.for('Logger'),
};
```

```ts
// src/user/user.repository.ts
export interface IUserRepository {
  findById(id: string): Promise<User | null>;
}
```

```ts
// src/user/user.repository.impl.ts
import { injectable } from 'inversify';

@injectable()
export class InMemoryUserRepository implements IUserRepository {
  private users = new Map<string, User>();
  async findById(id: string) {
    return this.users.get(id) ?? null;
  }
}
```

```ts
// src/user/user.service.ts
import { inject, injectable } from 'inversify';
import { TYPES } from '../types';

@injectable()
export class UserService {
  constructor(
    @inject(TYPES.UserRepository) private readonly repo: IUserRepository,
  ) {}

  getUser(id: string) {
    return this.repo.findById(id);
  }
}
```

```ts
// src/container.ts  (composition root)
import { Container } from 'inversify';

const container = new Container();
container.bind<IUserRepository>(TYPES.UserRepository).to(InMemoryUserRepository);
container.bind<UserService>(TYPES.UserService).to(UserService);

export { container };
```

Now swapping `InMemoryUserRepository` for a Prisma or Mongo repository is a one-line change in the container; `UserService` never changes.

---

## Level 3 — Binding types

| Binding | Use it when |
| --- | --- |
| `.to(Class)` | Map an identifier to a class |
| `.toSelf()` | The identifier is the class itself |
| `.toConstantValue(value)` | Config objects, already-built clients, mocks in tests |
| `.toDynamicValue(ctx => ...)` | Build the value with custom logic (can read other bindings) |
| `.toFactory(...)` | Inject a function that creates instances on demand |
| `.toService(otherId)` | Make one identifier an alias of another binding |

```ts
// constant value: config
container.bind<AppConfig>(TYPES.Config).toConstantValue({
  port: Number(process.env.PORT ?? 3000),
  jwtSecret: process.env.JWT_SECRET!,
});

// dynamic value: built from other bindings
container.bind<Logger>(TYPES.Logger).toDynamicValue((context) => {
  const config = context.get<AppConfig>(TYPES.Config);
  return new ConsoleLogger(config.port === 3000 ? 'debug' : 'info');
});
```

The exact shape of the `context` argument and factory helpers can differ by major version; confirm in the docs for your version.

---

## Level 4 — Scopes (lifetime)

| Scope | Behavior |
| --- | --- |
| Transient (default) | A new instance every time it is resolved |
| Singleton | One instance for the container's lifetime |
| Request | One instance per resolution tree (per `get` call) |

```ts
container.bind(TYPES.UserService).to(UserService).inSingletonScope();
container.bind(TYPES.Logger).to(ConsoleLogger).inTransientScope();
container.bind(TYPES.Cache).to(MemoryCache).inRequestScope();
```

Set a container-wide default if you want:

```ts
const container = new Container({ defaultScope: 'Singleton' });
```

Rules of thumb:

- Stateless services, repositories, loggers, DB clients: **singleton**.
- Anything holding per-request data (current user, request id): **do not make it a singleton**. A singleton that captures request data leaks it between users.
- A singleton must not depend on a shorter-lived (transient or request) dependency that you expect to refresh.

---

## Level 5 — Intermediate injection features

### 5.1 Multiple implementations

```ts
container.bind<Notifier>(TYPES.Notifier).to(EmailNotifier);
container.bind<Notifier>(TYPES.Notifier).to(SmsNotifier);

@injectable()
class AlertService {
  constructor(@multiInject(TYPES.Notifier) private notifiers: Notifier[]) {}
  async send(msg: string) {
    await Promise.all(this.notifiers.map((n) => n.notify(msg)));
  }
}
```

### 5.2 Named and tagged bindings

Choose between implementations of the same identifier:

```ts
container.bind<Weapon>(TYPES.Weapon).to(Katana).whenNamed('melee');
container.bind<Weapon>(TYPES.Weapon).to(Shuriken).whenNamed('throwable');

@injectable()
class Ninja {
  constructor(
    @inject(TYPES.Weapon) @named('melee') private melee: Weapon,
    @inject(TYPES.Weapon) @named('throwable') private ranged: Weapon,
  ) {}
}
```

`@tagged('key', value)` with `.whenTagged('key', value)` works the same way for key-value metadata.

### 5.3 Optional dependencies

```ts
constructor(@inject(TYPES.Logger) @optional() private logger?: Logger) {}
```

### 5.4 Property injection

Possible, but prefer **constructor injection**: dependencies are explicit, required and visible, and objects are never half-built.

### 5.5 Lifecycle hooks

```ts
@injectable()
class Database {
  @postConstruct()
  init() {
    // runs after the instance is created and injected
  }

  @preDestroy()
  close() {
    // cleanup when a singleton binding is unbound/disposed
  }
}
```

For async setup (connect to DB), prefer an explicit `await db.connect()` at startup or an async-aware binding with `container.getAsync(...)`. Check which hooks your version supports.

---

## Level 6 — Organizing with container modules

Split bindings by feature instead of one giant file.

```ts
// src/user/user.module.ts
import { ContainerModule } from 'inversify';

export const userModule = new ContainerModule(({ bind }) => {
  bind<IUserRepository>(TYPES.UserRepository).to(PrismaUserRepository).inSingletonScope();
  bind<UserService>(TYPES.UserService).to(UserService).inSingletonScope();
});
```

```ts
// src/container.ts
const container = new Container();
await container.load(userModule, authModule, infraModule);
```

The `ContainerModule` callback signature (destructured options object vs positional arguments) and whether `load` is async changed across versions. Confirm in the docs for your version.

Suggested structure:

```text
src/
├── index.ts              # import 'reflect-metadata' first, start server
├── container.ts          # composition root
├── types.ts              # symbols
├── config/
├── infra/
│   ├── database.module.ts
│   └── logger.module.ts
└── modules/
    ├── user/
    │   ├── user.controller.ts
    │   ├── user.service.ts
    │   ├── user.repository.ts
    │   └── user.module.ts
    └── auth/
```

---

## Level 7 — Using it with Express

The simplest, framework-neutral approach: build the object graph at startup and pass resolved services into routes.

```ts
// src/index.ts
import 'reflect-metadata';
import express from 'express';
import { container } from './container';
import { TYPES } from './types';
import { UserController } from './modules/user/user.controller';

const app = express();
app.use(express.json());

const userController = container.get<UserController>(TYPES.UserController);
app.get('/users/:id', (req, res, next) => userController.getOne(req, res).catch(next));

app.listen(3000);
```

```ts
// src/modules/user/user.controller.ts
@injectable()
export class UserController {
  constructor(@inject(TYPES.UserService) private service: UserService) {}

  async getOne(req: Request, res: Response) {
    const user = await this.service.getUser(req.params.id);
    if (!user) return res.status(404).json({ message: 'Not found' });
    return res.json(user);
  }
}
```

Important: resolve services once at startup (or per request only when you truly need request-scoped data). Do not call `container.get` deep inside business code; that is the *service locator* anti-pattern and hides dependencies.

There are also decorator-based controller helpers in the Inversify ecosystem (for example `inversify-express-utils`). Check that the package supports the Inversify version you installed before adopting it.

---

## Level 8 — Testing

DI makes unit tests simple because you construct the class directly with fakes:

```ts
const fakeRepo: IUserRepository = {
  findById: async () => ({ id: '1', name: 'Asha' }),
};
const service = new UserService(fakeRepo);
expect(await service.getUser('1')).toEqual({ id: '1', name: 'Asha' });
```

For integration tests, build a **test container** with the same modules but override the infrastructure:

```ts
function buildTestContainer() {
  const c = new Container();
  c.bind<IUserRepository>(TYPES.UserRepository).toConstantValue(fakeRepo);
  c.bind<UserService>(TYPES.UserService).to(UserService);
  return c;
}
```

Creating a fresh container per test file keeps tests isolated and avoids relying on rebind or unbind semantics, which differ across versions.

---

## Level 9 — Advanced practices

1. **One composition root.** Only `container.ts` and module files know about the container. Business classes only know `@inject`.
2. **Symbols over strings.** Avoids typos and collisions; keep them in one `types.ts`.
3. **Ports and adapters.** Services depend on interfaces (ports); infrastructure binds adapters (Prisma, Redis, SMTP).
4. **Config as a binding.** Validate environment variables once at startup, bind the typed config object, inject it everywhere.
5. **Graceful shutdown.** On `SIGTERM`, close singletons (DB, queues) explicitly in a shutdown routine.
6. **Avoid circular dependencies.** If A needs B and B needs A, extract a third service or use events. Circulars are a design smell.
7. **Keep constructors light.** No I/O in constructors; do async setup in an explicit init step.
8. **Per-request data.** Pass it as method arguments, or create a request-scoped child container deliberately; do not stash it in singletons.

---

## Level 10 — Common errors and fixes

| Error / symptom | Likely cause |
| --- | --- |
| `Reflect.getMetadata is not a function` | `reflect-metadata` not imported first |
| Missing `@injectable` / no metadata for class | Decorator missing, or `experimentalDecorators` / `emitDecoratorMetadata` disabled |
| `No bindings found for service` | Forgot `bind`, or symbol mismatch (`Symbol('x')` vs `Symbol.for('x')`) |
| Ambiguous match found | Two bindings for one identifier without `whenNamed` / `whenTagged` / `multiInject` |
| Circular dependency error | A and B inject each other; restructure |
| Data leaking between users | Request data stored on a singleton |
| Interface injected as undefined | Used the interface as a key; use a symbol instead |
| Decorators ignored under some build tools | Compiler (esbuild/swc) not emitting decorator metadata; configure it or use `tsc` / `tsx` |

---

## Inversify vs alternatives

| Option | Notes |
| --- | --- |
| InversifyJS | Explicit, framework-agnostic, strong TypeScript focus; more setup |
| NestJS DI | Built into the framework; modules and providers; less manual wiring |
| tsyringe / TypeDI | Lighter decorator-based containers |
| Manual DI (no library) | Just construct objects in one file; perfectly valid for small apps |

If you are already using NestJS, you do not need Inversify. Use Inversify with plain Express, Fastify or CLI/worker apps that need a clean architecture.

---

## Cheat sheet

```ts
import 'reflect-metadata';
import { Container, injectable, inject } from 'inversify';

@injectable() class Repo {}
@injectable() class Service { constructor(@inject(Repo) private repo: Repo) {} }

const c = new Container();
c.bind(Repo).toSelf().inSingletonScope();
c.bind(Service).toSelf();

const s = c.get(Service);
```

Bindings: `.to()`, `.toSelf()`, `.toConstantValue()`, `.toDynamicValue()`, `.toFactory()`, `.toService()`. Scopes: `.inSingletonScope()`, `.inTransientScope()`, `.inRequestScope()`. Selectors: `@named`, `@tagged`, `@multiInject`, `@optional`.

---

## Practice exercises

1. **Basic:** recreate Ninja and Katana with `toSelf()`.
2. **Interfaces:** build `UserService` with an `IUserRepository` and swap two implementations.
3. **Scopes:** log constructor calls and prove the difference between singleton and transient.
4. **Modules:** split bindings into `userModule` and `authModule`.
5. **Express:** wire a controller, service and repository into a small REST API.
6. **Testing:** write a unit test with a fake repo, then an integration test with a test container.
7. **Architecture:** add a config binding validated at startup and a graceful shutdown routine.

## Interview questions

- What is the difference between DI and IoC?
- Why do you need `reflect-metadata` and the two tsconfig options?
- How do you inject an interface in TypeScript if interfaces disappear at runtime?
- Singleton vs transient vs request scope: when do you use each?
- What is the service locator anti-pattern and how do you avoid it?
- How would you test a class that depends on a database?
- When would you choose Inversify over NestJS or manual DI?

## References

- InversifyJS official documentation: inversify.io (Getting started, bindings, scopes, container modules)
- reflect-metadata on npm
