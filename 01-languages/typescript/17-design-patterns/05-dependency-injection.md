# Dependency Injection and Inversion of Control in TypeScript

> Implementing Dependency Injection (DI) and Inversion of Control (IoC) in TypeScript: constructor injection, property injection, type-safe lightweight DI containers, decorators, and NestJS/Inversify patterns.

| Metadata | Details |
| :--- | :--- |
| **Category** | Design Patterns |
| **Difficulty** | Advanced |
| **Prerequisites** | [05-classes/00-class-basics.md](../05-classes/00-class-basics.md), [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md), [05-classes/02-decorators.md](../05-classes/02-decorators.md) |
| **TypeScript Baseline** | TS 4.5+, TS 5.0+ |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

**Inversion of Control (IoC)** is a design principle in which the control of object creation, configuration, and lifecycle management is transferred from the individual application components to an external framework or container. **Dependency Injection (DI)** is the primary design pattern used to implement IoC.

When a class instantiates its own dependencies directly:
```typescript
class OrderService {
  private mailer = new SendGridMailer('api-key-123'); // Hardcoded dependency!
  private db = new PostgresClient();                 // Direct instantiation!
}
```
The codebase suffers from:
1. **Severe Testability Blockers**: Unit testing `OrderService` without sending real emails or connecting to PostgreSQL requires dirty monkey-patching.
2. **Configuration Rigidity**: Changing connection strings or swapping providers requires modifying multiple classes across the codebase.
3. **Hidden Lifecycles**: Managing singleton vs transient instances across multiple services becomes unpredictable.

With Dependency Injection, dependencies are provided ("injected") to the class from the outside, typically through its constructor:
```typescript
class OrderService {
  constructor(private mailer: IMailer, private db: IDatabase) {}
}
```

```
+-----------------------------------------------------------+
|                        IoC Container                      |
|                                                           |
|  1. Registers Tokens: IMailer -> SendGridMailer           |
|                       IDatabase -> PostgresClient         |
|  2. Resolves Dependencies recursively                     |
|  3. Injects instances into OrderService constructor       |
+-----------------------------------------------------------+
                              |
                              v
             new OrderService(mailer, db)
```

---

## 2. Core Concepts

### 1. The Three Forms of Injection
1. **Constructor Injection**: Injected through constructor parameters. (The recommended standard in 95% of cases; enforces immutable references).
2. **Property / Field Injection**: Injected directly into class properties via decorators (`@Inject() private mailer: IMailer;`).
3. **Method Injection**: Injected as an argument to a specific method invocation.

### 2. Dependency Lifecycles
- **Singleton**: A single instance is created once and shared across the entire application lifetime.
- **Transient**: A brand-new instance is instantiated every time the dependency is requested.
- **Request / Scoped**: A single instance is created per incoming HTTP request and shared within that request's dependency sub-tree.

### 3. The Interface Erasure Problem
Because TypeScript interfaces are completely erased during compilation, an IoC container cannot look up an interface token like `container.get<IMailer>()` using runtime reflection alone!
Solutions:
- **String / Symbol Injection Tokens**: `const MAILER_TOKEN = Symbol('IMailer');`
- **Class Tokens (Abstract Classes)**: Using abstract classes as tokens since classes survive compilation into JavaScript functions.

---

## 3. How It Works Internally

A minimal DI container maintains a registry mapping injection tokens (symbols, strings, or constructors) to provider recipes:

```
Token Registry:
Symbol('Logger')      -> { useClass: ConsoleLogger, scope: 'singleton', instance: null }
Symbol('Config')      -> { useValue: { port: 8080 } }
Symbol('UserService') -> { useClass: UserService, dependencies: [Symbol('Logger')] }
```

