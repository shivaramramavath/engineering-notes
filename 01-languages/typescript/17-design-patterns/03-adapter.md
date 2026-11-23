# Adapter and Facade Patterns in TypeScript

> Implementing the Adapter and Facade design patterns in TypeScript: reconciling incompatible third-party interfaces, object vs class adapters, anti-corruption layers (ACL), and structural subtyping.

| Metadata | Details |
| :--- | :--- |
| **Category** | Design Patterns |
| **Difficulty** | Intermediate |
| **Prerequisites** | [04-objects-and-interfaces/00-interfaces.md](../04-objects-and-interfaces/00-interfaces.md), [05-classes/00-class-basics.md](../05-classes/00-class-basics.md) |
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

The **Adapter Pattern** is a structural design pattern that allows objects with incompatible interfaces to collaborate. In enterprise TypeScript development, applications frequently integrate external third-party SDKs (Stripe, Twilio, SendGrid), legacy libraries, or browser APIs that do not conform to internal domain interfaces.

Directly coupling business logic to external SDK interfaces causes:
1. **Vendor Lock-in**: Switching providers (e.g., from AWS S3 to Google Cloud Storage) requires refactoring hundreds of application files.
2. **Fragile Testing**: External classes with complex internal state are painful to mock in unit tests.
3. **Leaky Abstractions**: Third-party exceptions and idiosyncratic data structures leak into the core domain layer.

By introducing an Adapter (often serving as an **Anti-Corruption Layer (ACL)** in Domain-Driven Design), we wrap the third-party dependency behind a clean, domain-specific TypeScript interface.

```
+--------------------+        calls         +---------------------+
|   Domain Service   |  ----------------->  |   Target Interface  |
|  (Business Logic)  |                      |    (Internal API)   |
+--------------------+                      +---------------------+
                                                       ^
                                                       | implements
                                            +---------------------+       adapts        +--------------------+
                                            |       Adapter       |  ---------------->  |       Adaptee      |
                                            |   (Translation)     |                     | (3rd Party SDK/API)|
                                            +---------------------+                     +--------------------+
```

---

## 2. Core Concepts

### 1. Object Adapter vs Class Adapter
- **Object Adapter (Composition)**: The adapter holds an instance of the adaptee (`private adaptee: ThirdPartyService`). This is the idiomatic approach in TypeScript and JavaScript.
- **Class Adapter (Inheritance)**: The adapter inherits from both the domain class and the adaptee class (multiple inheritance). Because TypeScript classes only support single inheritance, class adapters are achieved via interface implementation (`class Adapter extends Adaptee implements TargetInterface`).

### 2. Adapter vs Facade vs Proxy
- **Adapter**: Converts one specific interface into another expected interface to resolve incompatibility.
- **Facade**: Simplifies a complex subsystem into an easier, unified high-level interface.
- **Proxy**: Provides the identical interface to control access, add caching, or perform lazy loading.

---

## 3. How It Works Internally

The adapter acts as a translation layer at compile-time and runtime:
1. **Compile-Time**: Implements the target interface `Target`, ensuring all required domain methods exist with expected types.
2. **Runtime**: Intercepts domain method calls, transforms parameter formats into the format expected by the adaptee, invokes the adaptee, catches adaptee exceptions, and translates responses into domain models.

```
Domain Call:             adapter.sendEmail({ to: 'a@b.com', body: 'Hi' })
                              |
Parameter Translation:   const payload = { personalizations: [{ to: [{ email: 'a@b.com' }] }], content: [...] }
                              |
Adaptee Invocation:      await sendgridSdk.send(payload)
                              |
Return Translation:      return { success: true }
```

---

## 4. Syntax & API Reference

### Generic Adapter Contract
```typescript
export interface TargetPort<TInput, TOutput> {
  process(data: TInput): Promise<TOutput>;
}

export class ServiceAdapter<TAdaptee, TInput, TOutput> implements TargetPort<TInput, TOutput> {
  constructor(protected readonly adaptee: TAdaptee) {}

  public async process(data: TInput): Promise<TOutput> {
    throw new Error('Must be implemented by concrete adapter');
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic Object Adapter (Date Library Adapter)
```typescript
export interface DateTimeProvider {
  nowIso(): string;
  addDays(iso: string, days: number): string;
}

// Adaptee: Legacy library or specialized library
class LegacyDateLib {
  public getTimestamp(): number { return Date.now(); }
  public shiftDays(timestamp: number, days: number): number {
    return timestamp + days * 86400000;
  }
}

