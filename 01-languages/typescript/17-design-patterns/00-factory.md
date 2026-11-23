# Factory and Abstract Factory Patterns in TypeScript

> Implementation of the Factory Method and Abstract Factory patterns in TypeScript using interfaces, discriminated unions, generic type constraints, and class-free functional factories.

| Metadata | Details |
| :--- | :--- |
| **Category** | Design Patterns |
| **Difficulty** | Intermediate |
| **Prerequisites** | [04-objects-and-interfaces/00-interfaces.md](../04-objects-and-interfaces/00-interfaces.md), [05-classes/00-class-basics.md](../05-classes/00-class-basics.md), [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md) |
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

The **Factory Pattern** is a creational design pattern that provides an interface for creating objects in a superclass or factory function, allowing callers or subclasses to alter the type of objects that will be created without coupling client code to concrete implementations.

In languages like Java or C++, factories traditionally rely on extensive class inheritance hierarchies (`LoggerFactory`, `ConsoleLoggerFactory`, `FileLoggerFactory`). In TypeScript, however, structural typing and first-class functions provide significantly more expressive, lightweight, and type-safe alternatives:
- **Discriminated Union Factories**: Selecting object implementations based on literal string tags with exhaustive compiler checking.
- **Generic Record Factories**: Using TypeScript key lookup types (`Record<K, Constructor>`) to ensure compile-time registry completeness.
- **Functional Closures**: Creating stateful instances without `class` or `new` keywords.

```
                    +--------------------+
                    |   Factory Method   |
                    +--------------------+
                               |
              +----------------+----------------+
              |                                 |
              v                                 v
     +-----------------+               +-----------------+
     | SQLConnection   |               | MongoConnection |
     | - connect()     |               | - connect()     |
     | - query()       |               | - query()       |
     +-----------------+               +-----------------+
```

---

## 2. Core Concepts

### 1. Simple Factory vs Factory Method vs Abstract Factory
- **Simple Factory**: A standalone function (often using a `switch` statement) that accepts a discriminator parameter and returns one of several concrete instances sharing an interface.
- **Factory Method**: An interface or abstract class declaring a creation method, deferring instantiation to implementing classes or specialized callbacks.
- **Abstract Factory**: An interface declaring a set of methods for creating a *family* of related or dependent objects (e.g., UI component families: `Button`, `Dialog`, `Scrollbar` for Windows vs macOS).

### 2. TypeScript-Idiomatic Representation
Rather than rigid OOP inheritance, TypeScript developers leverage lookup types and mapped types:
```typescript
export interface Logger {
  log(message: string): void;
}

export type LoggerFactoryMap = {
  console: () => Logger;
  file: (path: string) => Logger;
  remote: (endpoint: string, apiKey: string) => Logger;
};
```

---

## 3. How It Works Internally

When a factory method returns a union of types or an interface, TypeScript uses structural assignability to verify that every returned object conforms to the contract:

```
  createLogger('console') ---> Type Inferred: ConsoleLogger
  createLogger('file')    ---> Type Inferred: FileLogger
  createLogger(dynamic)   ---> Type Inferred: ConsoleLogger | FileLogger
```

By parameterizing the factory with a generic type `TKey extends keyof Registry`, TypeScript performs return-type indexing: `Registry[TKey]`, preserving exact return types instead of widening to a loose union or base interface.

---

## 4. Syntax & API Reference

### Generic Registry Factory Pattern
```typescript
export interface Product {
  name: string;
  price: number;
}

export type ProductConstructor<T extends Product = Product> = new (...args: any[]) => T;

export class ProductFactory {
  private static registry = new Map<string, ProductConstructor>();

  public static register<T extends Product>(type: string, ctor: ProductConstructor<T>): void {
    this.registry.set(type, ctor);
  }

  public static create(type: string, ...args: any[]): Product {
    const Ctor = this.registry.get(type);
    if (!Ctor) throw new Error(`Unknown product type: ${type}`);
    return new Ctor(...args);
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic Functional Factory with Discriminated Unions
```typescript
export type StorageType = 's3' | 'local' | 'memory';

export interface StorageProvider {
  upload(key: string, data: Buffer): Promise<string>;
  download(key: string): Promise<Buffer>;
}

export function createStorageProvider(type: 's3', bucket: string): StorageProvider;
export function createStorageProvider(type: 'local', dirPath: string): StorageProvider;
export function createStorageProvider(type: 'memory'): StorageProvider;
export function createStorageProvider(type: StorageType, ...args: any[]): StorageProvider {
  switch (type) {
    case 's3':
      return {
        upload: async (key) => `https://s3.amazonaws.com/${args[0]}/${key}`,
        download: async () => Buffer.from('s3-data')
      };
    case 'local':
      return {
        upload: async (key) => `${args[0]}/${key}`,
        download: async () => Buffer.from('local-data')
      };
    case 'memory': {
      const store = new Map<string, Buffer>();
      return {
        upload: async (key, data) => { store.set(key, data); return `mem://${key}`; },
        download: async (key) => store.get(key) ?? Buffer.alloc(0)
      };
    }
  }
}
```

### Example 2: Real-World Abstract Factory for Multi-Cloud Clients
```typescript
export interface CloudCompute {
  spawnInstance(cpu: number, ramGb: number): Promise<string>;
}