When resolving `container.resolve(Symbol('UserService'))`:
1. The container inspects the dependency array of `UserService`.
2. For each dependency token, it recursively calls `resolve(token)`.
3. If singleton, it returns the cached instance; otherwise, it instantiates a new one.
4. It calls `new UserService(...resolvedDependencies)`.
5. It detects circular dependencies by tracking the current resolution call stack.

---

## 4. Syntax & API Reference

### Token-Based Type-Safe DI Container Interface
```typescript
export type InjectionToken<T> = string | symbol | (new (...args: any[]) => T);

export interface Container {
  registerValue<T>(token: InjectionToken<T>, value: T): void;
  registerClass<T>(token: InjectionToken<T>, target: new (...args: any[]) => T, deps?: InjectionToken<any>[]): void;
  resolve<T>(token: InjectionToken<T>): T;
}
```

---

## 5. Practical Examples

### Example 1: Pure Constructor Injection Without Any Container (Pure DI)
```typescript
export interface Logger { log(msg: string): void; }
export interface Database { query(sql: string): Promise<any>; }

export class ConsoleLogger implements Logger {
  log(msg: string) { console.log(`[INFO] ${msg}`); }
}

export class MockDatabase implements Database {
  async query(sql: string) { return [{ id: 1 }]; }
}

export class UserService {
  constructor(private logger: Logger, private db: Database) {}

  public async getUser(id: number) {
    this.logger.log(`Fetching user ${id}`);
    return this.db.query(`SELECT * FROM users WHERE id = ${id}`);
  }
}

// Composition Root (entrypoint) wires everything cleanly:
const logger = new ConsoleLogger();
const db = new MockDatabase();
const userService = new UserService(logger, db);
```

### Example 2: Real-World Lightweight Type-Safe IoC Container
```typescript
export class LightweightContainer {
  private bindings = new Map<any, { factory: () => any; isSingleton: boolean; instance?: any }>();

  public bindValue<T>(token: any, value: T): void {
    this.bindings.set(token, { factory: () => value, isSingleton: true, instance: value });
  }

  public bindClass<T>(
    token: any,
    ctor: new (...args: any[]) => T,
    dependencies: any[] = [],
    isSingleton: boolean = true
  ): void {
    this.bindings.set(token, {
      factory: () => {
        const resolvedDeps = dependencies.map((dep) => this.resolve(dep));
        return new ctor(...resolvedDeps);
      },
      isSingleton
    });
  }

  public resolve<T>(token: any): T {
    const binding = this.bindings.get(token);
    if (!binding) throw new Error(`No binding found for token: ${String(token)}`);

    if (binding.isSingleton) {
      if (!binding.instance) {
        binding.instance = binding.factory();
      }
      return binding.instance;
    }

    return binding.factory();
  }
}
```

### Example 3: Edge Case - Circular Dependency Detection
```typescript
export class ResilientContainer extends LightweightContainer {
  private resolutionStack = new Set<any>();

  public override resolve<T>(token: any): T {
    if (this.resolutionStack.has(token)) {
      const chain = [...this.resolutionStack, token].map(String).join(' -> ');
      throw new Error(`Circular dependency detected: ${chain}`);
    }

    this.resolutionStack.add(token);
    try {
      return super.resolve<T>(token);
    } finally {
      this.resolutionStack.delete(token);
    }
  }
}
```

### Example 4: Failure Case - Missing Dependency Token Registration
```typescript
const container = new LightweightContainer();
const AUTH_TOKEN = Symbol('AuthService');

// @ts-expect-error TS2345: Symbol cannot be cast directly if strict tokens are typed!
container.resolve(AUTH_TOKEN); // Throws runtime Error: No binding found for token: Symbol(AuthService)
```