export class LegacyDateAdapter implements DateTimeProvider {
  constructor(private legacyLib: LegacyDateLib) {}

  public nowIso(): string {
    return new Date(this.legacyLib.getTimestamp()).toISOString();
  }

  public addDays(iso: string, days: number): string {
    const ts = new Date(iso).getTime();
    const shifted = this.legacyLib.shiftDays(ts, days);
    return new Date(shifted).toISOString();
  }
}
```

### Example 2: Real-World Cloud Storage Anti-Corruption Layer
```typescript
export interface FileStoragePort {
  save(filename: string, content: Buffer): Promise<string>;
  delete(filename: string): Promise<void>;
}

// Adaptee: AWS S3 Client
class MockS3Client {
  public async putObject(params: { Bucket: string; Key: string; Body: Buffer }) {
    return { ETag: '"hash123"' };
  }
  public async deleteObject(params: { Bucket: string; Key: string }) {
    return {};
  }
}

export class S3StorageAdapter implements FileStoragePort {
  constructor(private s3: MockS3Client, private bucketName: string) {}

  public async save(filename: string, content: Buffer): Promise<string> {
    const res = await this.s3.putObject({
      Bucket: this.bucketName,
      Key: filename,
      Body: content
    });
    return `s3://${this.bucketName}/${filename}`;
  }

  public async delete(filename: string): Promise<void> {
    await this.s3.deleteObject({
      Bucket: this.bucketName,
      Key: filename
    });
  }
}
```

### Example 3: Edge Case - Adapting Async Callback API to Promise Interface
```typescript
export interface KeyValueStore {
  get(key: string): Promise<string | null>;
  set(key: string, value: string): Promise<void>;
}

// Old callback-based Node client
interface CallbackRedis {
  get(key: string, cb: (err: Error | null, val: string | null) => void): void;
  set(key: string, val: string, cb: (err: Error | null) => void): void;
}

export class CallbackRedisAdapter implements KeyValueStore {
  constructor(private client: CallbackRedis) {}

  public get(key: string): Promise<string | null> {
    return new Promise((resolve, reject) => {
      this.client.get(key, (err, val) => (err ? reject(err) : resolve(val)));
    });
  }

  public set(key: string, value: string): Promise<void> {
    return new Promise((resolve, reject) => {
      this.client.set(key, value, (err) => (err ? reject(err) : resolve()));
    });
  }
}
```

### Example 4: Failure Case - Missing Method in Incompatible Adaptee
```typescript
interface LoggerPort {
  info(msg: string): void;
  error(msg: string, err?: Error): void;
}

class IncompatibleConsole {
  log(msg: string) { console.log(msg); }
}

// @ts-expect-error TS2420: Class 'BadAdapter' incorrectly implements interface 'LoggerPort'. Property 'error' is missing.
export class BadAdapter implements LoggerPort {
  constructor(private c: IncompatibleConsole) {}
  info(msg: string) { this.c.log(msg); }
}
```

### Example 5: Idiomatic Facade for Multi-Step Payment Processing
```typescript
class InventoryService { public reserve(sku: string, qty: number) { return true; } }
class PaymentGateway { public charge(amount: number) { return 'tx_123'; } }
class NotificationService { public notifyCustomer(email: string) { return true; } }

export class CheckoutFacade {
  constructor(
    private inventory: InventoryService,
    private payment: PaymentGateway,
    private notifications: NotificationService
  ) {}