export interface CloudStorage {
  putObject(name: string, payload: Uint8Array): Promise<void>;
}

export interface CloudFactory {
  createCompute(): CloudCompute;
  createStorage(): CloudStorage;
}

export class AWSCloudFactory implements CloudFactory {
  createCompute(): CloudCompute {
    return {
      spawnInstance: async (cpu, ram) => `EC2 instance created with ${cpu} vCPU, ${ram}GB RAM`
    };
  }
  createStorage(): CloudStorage {
    return {
      putObject: async (name) => console.log(`Stored ${name} in S3`)
    };
  }
}

export class GCPCloudFactory implements CloudFactory {
  createCompute(): CloudCompute {
    return {
      spawnInstance: async (cpu, ram) => `GCE VM created with ${cpu} vCPU, ${ram}GB RAM`
    };
  }
  createStorage(): CloudStorage {
    return {
      putObject: async (name) => console.log(`Stored ${name} in GCS`)
    };
  }
}
```

### Example 3: Edge Case - Parameter-Inferred Type-Safe Factory Registry
```typescript
type FactoryMap = {
  email: { to: string; subject: string; body: string };
  sms: { phoneNumber: string; message: string };
  webhook: { url: string; payload: Record<string, unknown> };
};

export interface Notifier<TConfig> {
  notify(config: TConfig): Promise<boolean>;
}

export type NotifierRegistry = {
  [K in keyof FactoryMap]: Notifier<FactoryMap[K]>;
};

export class NotificationDispatcher {
  constructor(private registry: NotifierRegistry) {}

  public send<K extends keyof FactoryMap>(channel: K, config: FactoryMap[K]): Promise<boolean> {
    const notifier = this.registry[channel];
    return notifier.notify(config);
  }
}
```

### Example 4: Failure Case - Type Mismatch in Registry Invocations
```typescript
const registry: NotifierRegistry = {
  email: { notify: async (c) => { console.log(c.to); return true; } },
  sms: { notify: async (c) => { console.log(c.phoneNumber); return true; } },
  webhook: { notify: async (c) => { console.log(c.url); return true; } }
};

const dispatcher = new NotificationDispatcher(registry);

// @ts-expect-error TS2345: Argument of type '{ to: string; }' is not assignable to parameter of type '{ to: string; subject: string; body: string; }'. Property 'subject' is missing.
dispatcher.send('email', { to: 'admin@corp.com' });
```

### Example 5: Idiomatic Factory Using Static Class Methods
```typescript
export class HttpResponse<T> {
  private constructor(
    public readonly statusCode: number,
    public readonly data: T | null,
    public readonly error: string | null
  ) {}

  public static ok<U>(data: U): HttpResponse<U> {
    return new HttpResponse(200, data, null);
  }

  public static created<U>(data: U): HttpResponse<U> {
    return new HttpResponse(201, data, null);
  }

  public static badRequest<U = null>(error: string): HttpResponse<U> {
    return new HttpResponse(400, null, error);
  }

