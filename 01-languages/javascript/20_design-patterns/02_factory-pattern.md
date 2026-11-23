# The Factory Pattern in JavaScript

> Simple Factory, Factory Method, Abstract Factory, and dynamic polymorphic object instantiation without constructor coupling.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [Objects and Prototypes](file:///e:/Learning/engineering-notes/01-languages/javascript/05_this-and-oop/03_prototypes-and-prototype-chain.md), [Classes](file:///e:/Learning/engineering-notes/01-languages/javascript/05_this-and-oop/05_classes.md) |
| **Time to read** | 22 minutes |
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

In object-oriented programming, instantiating concrete classes directly using the `new` operator couples calling code directly to the implementation details of those classes. If the instantiation logic later requires complex initialization, caching, dependency injection, or switching implementations based on runtime configuration (e.g., swapping a SQLite database driver for Postgres), every single `new Database()` call across the codebase must be refactored.

The **Factory Pattern** encapsulates object creation behind a clean functional abstraction. In JavaScript, where functions are first-class and object literals are natively flexible, factory functions offer a powerful, lightweight alternative to complex class inheritance hierarchies.

---

## 2. Core Concepts

### Variations of the Factory Pattern
1. **Simple Factory**: A standalone function that encapsulates object creation logic, deciding which object structure to construct based on passed parameters.
2. **Factory Method**: Defines an interface for creating an object, but delegates the actual instantiation decision to subclasses or specialized creator functions.
3. **Abstract Factory**: A factory that produces families of related or dependent objects without specifying their concrete classes (e.g., UI component kits that produce matching buttons, inputs, and modals for Dark Mode vs Light Mode).

### Factory Functions vs `new` Constructors
- Factory functions are regular JavaScript functions that return an object.
- They do not require the `new` operator, eliminating the catastrophic `this` binding bugs that occur when `new` is forgotten on traditional constructor functions.
- They can return different object shapes, cached instances, or Promises (Async Factories), whereas constructors can only return synchronous object instances bound to their prototype.

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| CLIENT CALLER CODE                                                       |
| const logger = LoggerFactory.createLogger(process.env.NODE_ENV);         |
+------------------------------------+-------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
| FACTORY METHOD (LoggerFactory)                                           |
| Inspects environment / config:                                           |
|   - If 'production'  -> instantiate Pino / Datadog Logger               |
|   - If 'development' -> instantiate Colorized Console Logger             |
|   - If 'test'        -> instantiate In-Memory Mock Logger                |
+------------------------------------+-------------------------------------+
                                     |
              +----------------------+----------------------+
              |                      |                      |
              v                      v                      v
+----------------------+ +----------------------+ +----------------------+
| Production Logger    | | Development Logger   | | Test Mock Logger     |
| - JSON formatter     | | - Pretty colorizer   | | - In-memory array    |
| - stdout streaming   | | - console.debug      | | - assert() spy       |
+----------------------+ +----------------------+ +----------------------+
All implement the unified ILogger interface: { info(msg), error(msg) }
```

---

## 4. Syntax & API Reference

### Simple Factory Function
```javascript
function createUser(type, name) {
  const basePermissions = ['read'];

  if (type === 'admin') {
    return {
      name,
      role: 'admin',
      permissions: [...basePermissions, 'write', 'delete', 'manage_users'],
    };
  }

  return {
    name,
    role: 'member',
    permissions: basePermissions,
  };
}
```

### Async Factory Pattern
Constructors in JavaScript cannot be `async`. The Async Factory pattern solves this limitation:
```javascript
class DatabaseConnection {
  // Private constructor prevents direct `new` instantiation
  constructor(client) {
    this.client = client;
  }

  static async connect(connectionString) {
    const rawClient = await driver.open(connectionString);
    await rawClient.ping(); // Asynchronous handshake
    return new DatabaseConnection(rawClient);
  }
}

// Client usage:
const db = await DatabaseConnection.connect('postgres://localhost:5432/db');
```

---

## 5. Practical Examples

### Example 1: Notification Dispatcher Factory (Simple Factory)
```javascript
class EmailNotification {
  send(recipient, message) {
    console.log(`[EMAIL] Sending to ${recipient}: ${message}`);
  }
}

class SMSNotification {
  send(recipient, message) {
    console.log(`[SMS] Sending to ${recipient}: ${message}`);
  }
}

class PushNotification {
  send(recipient, message) {
    console.log(`[PUSH] Dispatching to device ${recipient}: ${message}`);
  }
}

export class NotificationFactory {
  static create(channel) {
    switch (channel.toLowerCase()) {
      case 'email':
        return new EmailNotification();
      case 'sms':
        return new SMSNotification();
      case 'push':
        return new PushNotification();
      default:
        throw new Error(`Unsupported notification channel: ${channel}`);
    }
  }
}
```

### Example 2: Cross-Platform Storage Engine (Abstract Factory)
```javascript
// Abstract Factory creating matching key-value storage and file storage
export function createStorageFactory(environment = 'browser') {
  if (environment === 'browser') {
    return {
      createKeyValueStore: () => ({
        get: (k) => Promise.resolve(localStorage.getItem(k)),
        set: (k, v) => Promise.resolve(localStorage.setItem(k, v)),
      }),
      createFileStore: () => ({
        save: (name, blob) => {
          const a = document.createElement('a');
          a.href = URL.createObjectURL(blob);
          a.download = name;
          a.click();
          return Promise.resolve();
        },
      }),
    };
  }

  // Node.js implementation
  return {
    createKeyValueStore: () => {
      const inMemory = new Map();
      return {
        get: (k) => Promise.resolve(inMemory.get(k)),
        set: (k, v) => Promise.resolve(inMemory.set(k, v)),
      };
    },
    createFileStore: () => ({
      save: async (name, buffer) => {
        const fs = await import('node:fs/promises');
        await fs.writeFile(name, buffer);
      },
    }),
  };
}
```

### Example 3: Extensible Open-Closed Factory via Registry
```javascript
// Avoid massive switch-case statements by allowing dynamic plugin registration
export class PaymentProcessorFactory {
  static #registry = new Map();

  static register(type, creatorFn) {
    this.#registry.set(type.toLowerCase(), creatorFn);
  }

  static create(type, options) {
    const creator = this.#registry.get(type.toLowerCase());
    if (!creator) {
      throw new Error(`No payment processor registered for: ${type}`);
    }
    return creator(options);
  }
}

// Register plugins cleanly without modifying factory source code
PaymentProcessorFactory.register('stripe', (opts) => new StripeGateway(opts.apiKey));
PaymentProcessorFactory.register('paypal', (opts) => new PayPalGateway(opts.clientId));
```

### Example 4: Composable Object Factory with Functional Mixins
```javascript
const withLogging = (state) => ({
  log: (msg) => console.log(`[${state.name}] ${msg}`),
});

const withValidation = (state) => ({
  validate: () => state.age >= 18,
});

export function createPlayer(name, age, score = 0) {
  const state = { name, age, score };

  return Object.freeze({
    ...state,
    ...withLogging(state),
    ...withValidation(state),
    addScore(points) {
      state.score += points;
      return state.score;
    },
  });
}
```

### Example 5: Testing Mock Injection Factory
```javascript
export function createHttpClientFactory(isMock = false) {
  if (isMock) {
    const recordedCalls = [];
    return {
      client: {
        get: async (url) => {
          recordedCalls.push({ url, timestamp: Date.now() });
          return { status: 200, data: { mock: true } };
        },
      },
      history: recordedCalls,
    };
  }

  return {
    client: {
      get: async (url) => {
        const res = await fetch(url);
        return { status: res.status, data: await res.json() };
      },
    },
    history: null,
  };
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Loss of `instanceof` Checks**: Objects produced by functional factories without prototypes will fail `instanceof SomeClass` checks. Instead of relying on `instanceof`, embrace **Duck Typing** ("if it walks like a duck and quacks like a duck, it is a duck") or explicit type tags (`obj.type === 'admin'`).
2. **Switch-Case Explosion**: Hardcoding class types inside a monolithic `switch` statement violates the **Open-Closed Principle** (open for extension, closed for modification). Use the Registry pattern (Example 3) so new classes can be registered dynamically.
3. **Async Constructor Anti-Pattern**: Attempting to initiate network or disk connections inside a constructor leads to unhandled promise rejections and race conditions. Always use static asynchronous factory methods (`Class.create()`).

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Giant nested `switch-case` in factory | Must edit factory code every time a new type is created | Use a dynamic Registry `Map` of creators |
| Performing async I/O in constructors | Constructors cannot return promises cleanly | Use static `async create()` factory methods |
| Using `instanceof` on factory output | Factory objects might not share prototype chains | Use Duck Typing or discriminant property tags |
| Over-engineering simple objects with factories | Unnecessary indirection for static, trivial structures | Use plain object literals or simple constructors |
| Forgetting to throw on unrecognized types | Silently returns `undefined`, causing runtime crashes later | Explicitly throw descriptive `Error` on unknown types |

---

## 8. Performance Characteristics

- **Allocation Speed**: Plain factory functions returning object literals execute at near-identical speed to class constructors in V8.
- **Hidden Class Consistency**: If a factory function consistently returns object literals with identical property order, V8 shares the exact same Hidden Class, maintaining peak monomorphic property access speed.

---

## 9. Engine Internals

1. **In-Object Property Pre-allocation**:
   - When a factory function returns `{ a: 1, b: 2 }`, V8 analyzes the literal AST and allocates space for in-object properties in contiguous memory.
2. **TurboFan Inlining of Small Factories**:
   - Small factory functions that return simple objects are frequently inlined directly into calling code by TurboFan, completely eliminating function call overhead.

---

## 10. Best Practices & Design Guidelines

- **Standardize Interface Contracts**: Ensure all objects returned by a factory share a consistent method signature so consumers can treat them polymorphically.
- **Favor Factory Functions over Constructors**: Prefer factory functions for objects requiring encapsulation, private closures, or dynamic configuration.
- **Use Registries for Extensibility**: Allow external packages or plugins to register new creators without modifying core code.

---

## 11. Real-World Case Study

### Multi-Cloud Object Storage Factory
```javascript
// Clean abstraction allowing microservices to switch between S3, GCS, and Local Disk
export class CloudStorageFactory {
  static create(provider, config) {
    switch (provider.toUpperCase()) {
      case 'AWS_S3':
        return {
          upload: async (key, buffer) => {
            console.log(`Uploading ${key} to AWS S3 bucket: ${config.bucket}`);
            // S3Client.send(new PutObjectCommand(...))
          },
        };
      case 'GCS':
        return {
          upload: async (key, buffer) => {
            console.log(`Uploading ${key} to Google Cloud Storage: ${config.bucket}`);
            // storage.bucket(config.bucket).file(key).save(buffer)
          },
        };
      case 'LOCAL':
        return {
          upload: async (key, buffer) => {
            const fs = await import('node:fs/promises');
            const path = await import('node:path');
            await fs.writeFile(path.resolve(config.directory, key), buffer);
          },
        };
      default:
        throw new Error(`Unsupported storage provider: ${provider}`);
    }
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between a Simple Factory, Factory Method, and Abstract Factory?</summary>
A <b>Simple Factory</b> is a single function or class that instantiates different object types based on input arguments. A <b>Factory Method</b> defines an interface for object creation but delegates the actual creation to derived subclasses or specialized creator functions. An <b>Abstract Factory</b> provides an interface for creating families of related or dependent objects without specifying their concrete classes.
</details>

<details>
<summary>2. Why are factory functions often preferred over ES6 classes in JavaScript?</summary>
Factory functions do not require the <code>new</code> operator (eliminating bugs caused by missing <code>new</code>), allow true private state via closures, can return completely different object types or cached instances conditionally, and can be asynchronous.
</details>

<details>
<summary>3. How do you handle asynchronous initialization when creating an object in JavaScript?</summary>
Because constructors cannot be marked <code>async</code>, the standard pattern is to use a static asynchronous factory method (e.g. <code>static async create()</code>) that performs the asynchronous operations (such as connecting to a database or reading a file) and returns the fully initialized instance.
</details>

<details>
<summary>4. How does the Registry pattern improve the Factory pattern?</summary>
A hardcoded <code>switch-case</code> statement in a factory violates the Open-Closed Principle because adding a new type requires modifying the factory source code. The Registry pattern uses an internal <code>Map</code> of creator functions, allowing new types to be registered dynamically at runtime without altering core factory code.
</details>

<details>
<summary>5. What happens to instanceof checks when using functional factory objects?</summary>
Objects created by functional factory functions returning plain object literals do not link to a specific constructor's prototype. As a result, <code>instanceof</code> checks return <code>false</code>. Code should instead rely on Duck Typing or discriminant type properties (e.g., <code>obj.type === 'USER'</code>).
</details>

<details>
<summary>6. How can a factory enforce the Open-Closed Principle (SOLID)?</summary>
By exposing a registration method that allows developers to add new product types and creators to the factory without modifying its internal logic or existing switch cases.
</details>

<details>
<summary>7. What is Duck Typing and how does it relate to the Factory pattern?</summary>
Duck Typing ("if it walks like a duck and quacks like a duck, it is a duck") relies on checking whether an object has the necessary methods and properties rather than its nominal class inheritance. Factories produce objects that satisfy a shared interface contract, allowing client code to interact with them polymorphically regardless of their underlying implementation.
</details>

<details>
<summary>8. Can factory functions cause memory bloat compared to class prototypes?</summary>
Yes. If a factory function defines methods as nested closures for every created instance, each instance allocates its own set of function closures in memory. If thousands or millions of instances are created, prototype methods on classes consume significantly less memory.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Form Validator Factory
Implement a factory function `createValidator(rules)` that accepts validation rules (e.g., `{ minLength: 5, required: true }`) and returns a validator object with a `validate(input)` method returning `{ isValid: boolean, errors: string[] }`.

<details>
<summary>View Solution</summary>

```javascript
export function createValidator(rules = {}) {
  const checks = [];

  if (rules.required) {
    checks.push(val => (val !== undefined && val !== null && val !== '') ? null : 'Field is required');
  }

  if (typeof rules.minLength === 'number') {
    checks.push(val => (typeof val === 'string' && val.length >= rules.minLength) ? null : `Minimum length is ${rules.minLength}`);
  }

  if (rules.pattern instanceof RegExp) {
    checks.push(val => rules.pattern.test(val) ? null : 'Invalid format');
  }

  return {
    validate(input) {
      const errors = [];
      for (const check of checks) {
        const error = check(input);
        if (error) errors.push(error);
      }
      return {
        isValid: errors.length === 0,
        errors,
      };
    },
  };
}
```
</details>

### Exercise 2: Extensible Logger Factory with Level Formatting
Implement an extensible `LoggerFactory` using the registry pattern that supports registering custom log output formatters (e.g., JSON, Plain Text, Colored Console) and producing loggers with `debug`, `info`, `warn`, and `error` methods.

<details>
<summary>View Solution</summary>

```javascript
export class LoggerFactory {
  static #formatters = new Map();

  static registerFormatter(name, formatterFn) {
    this.#formatters.set(name.toLowerCase(), formatterFn);
  }

  static createLogger(formatName = 'text') {
    const formatter = this.#formatters.get(formatName.toLowerCase()) || ((lvl, msg) => `[${lvl.toUpperCase()}] ${msg}`);

    function log(level, message) {
      const output = formatter(level, message);
      if (level === 'error') console.error(output);
      else if (level === 'warn') console.warn(output);
      else console.log(output);
    }

    return {
      debug: (msg) => log('debug', msg),
      info: (msg) => log('info', msg),
      warn: (msg) => log('warn', msg),
      error: (msg) => log('error', msg),
    };
  }
}

// Register JSON Formatter
LoggerFactory.registerFormatter('json', (level, msg) => JSON.stringify({ level, msg, timestamp: Date.now() }));
```
</details>
