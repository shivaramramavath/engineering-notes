# Dependency Injection and Inversion of Control in JavaScript

> Inversion of Control (IoC), constructor injection, building a lightweight IoC container, lifecycle management, and testable architectures.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Classes](file:///e:/Learning/engineering-notes/01-languages/javascript/05_this-and-oop/05_classes.md), [Factory Pattern](file:///e:/Learning/engineering-notes/01-languages/javascript/20_design-patterns/02_factory-pattern.md) |
| **Time to read** | 24 minutes |
| **Status** | Complete |
| **Standard / Runtime** | ECMAScript 2015+, Node.js & Evergreen Browsers |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In tightly coupled systems, classes and services directly instantiate their own dependencies using hardcoded `new` statements or global singleton imports:
```javascript
// Tightly Coupled (Anti-Pattern)
import { Database } from './db.js';
import { Mailer } from './mailer.js';

class UserService {
  constructor() {
    this.db = new Database(); // Direct coupling!
    this.mailer = new Mailer(); // Cannot be mocked in tests!
  }
}
```
This architecture creates severe maintenance and testing hurdles: you cannot unit test `UserService` in isolation without connecting to a live database or triggering real outgoing emails.

**Dependency Injection (DI)** is a specific implementation of the broader **Inversion of Control (IoC)** principle. Instead of an object constructing its dependencies, dependencies are provided ("injected") into the object from the outside (typically via constructor parameters). This completely decouples business logic from concrete infrastructural implementations, making systems modular, configurable, and trivial to unit test.

---

## 2. Core Concepts

### The Three Forms of Injection
1. **Constructor Injection**: Dependencies are passed as parameters to the class constructor. This is the gold standard: dependencies are explicit, immutable, and guaranteed to be present before any method executes.
2. **Property / Setter Injection**: Dependencies are assigned to public properties or via setter methods after instantiation. Useful for optional dependencies or circular references.
3. **Method / Parameter Injection**: Dependencies are passed directly into the specific method that requires them, keeping the rest of the class decoupled.

### IoC Containers and Lifecycles
In large enterprise applications, manually wiring dozens of nested dependencies together (known as *Pure DI* or *Poor Man's DI*) becomes tedious. An **IoC Container** automates this by managing a registry of services and automatically resolving dependency graphs.

Common service lifecycles managed by IoC containers include:
- **Singleton**: Exactly one instance is created and shared across the entire application lifecycle.
- **Transient**: A fresh instance is instantiated every time the dependency is requested.
- **Scoped**: A single instance is shared within the boundaries of a specific context (such as an incoming HTTP request).

---

## 3. How It Works Internally

```
WITHOUT DEPENDENCY INJECTION (Hardcoded Coupling):
[ UserService ] ---> calls 'new Database()' ---> [ Concrete PostgreSQL ]
                ---> calls 'new Mailer()'   ---> [ Concrete SendGrid ]
(Impossible to swap drivers or mock in unit tests)

WITH DEPENDENCY INJECTION (Inversion of Control):
[ Client Code / IoC Container ]
       |
       | 1. Instantiates mock or real dependencies:
       |    const db = new MockDatabase();
       |    const mailer = new InMemoryMailer();
       |
       | 2. Injects dependencies into constructor:
       v
[ UserService(db, mailer) ]
       |
       v
Interacts ONLY with the abstract interfaces (IDatabase, IMailer)
```

---

## 4. Syntax & API Reference

### Pure Constructor Injection (No Frameworks Needed)
```javascript
export class UserService {
  #db;
  #mailer;

  constructor({ database, mailer }) {
    if (!database) throw new Error('database is required');
    if (!mailer) throw new Error('mailer is required');
    this.#db = database;
    this.#mailer = mailer;
  }

  async registerUser(email, password) {
    const existing = await this.#db.findUser(email);
    if (existing) throw new Error('User already exists');

    const user = await this.#db.createUser({ email, password });
    await this.#mailer.sendWelcomeEmail(email);
    return user;
  }
}
```

---

## 5. Practical Examples

### Example 1: Building a Lightweight IoC Container
```javascript
export class Container {
  #services = new Map();
  #singletons = new Map();

  // Register a factory function with a lifecycle
  register(name, factory, { singleton = false } = {}) {
    this.#services.set(name, { factory, singleton });
  }

  // Resolve service and its recursive dependencies
  resolve(name) {
    const serviceConfig = this.#services.get(name);
    if (!serviceConfig) {
      throw new Error(`Service "${name}" is not registered in the container`);
    }

    if (serviceConfig.singleton) {
      if (!this.#singletons.has(name)) {
        // Evaluate factory and cache singleton instance
        this.#singletons.set(name, serviceConfig.factory(this));
      }
      return this.#singletons.get(name);
    }

    // Transient: return fresh instance on every resolve
    return serviceConfig.factory(this);
  }
}

// Container Wiring Setup
const container = new Container();

container.register('config', () => ({ dbUrl: 'postgres://localhost:5432/db' }), { singleton: true });
container.register('db', (c) => new Database(c.resolve('config')), { singleton: true });
container.register('mailer', () => new Mailer(), { singleton: true });
container.register('userService', (c) => new UserService({
  database: c.resolve('db'),
  mailer: c.resolve('mailer'),
}));

// Resolve fully wired application service:
const userService = container.resolve('userService');
```

### Example 2: Unit Testing Without Mocks via Pure DI
```javascript
import { UserService } from './UserService.js';

// In test file (e.g. user.test.js)
async function testUserRegistration() {
  // Create lightweight in-memory test fakes
  const fakeDb = {
    users: [],
    findUser: async (email) => fakeDb.users.find(u => u.email === email),
    createUser: async (user) => { fakeDb.users.push(user); return user; },
  };

  const fakeMailer = {
    sentEmails: [],
    sendWelcomeEmail: async (email) => { fakeMailer.sentEmails.push(email); },
  };

  // Inject fakes cleanly into service
  const service = new UserService({ database: fakeDb, mailer: fakeMailer });
  const user = await service.registerUser('ada@example.com', 'secret');

  console.assert(user.email === 'ada@example.com', 'User email mismatch');
  console.assert(fakeMailer.sentEmails.length === 1, 'Welcome email not sent');
  console.log('Test passed cleanly without mocking libraries!');
}

testUserRegistration();
```

### Example 3: Scoped DI Container for Express/HTTP Requests
```javascript
// Creating an isolated request-scoped container for multi-tenant isolation
export function createRequestScope(parentContainer, req) {
  const scopedInstances = new Map();

  return {
    resolve(name) {
      if (name === 'currentUser') {
        return req.user; // Request-specific dependency
      }
      if (name === 'correlationId') {
        return req.headers['x-correlation-id'] || crypto.randomUUID();
      }

      // Delegate other services to parent container
      return parentContainer.resolve(name);
    },
  };
}

// In HTTP Middleware:
app.use((req, res, next) => {
  req.scope = createRequestScope(globalContainer, req);
  next();
});
```

### Example 4: Functional Dependency Injection (Currying)
```javascript
// In functional programming, DI is achieved naturally via higher-order functions
const createUserRegistrationHandler = ({ db, mailer }) => async (email, password) => {
  const user = await db.insert({ email, password });
  await mailer.send(email, 'Welcome!');
  return user;
};

// Production composition
const registerUser = createUserRegistrationHandler({ db: productionDb, mailer: productionMailer });

// Test composition
const testRegisterUser = createUserRegistrationHandler({ db: mockDb, mailer: mockMailer });
```

### Example 5: Resolving Circular Dependencies with Lazy Getters
```javascript
// Handling unavoidable circular dependencies using property getters
export class ServiceA {
  constructor(getServiceB) {
    this._getServiceB = getServiceB;
  }
  doWork() {
    const serviceB = this._getServiceB();
    console.log('ServiceA calling ServiceB:', serviceB.id);
  }
}

export class ServiceB {
  constructor(serviceA) {
    this.serviceA = serviceA;
    this.id = 'B-100';
  }
}

// Container wires circular reference safely via closure getter:
let serviceA;
let serviceB;
serviceA = new ServiceA(() => serviceB);
serviceB = new ServiceB(serviceA);
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Service Locator Anti-Pattern**: If you pass the `container` instance directly into `UserService` (e.g. `this.db = container.resolve('db')`), you have replaced DI with the **Service Locator** anti-pattern. This hides dependencies, makes unit testing harder, and couples the class to the container. Always inject the resolved dependency, not the container!
2. **Circular Dependency Deadlocks**: If Service A requires Service B, and Service B requires Service A during construction, an IoC container will enter an infinite recursive loop and crash with a stack overflow. Solve this by refactoring shared logic into a Service C, or use lazy property getters.
3. **Memory Leaks in Singleton Containers**: If an IoC container is stored globally and holds references to services that accumulate in-memory event listeners or cache records, the entire graph remains pinned in memory forever.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Passing the `Container` into service constructors | Service Locator anti-pattern; hides true dependencies | Inject the concrete resolved services directly |
| Hardcoding `new Service()` in classes | Couples classes to implementations; impossible to unit test | Pass dependencies via constructor parameters |
| Overusing heavy reflection DI frameworks for small projects | Adds TypeScript reflection bloat and runtime overhead | Use Pure DI (constructor parameters) |
| Creating circular constructor dependencies | Causes infinite recursion and stack overflow in IoC resolution | Extract shared logic to a 3rd service or use lazy getters |
| Storing request-scoped data in Singleton services | Leaks user data across concurrent HTTP requests | Use Scoped lifecycles for request contexts |

---

## 8. Performance Characteristics

- **Constructor Overhead**: Destructuring an options object `{ db, mailer }` adds $< 10$ nanoseconds of overhead per instantiation in V8.
- **Container Resolution Speed**: A simple `Map`-based IoC container resolves cached singletons in sub-microsecond $O(1)$ time.

---

## 9. Engine Internals

1. **V8 In-Object Property Optimization**:
   - Injected dependencies assigned to private `#fields` or public properties in constructors establish a consistent Hidden Class layout during instantiation, ensuring monomorphic property access throughout service lifecycles.

---

## 10. Best Practices & Design Guidelines

- **Prefer Pure DI (Constructor Injection)**: For 90% of Node.js and frontend projects, manually assembling dependencies in a composition root (`app.js` or `main.js`) is simpler and more transparent than pulling in heavy IoC libraries.
- **Group Dependencies in Parameter Objects**: Instead of `constructor(a, b, c, d, e)`, use an object `constructor({ a, b, c, d, e })`. This eliminates parameter ordering mistakes and simplifies testing overrides.
- **Design to Interfaces**: Depend on abstract contracts (method signatures), not concrete classes.

---

## 11. Real-World Case Study

### Architectural Composition Root in an Express Backend
```javascript
// composition-root.js: The single place where the application graph is assembled
import { PostgresDatabase } from './infrastructure/PostgresDatabase.js';
import { SendGridMailer } from './infrastructure/SendGridMailer.js';
import { RedisTokenStore } from './infrastructure/RedisTokenStore.js';
import { UserService } from './domain/UserService.js';
import { AuthController } from './api/AuthController.js';

export function buildApplication(config) {
  // 1. Instantiate Infrastructure Layer
  const database = new PostgresDatabase(config.databaseUrl);
  const mailer = new SendGridMailer(config.sendgridApiKey);
  const tokenStore = new RedisTokenStore(config.redisUrl);

  // 2. Instantiate Domain Services
  const userService = new UserService({ database, mailer, tokenStore });

  // 3. Instantiate Presentation Controllers
  const authController = new AuthController({ userService });

  return {
    authController,
    async shutdown() {
      await database.close();
      await tokenStore.close();
    },
  };
}

// In server.js:
const app = buildApplication(process.env);
// In test suite:
// const app = buildApplication(mockConfig); // Easy!
```

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between Inversion of Control (IoC) and Dependency Injection (DI)?</summary>
<b>Inversion of Control (IoC)</b> is a broad architectural design principle where the control flow of a program is inverted: rather than custom code controlling execution and creating dependencies, an external framework or caller directs control flow. <b>Dependency Injection (DI)</b> is a specific software design pattern that implements IoC by passing dependencies into objects rather than letting objects construct them directly.
</details>

<details>
<summary>2. Why is the Service Locator pattern often considered an anti-pattern compared to Dependency Injection?</summary>
The Service Locator pattern passes the container or registry into the class, which queries the locator for dependencies. This is an anti-pattern because it hides the class's true dependencies (you cannot see what it requires without inspecting its method bodies), couples the class to the container framework, and makes isolated unit testing significantly harder.
</details>

<details>
<summary>3. What are the three primary lifecycles in an IoC container?</summary>
1. <b>Singleton</b>: Exactly one instance is created and shared across the entire application process.
2. <b>Transient</b>: A brand-new instance is instantiated every time the dependency is requested.
3. <b>Scoped</b>: An instance is shared within a specific execution boundary, such as a single HTTP request lifecycle.
</details>

<details>
<summary>4. What is a "Composition Root"?</summary>
The Composition Root is the centralized location in an application (typically near the entry point, like <code>main.js</code> or <code>server.js</code>) where the entire dependency graph is wired together and instantiated before the application starts accepting requests.
</details>

<details>
<summary>5. How can you handle circular dependencies when using constructor injection?</summary>
Circular dependencies can be resolved by:
1. Refactoring shared responsibilities into a new, independent third service.
2. Using lazy evaluation (injecting a factory function or getter <code>() => serviceB</code> rather than the resolved instance).
3. Using property/setter injection to attach the dependency after instantiation.
</details>

<details>
<summary>6. What is "Pure DI" (or Poor Man's DI)?</summary>
Pure DI is the practice of implementing dependency injection manually using standard language features (constructor parameters and factory functions) without relying on any external IoC container libraries.
</details>

<details>
<summary>7. How does Dependency Injection make unit testing easier?</summary>
Because dependencies are passed from the outside, unit tests can instantiate the target class with lightweight in-memory fake objects or mock stubs instead of real database connections, file systems, or external third-party HTTP APIs.
</details>

<details>
<summary>8. Why is parameter object destructuring (constructor({ a, b })) preferred for DI in JavaScript?</summary>
Because JavaScript lacks named parameters. Positional parameters (<code>constructor(a, b, c, d)</code>) are prone to ordering mistakes when services have many dependencies. Parameter object destructuring allows passing dependencies by name in any order and makes it easy to provide default options.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build an Auto-Wiring Dependency Injector
Implement an asynchronous function `autoWire(factoryFn, container)` that inspects the parameter names of a service factory function and automatically resolves and injects those dependencies from the container.

<details>
<summary>View Solution</summary>

```javascript
export function autoWire(factoryFn, container) {
  // Extract parameter names using regex from function string
  const fnStr = factoryFn.toString();
  const paramMatch = fnStr.match(/\(([^)]*)\)/);
  if (!paramMatch) return factoryFn();

  const paramNames = paramMatch[1]
    .split(',')
    .map(p => p.trim())
    .filter(Boolean);

  const resolvedDependencies = paramNames.map(name => container.resolve(name));
  return factoryFn(...resolvedDependencies);
}
```
</details>

### Exercise 2: Scoped Request Context Manager
Implement a class `ScopedContext` that creates an isolated scope for incoming HTTP requests, ensuring that a request-scoped database transaction (`dbTransaction`) is automatically rolled back if an error occurs, or committed if the request completes successfully.

<details>
<summary>View Solution</summary>

```javascript
export class ScopedTransactionManager {
  constructor(databasePool) {
    this.pool = databasePool;
  }

  async runInScope(action) {
    const transaction = await this.pool.beginTransaction();
    const scope = {
      db: transaction,
      id: crypto.randomUUID(),
    };

    try {
      const result = await action(scope);
      await transaction.commit();
      return result;
    } catch (err) {
      await transaction.rollback();
      throw err;
    }
  }
}
```
</details>