  public static notFound<U = null>(error: string = 'Not found'): HttpResponse<U> {
    return new HttpResponse(404, null, error);
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Over-Engineering**: Creating factory classes with abstract interfaces when a simple pure function returning an object literal would suffice.
2. **Missing Discriminated Unions**: Returning `any` or loose unions without a discriminator property strips TypeScript's ability to narrow the returned object.
3. **Private Constructor Bypass**: In TypeScript, private constructors (`private constructor()`) can still be invoked via reflection or type assertion (`new (MyClass as any)()`).
4. **Dynamic String Keys**: Calling a factory with a runtime `string` variable rather than a string literal union widens the return type to all possible products, requiring subsequent type assertions.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Giant Un-Typed Switch** | `function create(type: string): any` | Discriminated union or generic map | Loss of type safety; returns `any` |
| **Class Factory Boilerplate** | 4 abstract classes for 2 simple functions | Simple functional factory closure | Bloats bundle and hurts readability |
| **Public Constructor with Factory Method** | `class Model { constructor() {} static create() {} }` | `private constructor()` | Callers bypass the factory and instantiate invalid states |
| **Missing Exhaustiveness Check** | Missing `default: assertNever(type)` | Exhaustiveness check with `never` | Adding a new type causes runtime fallback bugs |
| **Ignoring Async Initialization** | Constructors doing I/O | Static async factory methods (`static async init()`) | Node event loop blocking or unhandled rejections |
| **Tight Coupling to Concrete Class** | Functions accepting `AWSClient` directly | Accepting generic interface `CloudStorage` | Prevents mocking and testing |

---

## 8. Performance & Type-Checker Complexity

- **Object Allocation**: Plain functional factories (`return { foo() {} }`) allocate new function references per instance. If creating millions of instances in memory, prototype-based classes (`class Foo { foo() {} }`) share method definitions on the prototype, consuming significantly less heap memory.
- **Type Resolution**: Keyed registry lookups (`Registry[K]`) resolve in $O(1)$ time in the compiler.

---

## 9. Compiler Internals

When using function overloads for factories:
```typescript
function create(kind: 'a'): TypeA;
function create(kind: 'b'): TypeB;
function create(kind: string): TypeA | TypeB;
```
The TypeScript type checker resolves calls by evaluating overload signatures sequentially in declaration order. The first signature that matches the argument types is selected. If none match, it falls back to the implementation signature or raises `TS2769: No overload matches this call`.

---

## 10. Best Practices & Design Guidelines

1. **Use Static Factory Methods for Validation**: Keep class constructors private and expose static methods (`User.create(props)`) that validate invariants before returning instances.
2. **Prefer Functional Closures for Services**: Use simple functions returning typed interfaces instead of heavy `AbstractWidgetFactory` classes.
3. **Always Assert Exhaustiveness**: In factory switch statements, check the default branch with `const _ex: never = type;`.
4. **Leverage Const Assertions for Key Maps**: Define registries with `as const` to retain literal types.

---

## 11. Real-World Case Study

### Multi-Tenant Database Connection Pool Factory
A SaaS platform dynamically creates database adapters (PostgreSQL, MySQL, SQLite) based on tenant configuration:

```typescript
export interface DbConnection {
  query<T>(sql: string, params?: unknown[]): Promise<T[]>;
  close(): Promise<void>;
}

export type DbConfig =
  | { type: 'postgres'; connectionString: string; ssl: boolean }
  | { type: 'mysql'; host: string; port: number; user: string }
  | { type: 'sqlite'; filename: string };

export class ConnectionFactory {
  public static async createConnection(config: DbConfig): Promise<DbConnection> {
    switch (config.type) {
      case 'postgres':
        // Connect to PG
        return {
          query: async (sql) => [{ id: '1', engine: 'postgres' }] as any,
          close: async () => console.log('Closed Postgres')
        };
      case 'mysql':
        // Connect to MySQL
        return {
          query: async (sql) => [{ id: '1', engine: 'mysql' }] as any,
          close: async () => console.log('Closed MySQL')
        };
      case 'sqlite':
        // Connect to SQLite
        return {
          query: async (sql) => [{ id: '1', engine: 'sqlite' }] as any,
          close: async () => console.log('Closed SQLite')
        };
      default: {
        const _unreachable: never = config;
        throw new Error(`Unsupported engine: ${JSON.stringify(_unreachable)}`);
      }
    }
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the advantage of using static factory methods over direct constructor calls?</summary>

Static factory methods can have descriptive names (e.g., `ComplexNumber.fromCartesian(x, y)` vs `ComplexNumber.fromPolar(r, theta)`), can return existing cached instances (Flyweight/Singleton pattern), can return subtypes or interfaces, and can perform asynchronous initialization before instance creation.
</details>

<details>
<summary>2. How do you implement an async factory method in TypeScript?</summary>

Because constructors cannot be `async`, mark the constructor as `private` and provide a `public static async create()` method:
```typescript
export class Database {
  private constructor(private client: unknown) {}
  public static async connect(uri: string): Promise<Database> {
    const client = await rawConnect(uri);
    return new Database(client);
  }
}
```
</details>

<details>
<summary>3. What is the Abstract Factory pattern and how does it differ from the Factory Method?</summary>

Factory Method defines an interface for creating a single product, leaving the choice of concrete class to subclasses or implementations. Abstract Factory defines an interface for creating entire families of related products (e.g., Mac Button + Mac Checkbox vs Windows Button + Windows Checkbox) without specifying their concrete classes.
</details>

<details>
<summary>4. How does TypeScript's structural typing impact the Factory pattern?</summary>

In nominal languages (Java/C#), two objects with identical properties are not interchangeable unless they explicitly inherit from the same class or interface. In TypeScript, any object that satisfies the shape of an interface is assignable to it. This allows factories to return plain object literals or anonymous closures without requiring formal class hierarchies.
</details>

<details>
<summary>5. How can you ensure at compile-time that a factory handles every case in an enum or union?</summary>

By routing the `default` case in a switch statement to a function or variable typed as `never`:
```typescript
default: {
  const _unhandled: never = kind;
  throw new Error(`Unhandled kind: ${_unhandled}`);
}
```
If a new member is added to the union, TypeScript emits `TS2322`.
</details>

<details>
<summary>6. What are the memory implications of functional factories versus class factories?</summary>

Functional factories create new function instances in memory for every method on every object created. Class instances inherit methods from the class prototype, sharing a single set of function definitions across all instances, which is much more memory efficient for large collections.
</details>

<details>
<summary>7. How do you type a factory that accepts a constructor function?</summary>

Use the `new (...args: any[]) => T` construct signature:
```typescript
function factory<T>(ctor: new (...args: any[]) => T, ...args: any[]): T {
  return new ctor(...args);
}
```
</details>

<details>
<summary>8. Can factories return different types based on conditional generic parameters?</summary>

Yes, using conditional types:
```typescript
type ResultType<T extends boolean> = T extends true ? SuccessDTO : FailureDTO;
function build<T extends boolean>(flag: T): ResultType<T> { ... }
```
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Simple String-Based Factory</summary>

**Problem:** Create a factory function that takes `'json' | 'csv' | 'xml'` and returns a serializer object with a `serialize(data: unknown): string` method.

**Solution:**
```typescript
export function createSerializer(format: 'json' | 'csv' | 'xml') {
  return {
    serialize: (data: unknown): string => {
      if (format === 'json') return JSON.stringify(data);
      if (format === 'csv') return Object.values(data as object).join(',');
      return `<data>${JSON.stringify(data)}</data>`;
    }
  };
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Class with Private Constructor Factory</summary>

**Problem:** Create a `User` class with a private constructor that can only be created via `User.create(email, age)`, throwing an error if `age < 18`.

**Solution:**
```typescript
export class User {
  private constructor(public readonly email: string, public readonly age: number) {}

  public static create(email: string, age: number): User {
    if (age < 18) throw new Error('User must be at least 18');
    return new User(email, age);
  }
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Generic Registry Factory</summary>

**Problem:** Implement a generic factory class `RegistryFactory<TInterface>` where concrete constructors can be registered by name and instantiated with arbitrary arguments.

**Solution:**
```typescript
export class RegistryFactory<T> {
  private registry = new Map<string, new (...args: any[]) => T>();

  public register(name: string, ctor: new (...args: any[]) => T): void {
    this.registry.set(name, ctor);
  }

  public create(name: string, ...args: any[]): T {
    const Ctor = this.registry.get(name);
    if (!Ctor) throw new Error(`Unknown registration: ${name}`);
    return new Ctor(...args);
  }
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Abstract Theme Factory</summary>

**Problem:** Design an abstract factory interface `ThemeFactory` that produces `Header` and `Footer` components, with concrete implementations for `DarkThemeFactory` and `LightThemeFactory`.

**Solution:**
```typescript
export interface UIComponent { render(): string; }
export interface ThemeFactory {
  createHeader(): UIComponent;
  createFooter(): UIComponent;
}

export class DarkThemeFactory implements ThemeFactory {
  createHeader(): UIComponent { return { render: () => '<header class="dark"/>' }; }
  createFooter(): UIComponent { return { render: () => '<footer class="dark"/>' }; }
}

export class LightThemeFactory implements ThemeFactory {
  createHeader(): UIComponent { return { render: () => '<header class="light"/>' }; }
  createFooter(): UIComponent { return { render: () => '<footer class="light"/>' }; }
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Type-Safe Parameterized Factory Map</summary>

**Problem:** Write a strongly typed factory registry where registering a factory associates a specific configuration payload type with a key, ensuring `create(key, config)` receives exact config types.

**Solution:**
```typescript
export class TypedFactory<TMap extends Record<string, any>, TProduct> {
  private factories = new Map<keyof TMap, (config: any) => TProduct>();

  public register<K extends keyof TMap>(key: K, fn: (config: TMap[K]) => TProduct): void {
    this.factories.set(key, fn);
  }

  public create<K extends keyof TMap>(key: K, config: TMap[K]): TProduct {
    const fn = this.factories.get(key);
    if (!fn) throw new Error(`No factory for ${String(key)}`);
    return fn(config);
  }
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Self-Registering Decorator Factory</summary>

**Problem:** Create a class decorator `@RegisterPlugin(name: string)` that automatically registers annotated plugin classes into a centralized global factory.

**Solution:**
```typescript
export interface Plugin { execute(): void; }
const pluginRegistry = new Map<string, new () => Plugin>();

export function RegisterPlugin(name: string) {
  return function <T extends new () => Plugin>(constructor: T) {
    pluginRegistry.set(name, constructor);
  };
}

export function createPlugin(name: string): Plugin {
  const Ctor = pluginRegistry.get(name);
  if (!Ctor) throw new Error(`Plugin ${name} not found`);
  return new Ctor();
}
```
</details>