  public async placeOrder(sku: string, qty: number, amount: number, email: string): Promise<{ orderId: string }> {
    const reserved = this.inventory.reserve(sku, qty);
    if (!reserved) throw new Error('Out of stock');

    const txId = this.payment.charge(amount);
    this.notifications.notifyCustomer(email);

    return { orderId: txId };
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Uncaught Adaptee Exceptions**: If the third-party library throws custom error instances (e.g. `AwsS3ServiceError`), failing to catch and wrap them in domain errors breaks the isolation of the domain layer. Always translate third-party errors inside the adapter.
2. **Behavioral Divergence**: Even if an adapter satisfies the TypeScript interface type-wise, behavioral differences (such as different timeout behaviors, idempotency guarantees, or character encoding) can cause production bugs.
3. **Over-Adapting Simple Utilities**: Creating adapters for pure, standardized utility libraries (like `lodash` or `date-fns`) adds needless layer complexity. Reserve adapters for I/O services and boundary systems.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Leaking Adaptee Types** | `public get(): AWS.S3.Object` | `public get(): DomainFile` | Domain layer becomes coupled to AWS SDK types |
| **Leaking Adaptee Exceptions** | Letting `StripeCardError` bubble up | Catch and rethrow as `DomainPaymentError` | External exception hierarchy pollutes domain code |
| **Class Adapter with Multiple Inheritance Hacks** | Mixing prototype properties dynamically | Object composition (`private adaptee`) | Fragile monkey-patching and loss of compiler tracking |
| **God Facade** | Single facade handling 50 unrelated services | Cohesive, focused domain facades | Tight coupling and high risk of regressions |
| **Ignoring Asynchronous Errors** | Not handling unhandled promise rejections in callbacks | Proper `new Promise((res, rej) => ...)` | Node process unhandledRejection crash |
| **Modifying Third-Party Prototypes** | Extending `Date.prototype` or `Array.prototype` | Dedicated wrapper/adapter class | Global pollution and compatibility bugs with third-party libs |

---

## 8. Performance & Type-Checker Complexity

- **Delegation Overhead**: Method delegation through an adapter (`adapter.method -> adaptee.method`) adds a single function stack frame, incurring negligible CPU overhead (<1 nanosecond).
- **Inlining**: Modern V8 JIT compilers easily inline trivial adapter forwarding calls.

---

## 9. Compiler Internals

When an adapter implements an interface:
```typescript
class MyAdapter implements TargetInterface { ... }
```
The compiler's type checker verifies that the class constructor's instance type contains every symbol declared in `TargetInterface`, checking parameter contravariance and return type covariance. If any method signature differs, `TS2420` is reported.

---

## 10. Best Practices & Design Guidelines

1. **Favor Object Adapters over Class Adapters**: Use composition (`has-a`) instead of inheritance (`is-a`).
2. **Define the Target Interface in the Domain Layer**: The interface belongs to the consumer, not the vendor.
3. **Translate All Errors**: Catch vendor exceptions in the adapter and translate them into domain-specific exceptions.
4. **Use Facades to Hide Subsystem Complexity**: Combine multiple lower-level adapters into high-level operations.

---

## 11. Real-World Case Study

### Swapping Payment Providers with Zero Domain Changes
A high-volume subscription platform migrates from Stripe to Adyen. Because the domain interacted solely with an internal `IPaymentProvider` interface, only the adapter implementation was swapped:

```typescript
export interface CustomerBillingProfile { id: string; email: string; }
export interface PaymentResult { success: boolean; transactionId: string; }

export interface IPaymentProvider {
  chargeCustomer(customer: CustomerBillingProfile, amountCents: number): Promise<PaymentResult>;
}

// Domain Service remains 100% unchanged!
export class SubscriptionService {
  constructor(private paymentProvider: IPaymentProvider) {}

  public async renewSubscription(customer: CustomerBillingProfile, amount: number) {
    const result = await this.paymentProvider.chargeCustomer(customer, amount);
    if (!result.success) throw new Error('Renewal failed');
    return result;
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental purpose of the Adapter pattern?</summary>

To allow classes with incompatible interfaces to work together by wrapping one class's interface around an existing class. It translates requests from the client's expected format into the format required by the adaptee.
</details>

<details>
<summary>2. What is an Anti-Corruption Layer (ACL) in Domain-Driven Design and how does it relate to the Adapter pattern?</summary>

An Anti-Corruption Layer is an architectural boundary that isolates a domain model from external subsystems or legacy APIs. It is implemented using Adapters, Translators, and Facades to ensure that external data shapes and concepts do not corrupt the purity of the core domain.
</details>

<details>
<summary>3. Why is composition preferred over inheritance when implementing adapters in TypeScript?</summary>

TypeScript (and JavaScript) does not support multiple class inheritance. With composition (Object Adapter), the adapter holds a reference to the adaptee, allowing it to adapt any subclass or alternative implementation of the adaptee dynamically at runtime without being rigidly bound to its inheritance hierarchy.
</details>

<details>
<summary>4. What is the difference between an Adapter and a Facade?</summary>

An **Adapter** changes an existing interface to match an expected interface, making two incompatible systems work together. A **Facade** defines a new, simplified interface over an entire complex subsystem to make it easier to consume.
</details>

<details>
<summary>5. How should third-party exceptions be handled in an Adapter?</summary>

They must be caught within the adapter methods and translated into domain-specific error types. Letting raw vendor exceptions escape defeats the decoupling purpose of the adapter pattern.
</details>

<details>
<summary>6. How can you adapt a callback-based legacy API to a modern Promise-based interface?</summary>

By wrapping the callback invocation inside `new Promise((resolve, reject) => { ... })`, forwarding errors to `reject(err)` and successful values to `resolve(data)`.
</details>

<details>
<summary>7. What is the role of structural typing in TypeScript adapters?</summary>

Because TypeScript is structurally typed, an adapter does not strictly need to explicitly declare `implements TargetInterface`. As long as its public shape satisfies the target interface, it can be passed anywhere the interface is expected. However, using `implements` is recommended for compile-time safety during development.
</details>

<details>
<summary>8. When does introducing an Adapter become an anti-pattern?</summary>

When applied prematurely to stable, standard libraries that are unlikely to change, or when an adapter merely passes calls through without performing any translation or abstraction, adding boilerplate without providing architectural decoupling.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Console Logger Adapter</summary>

**Problem:** Create a `LogAdapter` that adapts the browser `console` object to an `ILogger` interface with `logMessage(msg: string): void`.

**Solution:**
```typescript
export interface ILogger { logMessage(msg: string): void; }
export class ConsoleLogAdapter implements ILogger {
  constructor(private c: Console = console) {}
  public logMessage(msg: string): void { this.c.log(msg); }
}
```
</details>

<details>
<summary>Exercise 2 (Easy): LocalStorage to Async Storage Adapter</summary>

**Problem:** Create an adapter that wraps synchronous `window.localStorage` in an asynchronous `AsyncStorage` interface.

**Solution:**
```typescript
export interface AsyncStorage {
  getItem(key: string): Promise<string | null>;
  setItem(key: string, val: string): Promise<void>;
}

export class LocalStorageAdapter implements AsyncStorage {
  constructor(private storage: Storage = window.localStorage) {}
  async getItem(key: string) { return this.storage.getItem(key); }
  async setItem(key: string, val: string) { this.storage.setItem(key, val); }
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Fetch to Axios-Like Client Adapter</summary>

**Problem:** Adapt the native `fetch` API to an interface `SimpleHttpClient` with `get<T>(url: string): Promise<{ data: T; status: number }>`.

**Solution:**
```typescript
export interface SimpleHttpClient {
  get<T>(url: string): Promise<{ data: T; status: number }>;
}

export class FetchHttpAdapter implements SimpleHttpClient {
  async get<T>(url: string): Promise<{ data: T; status: number }> {
    const res = await fetch(url);
    const data = await res.json();
    return { data, status: res.status };
  }
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Event Listener Adapter</summary>

**Problem:** Adapt a Node.js `EventEmitter` (`.on(event, handler)`) to a DOM-style `EventTarget` (`.addEventListener(type, listener)`).

**Solution:**
```typescript
import { EventEmitter } from 'events';

export class NodeToDomEventAdapter {
  constructor(private ee: EventEmitter) {}

  public addEventListener(type: string, listener: (...args: any[]) => void): void {
    this.ee.on(type, listener);
  }

  public removeEventListener(type: string, listener: (...args: any[]) => void): void {
    this.ee.off(type, listener);
  }
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Universal Cache Adapter with TTL</summary>

**Problem:** Create an adapter that wraps any key-value store lacking TTL support (like a basic Map) and implements an `ExpiringCache` interface with auto-expiration.

**Solution:**
```typescript
export interface ExpiringCache<V> {
  set(key: string, value: V, ttlMs: number): void;
  get(key: string): V | null;
}

export class TTLMapAdapter<V> implements ExpiringCache<V> {
  private map = new Map<string, { value: V; expiresAt: number }>();

  public set(key: string, value: V, ttlMs: number): void {
    this.map.set(key, { value, expiresAt: Date.now() + ttlMs });
  }

  public get(key: string): V | null {
    const entry = this.map.get(key);
    if (!entry) return null;
    if (Date.now() > entry.expiresAt) {
      this.map.delete(key);
      return null;
    }
    return entry.value;
  }
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Resilient Fallback Facade</summary>

**Problem:** Implement a facade that coordinates primary and secondary SMS providers, automatically failing over to the secondary provider if the primary throws an exception.

**Solution:**
```typescript
export interface SmsSender { send(to: string, msg: string): Promise<boolean>; }

export class FailoverSmsFacade implements SmsSender {
  constructor(
    private primary: SmsSender,
    private secondary: SmsSender
  ) {}

  public async send(to: string, msg: string): Promise<boolean> {
    try {
      return await this.primary.send(to, msg);
    } catch (err) {
      console.warn('Primary SMS provider failed, failing over to secondary', err);
      return await this.secondary.send(to, msg);
    }
  }
}
```
</details>