### Example 5: Idiomatic Abstract Class Injection Token Pattern
```typescript
export abstract class PaymentGatewayPort {
  abstract process(amount: number): Promise<boolean>;
}

export class StripeGatewayService extends PaymentGatewayPort {
  async process(amount: number): Promise<boolean> {
    console.log(`Charged $${amount} via Stripe`);
    return true;
  }
}

export class CheckoutCoordinator {
  constructor(private gateway: PaymentGatewayPort) {}
}

const container = new LightweightContainer();
container.bindClass(PaymentGatewayPort, StripeGatewayService);
const checkout = new CheckoutCoordinator(container.resolve(PaymentGatewayPort));
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Interface Erasure**: Trying to use `container.resolve<IMyInterface>()` without passing a runtime token fails because TypeScript types disappear at runtime. Always provide a runtime token (`Symbol`, string, or class).
2. **Memory Leaks with Request-Scoped Singletons**: Storing request-scoped instances in a global container without explicitly pruning or scoping them causes memory leaks in Node.js HTTP servers. Always use child containers that get garbage collected when the request completes.
3. **Circular Dependencies**: When Service A depends on Service B, and Service B depends on Service A, recursive resolution causes a stack overflow (`RangeError: Maximum call stack size exceeded`). Break circularity with the Mediator pattern or lazy getters (`get serviceB() { return this.container.resolve(...) }`).
4. **Decorator Metadata Footprint**: Relying on `reflect-metadata` requires setting `"emitDecoratorMetadata": true`, which increases bundle size and is incompatible with fast bundlers like esbuild or swc without plugins.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Service Locator Anti-Pattern** | Passing `container` into class methods | Inject specific dependencies | Hides dependencies; breaks mockability |
| **Using `new` inside Services** | `this.repo = new SqlRepo()` | Injected via constructor | Tight coupling; impossible to unit test |
| **Interface Token at Runtime** | `container.get(IMailer)` (Syntax error) | `container.get(MAILER_TOKEN)` | Syntax error; interfaces do not exist at runtime |
| **Untracked Circular Dependencies** | Service A -> B -> A without detection | Resolution cycle guard in container | `RangeError: Maximum call stack size exceeded` |
| **Global Mutation of Tokens** | Overwriting container bindings midway | Immutably sealed container root | Unpredictable flaky race conditions |
| **Mixing DI with Static Singletons** | `Database.getInstance()` | Injected singleton via container | Hidden global state and test contamination |

---

## 8. Performance & Type-Checker Complexity

- **Resolution Latency**: Resolving an object with a dependency tree of depth 5 using a `Map` lookup takes ~1.5 microseconds in V8, which is negligible compared to any I/O operation.
- **Type Checking**: Token-based lookup functions using mapped types `resolve<T>(token: Token<T>): T` check instantly in TypeScript with zero compiler overhead.

---

## 9. Compiler Internals

When `"emitDecoratorMetadata": true` is enabled in `tsconfig.json`:
```typescript
class UserService {
  constructor(logger: LoggerService) {}
}
```
The TypeScript emitter attaches `__metadata("design:paramtypes", [LoggerService])` to the class constructor. This allows reflection libraries like `reflect-metadata` to inspect constructor parameter types at runtime, enabling automated decorator-based DI in frameworks like NestJS.

---

## 10. Best Practices & Design Guidelines

1. **Favor Pure DI for Small/Medium Apps**: Assemble dependencies in an explicit `compositionRoot()` function without pulling in external reflection libraries.
2. **Use Abstract Classes as Tokens**: When using classes as interfaces, you get both a compile-time type and a runtime injection token without needing extra `Symbol` identifiers.
3. **Keep Classes Ignorant of the Container**: A service should never import or reference the DI container; it should merely declare its dependencies in its constructor.
4. **Use Child Containers for HTTP Scopes**: Spawn a `requestContainer = rootContainer.createChild()` for each incoming HTTP request.

---

## 11. Real-World Case Study

### Multi-Tenant Microservice with Scoped Dependency Injection
A multi-tenant healthcare API isolates tenant database connections per request:

```typescript
export interface TenantContext { tenantId: string; }

export class TenantScopedDatabase {
  constructor(public tenantId: string) {}
  public query(sql: string) { return `Results for tenant ${this.tenantId}`; }
}

export class PatientService {
  constructor(private db: TenantScopedDatabase) {}
  public getPatientData(patientId: string) {
    return this.db.query(`SELECT * FROM patients WHERE id = '${patientId}'`);
  }
}

// Per-request scoped factory:
export function handleHttpRequest(tenantId: string, patientId: string) {
  const scopedDb = new TenantScopedDatabase(tenantId);
  const service = new PatientService(scopedDb);
  return service.getPatientData(patientId);
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between Inversion of Control (IoC) and Dependency Injection (DI)?</summary>

IoC is a broad architectural design principle where the control flow of a program is inverted (the framework controls program execution, calling user code). Dependency Injection is a specific structural design pattern that implements IoC by providing dependent objects to a class from the outside rather than allowing the class to instantiate them directly.
</details>

<details>
<summary>2. Why does the "Interface Erasure" problem affect DI containers in TypeScript, and how is it solved?</summary>

TypeScript interfaces are pure design-time constructs; the compiler strips them entirely during compilation, leaving no runtime artifacts. Because of this, an IoC container cannot look up an interface token like `container.get(IUserService)`. Solutions include using `Symbol('IUserService')`, string tokens, or using `abstract class IUserService` (which exists as a JavaScript constructor at runtime).
</details>

<details>
<summary>3. Why is the "Service Locator" considered an anti-pattern compared to Constructor Injection?</summary>

The Service Locator pattern passes the container or registry directly into classes (`container.get('Logger')`), which obscures the class's real dependencies, tightly couples the class to the container, makes unit testing harder, and converts compile-time missing dependency checks into runtime failures.
</details>

<details>
<summary>4. What are the three standard lifecycles in DI containers?</summary>

1. **Singleton**: Only one instance is created per application/container lifetime.
2. **Transient**: A fresh instance is created every time the dependency is requested.
3. **Scoped**: An instance is created once per execution scope (e.g. per HTTP request).
</details>

<details>
<summary>5. How can circular dependencies be resolved in a DI container?</summary>

By using:
1. **Lazy Injection / Proxies**: Injecting a proxy or function getter `() => serviceB` that resolves the instance only upon first method invocation.
2. **Property Injection / Lifecycle Hooks**: Instantiating both objects first, then populating references via setters or an `onInit` hook.
3. **Refactoring to a Mediator**: Extracting shared coordination logic into a third service to break the cycle.
</details>

<details>
<summary>6. How does NestJS implement Dependency Injection?</summary>

NestJS uses TypeScript decorator metadata (`emitDecoratorMetadata`), `reflect-metadata`, and custom tokens. The `@Injectable()` decorator registers classes in the NestIoC container, which reads constructor parameter metadata at startup and instantiates dependencies in topological order.
</details>

<details>
<summary>7. What is "Pure DI" and what are its advantages?</summary>

Pure DI (also called Poor Man's DI) is the practice of practicing dependency injection manually without using any DI framework or IoC container. Dependencies are instantiated and wired explicitly in a single "Composition Root" entrypoint function. Advantages include zero framework overhead, complete compile-time type safety, zero reflection magic, and instant startup speed.
</details>

<details>
<summary>8. How do you implement a child container for HTTP request scoping in TypeScript?</summary>

A child container maintains its own local bindings map while delegating unresolved lookups to the parent container. For each HTTP request, a child container is created, seeded with request-scoped tokens (e.g. current user, tenant ID, request logger), used to resolve request handlers, and discarded after the response is sent.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Pure DI Composition Root</summary>

**Problem:** Wire a `ConsoleNotifier`, an `SqlUserRepository`, and a `RegistrationService` manually in an entry function without a container.

**Solution:**
```typescript
interface Notifier { notify(msg: string): void; }
interface UserRepo { save(user: string): void; }

class ConsoleNotifier implements Notifier { notify(m: string) { console.log(m); } }
class SqlUserRepo implements UserRepo { save(u: string) { console.log(`Saved ${u}`); } }

class RegistrationService {
  constructor(private repo: UserRepo, private notifier: Notifier) {}
  register(name: string) { this.repo.save(name); this.notifier.notify(`Welcome ${name}`); }
}

export function main() {
  const service = new RegistrationService(new SqlUserRepo(), new ConsoleNotifier());
  service.register('Alice');
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Typed Symbol Token Factory</summary>

**Problem:** Create a helper `createToken<T>(description: string): InjectionToken<T>` that binds an injection token to an interface type.

**Solution:**
```typescript
export interface TypedToken<T> {
  readonly symbol: symbol;
  readonly _phantom?: T;
}

export function createToken<T>(desc: string): TypedToken<T> {
  return { symbol: Symbol(desc) };
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Type-Safe Container Token Resolution</summary>

**Problem:** Write a container class that supports registering and resolving values using `TypedToken<T>`, guaranteeing that `resolve(token)` returns type `T`.

**Solution:**
```typescript
export class SafeTokenContainer {
  private values = new Map<symbol, unknown>();

  public set<T>(token: TypedToken<T>, value: T): void {
    this.values.set(token.symbol, value);
  }

  public get<T>(token: TypedToken<T>): T {
    if (!this.values.has(token.symbol)) {
      throw new Error(`Token ${token.symbol.description} not found`);
    }
    return this.values.get(token.symbol) as T;
  }
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Hierarchical Child Container</summary>

**Problem:** Implement a `ChildContainer` that checks its own local registry for a token, falling back to the parent container if absent.

**Solution:**
```typescript
export class HierarchicalContainer {
  protected store = new Map<string, unknown>();

  constructor(private parent?: HierarchicalContainer) {}

  public register(key: string, val: unknown): void {
    this.store.set(key, val);
  }

  public resolve(key: string): unknown {
    if (this.store.has(key)) return this.store.get(key);
    if (this.parent) return this.parent.resolve(key);
    throw new Error(`Key ${key} not found`);
  }

  public createChild(): HierarchicalContainer {
    return new HierarchicalContainer(this);
  }
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Automatic Factory Dependency Resolver</summary>

**Problem:** Build an IoC container that accepts constructor definitions with an array of dependency tokens, resolving dependencies recursively and caching singletons.

**Solution:**
```typescript
export class AutoIoC {
  private singletons = new Map<any, any>();
  private definitions = new Map<any, { ctor: any; deps: any[] }>();

  public register(token: any, ctor: any, deps: any[]): void {
    this.definitions.set(token, { ctor, deps });
  }

  public resolve<T>(token: any): T {
    if (this.singletons.has(token)) return this.singletons.get(token);
    const def = this.definitions.get(token);
    if (!def) throw new Error(`Missing token ${String(token)}`);

    const args = def.deps.map((d) => this.resolve(d));
    const instance = new def.ctor(...args);
    this.singletons.set(token, instance);
    return instance;
  }
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Lazy Proxy Resolver for Circular Dependencies</summary>

**Problem:** Implement a function `createLazyProxy<T extends object>(resolver: () => T): T` that returns a Proxy wrapping an object, instantiating the real target only when a method or property is first accessed.

**Solution:**
```typescript
export function createLazyProxy<T extends object>(resolver: () => T): T {
  let instance: T | null = null;
  const getInstance = () => {
    if (!instance) instance = resolver();
    return instance;
  };

  return new Proxy({} as T, {
    get: (_, prop) => {
      const target = getInstance();
      const val = (target as any)[prop];
      return typeof val === 'function' ? val.bind(target) : val;
    },
    set: (_, prop, val) => {
      (getInstance() as any)[prop] = val;
      return true;
    }
  });
}
```
</details>
