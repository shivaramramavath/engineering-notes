# Strategy Pattern and Functional Inversion of Control in TypeScript

> Implementing the Strategy pattern in TypeScript: decoupling algorithms from contexts, first-class function strategies, discriminated strategy maps, and dependency injection.

| Metadata | Details |
| :--- | :--- |
| **Category** | Design Patterns |
| **Difficulty** | Intermediate |
| **Prerequisites** | [02-functions/00-function-types.md](../02-functions/00-function-types.md), [04-objects-and-interfaces/00-interfaces.md](../04-objects-and-interfaces/00-interfaces.md) |
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

The **Strategy Pattern** is a behavioral design pattern that defines a family of algorithms, encapsulates each one, and makes them interchangeable at runtime. The strategy pattern lets the algorithm vary independently from the clients that use it.

In legacy object-oriented codebases, implementing the Strategy pattern often involves heavy boilerplate:
- An abstract `Strategy` interface.
- 5 concrete strategy classes (`CreditCardStrategy`, `PayPalStrategy`, `BitcoinStrategy`).
- A `Context` class that holds a strategy reference and delegates execution.

In TypeScript, **functions are first-class citizens**. A strategy is simply a type signature for a function:
```typescript
type SortStrategy<T> = (a: T, b: T) => number;
```
By combining functional strategy signatures with object-oriented interfaces, TypeScript gives developers the flexibility to choose between lightweight closures and full-blown strategy classes depending on state complexity.

```
+--------------------+           delegates to          +--------------------+
|  PaymentContext    |  ---------------------------->  |  PaymentStrategy   |
| - amount: number   |                                 | (Interface or Fn)  |
| - executePayment() |                                 +--------------------+
+--------------------+                                           |
                                       +-------------------------+-------------------------+
                                       |                         |                         |
                                       v                         v                         v
                              +-----------------+       +-----------------+       +-----------------+
                              |   StripeCard    |       |     PayPal      |       |   CryptoWallet  |
                              +-----------------+       +-----------------+       +-----------------+
```

---

## 2. Core Concepts

### 1. Functional Strategy vs Class Strategy
- **Functional Strategy**: When the algorithm is stateless, a plain function `(input: T) => R` is preferred.
- **Class Strategy**: When the strategy requires its own private dependencies, state, life-cycle hooks, or configuration (e.g. API keys, database pools), an interface implemented by a class is appropriate.

### 2. Context Injection
The context object can receive the strategy via:
1. **Constructor Injection**: Injected when the context is created, fixed for the instance lifetime.
2. **Method Injection**: Passed directly to the executing method (`order.pay(stripeStrategy)`).
3. **Setter Mutation**: Changed at runtime (`order.setPaymentStrategy(payPalStrategy)`).

---

## 3. How It Works Internally

TypeScript verifies strategy compatibility via structural subtyping:
Any object or function that satisfies the parameter types and return type of the strategy interface is assignable, regardless of whether it uses the `implements` keyword.

```
Strategy Interface: interface Strategy { execute(data: Data): Result }
Anonymous Object:   const s = { execute: (d: Data) => ({ success: true }) }
Assignability:      CheckTypeRelatedTo(AnonymousObject, Strategy) -> SUCCESS
```

---

## 4. Syntax & API Reference

### Generic Strategy Definition
```typescript
export interface Strategy<TInput, TOutput> {
  execute(input: TInput): Promise<TOutput>;
}

export type StrategyFn<TInput, TOutput> = (input: TInput) => Promise<TOutput> | TOutput;
```

---

## 5. Practical Examples

### Example 1: Basic Functional Strategy (Discount Calculators)
```typescript
export type DiscountStrategy = (price: number) => number;

export const noDiscount: DiscountStrategy = (price) => price;
export const holidayDiscount: DiscountStrategy = (price) => price * 0.85;
export const vipDiscount: DiscountStrategy = (price) => price * 0.70;

export class Order {
  constructor(public total: number, private discountStrategy: DiscountStrategy = noDiscount) {}

  public setDiscountStrategy(strategy: DiscountStrategy): void {
    this.discountStrategy = strategy;
  }

  public getFinalPrice(): number {
    return this.discountStrategy(this.total);
  }
}
```

### Example 2: Real-World Authentication Strategy Pattern (Passport-Style)
```typescript
export interface AuthResult {
  success: boolean;
  userId?: string;
  error?: string;
}

export interface AuthStrategy {
  name: string;
  authenticate(req: Request): Promise<AuthResult>;
}

export class JwtAuthStrategy implements AuthStrategy {
  public readonly name = 'jwt';
  constructor(private secretKey: string) {}

  async authenticate(req: Request): Promise<AuthResult> {
    const authHeader = req.headers.get('authorization');
    if (!authHeader?.startsWith('Bearer ')) return { success: false, error: 'Missing Bearer token' };
    const token = authHeader.slice(7);
    return token === this.secretKey
      ? { success: true, userId: 'usr_jwt_123' }
      : { success: false, error: 'Invalid token' };
  }
}

export class ApiKeyAuthStrategy implements AuthStrategy {
  public readonly name = 'apiKey';
  constructor(private validApiKey: string) {}

  async authenticate(req: Request): Promise<AuthResult> {
    const key = req.headers.get('x-api-key');
    return key === this.validApiKey
      ? { success: true, userId: 'usr_api_456' }
      : { success: false, error: 'Invalid API Key' };
  }
}
```

### Example 3: Edge Case - Discriminated Strategy Map Lookup
```typescript
export type CompressionAlgorithm = 'gzip' | 'brotli' | 'deflate';

export type Compressor = (data: Buffer) => Promise<Buffer>;

export const compressorMap: Record<CompressionAlgorithm, Compressor> = {
  gzip: async (data) => Buffer.from(`gzip(${data.toString()})`),
  brotli: async (data) => Buffer.from(`brotli(${data.toString()})`),
  deflate: async (data) => Buffer.from(`deflate(${data.toString()})`)
};

export async function compressData(algo: CompressionAlgorithm, data: Buffer): Promise<Buffer> {
  const compressor = compressorMap[algo];
  return compressor(data);
}
```

### Example 4: Failure Case - Type Safety in Strategy Parameter Passing
```typescript
interface NumericStrategy {
  (a: number, b: number): number;
}

const stringConcatStrategy = (a: string, b: string) => a + b;

// @ts-expect-error TS2322: Type '(a: string, b: string) => string' is not assignable to type 'NumericStrategy'.
const numOp: NumericStrategy = stringConcatStrategy;
```

### Example 5: Idiomatic Strategy with Fallback Chain
```typescript
export class CompositeStrategy<TInput, TOutput> {
  constructor(private strategies: StrategyFn<TInput, TOutput | null>[]) {}

  public async execute(input: TInput, fallback: TOutput): Promise<TOutput> {
    for (const strategy of this.strategies) {
      const result = await strategy(input);
      if (result !== null) return result;
    }
    return fallback;
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **State Leaks Across Invocations**: If a strategy class caches intermediate calculation values in instance properties, concurrent requests using that same singleton strategy will corrupt each other's data. Strategies should ideally be stateless.
2. **Strategy Explosion**: Creating separate classes for trivial 1-line comparisons. Use lambda functions instead.
3. **Async / Sync Mismatch**: If the strategy interface is defined synchronously (`(x: T) => R`), implementing an asynchronous strategy requiring database I/O is impossible without rewriting the entire context to return a Promise. Always declare strategy interfaces as returning `Promise<R> | R` or `Promise<R>` if I/O is ever possible.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Hardcoded If/Else Ladder** | `if (type === 'A') ... else if ...` | Strategy Map or Injected Strategy | Violates Open/Closed Principle; modifying requires changing core class |
| **Stateful Leaky Strategy** | Storing request params on strategy instance | Pass all data via method arguments | Race conditions in multi-threaded/async Node.js |
| **Sync Signature for I/O** | `compute(): number` | `compute(): Promise<number>` | Cannot introduce external network/DB checks later |
| **Un-Typed Strategy Names** | `getStrategy(name: string)` | `getStrategy(name: StrategyKey)` | Typo in name causes runtime undefined error |
| **Over-Engineering Lambdas** | Class with 5 interfaces for `(a, b) => a - b` | Pure arrow function | Bloats code size by 50 lines unnecessarily |
| **Direct Mutation of Context** | Strategy reaching into context internals | Strategy returns result to context | Violates Law of Demeter and tightens coupling |

---

## 8. Performance & Type-Checker Complexity

- **Function Call Overhead**: Invoking a function strategy or calling a method on an object is optimized by V8's inline cache (IC). If all strategies share a monomorphic call site (calling `.execute()` on objects with identical hidden classes), V8 optimizes calls directly into machine code.
- **Type Checker**: Function types check significantly faster in TypeScript than deeply nested interface hierarchies.

---

## 9. Compiler Internals

When checking function assignability:
```typescript
type Target = (a: number) => void;
type Source = (a: number, b: string) => void;
```
The compiler checks parameter count and types. In TypeScript, functions with *fewer* parameters are assignable to functions expecting *more* parameters (e.g. `[1, 2].map(x => x)` ignores index and array arguments). However, the reverse is rejected with `TS2345`.

---

## 10. Best Practices & Design Guidelines

1. **Prefer Functions for Stateless Algorithms**: Use `type Strategy = (data: T) => R`.
2. **Use Classes When Strategies Require Configuration**: If a strategy holds API credentials or network connections, implement it as a class.
3. **Declare Return Types as Promises**: Anticipate async requirements by typing return as `Promise<R>` or `Promise<R> | R`.
4. **Use Discriminated Unions for Strategy Selection**: Group strategies by literal keys.

---

## 11. Real-World Case Study

### Multi-Gateway Payment Processing Engine
An international e-commerce platform selects payment processing strategies dynamically based on user currency and region:

```typescript
export interface PaymentRequest {
  orderId: string;
  amountCents: number;
  currency: 'USD' | 'EUR' | 'JPY';
}

export interface PaymentResponse {
  success: boolean;
  transactionReference: string;
}

export interface IPaymentGatewayStrategy {
  process(req: PaymentRequest): Promise<PaymentResponse>;
}

export class StripeGateway implements IPaymentGatewayStrategy {
  async process(req: PaymentRequest): Promise<PaymentResponse> {
    return { success: true, transactionReference: `stripe_${req.orderId}` };
  }
}

export class AdyenGateway implements IPaymentGatewayStrategy {
  async process(req: PaymentRequest): Promise<PaymentResponse> {
    return { success: true, transactionReference: `adyen_${req.orderId}` };
  }
}

export class PaymentRouter {
  public static selectStrategy(currency: string): IPaymentGatewayStrategy {
    if (currency === 'EUR') return new AdyenGateway();
    return new StripeGateway();
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. How does the Strategy pattern uphold the Open/Closed Principle?</summary>

The Open/Closed Principle states that software entities should be open for extension, but closed for modification. With the Strategy pattern, adding a new algorithm (e.g. a new Tax Calculator) only requires creating a new strategy class or function that implements the strategy interface. The context class consuming the strategy does not need to be modified.
</details>

<details>
<summary>2. How does TypeScript enable functional alternatives to traditional OOP Strategy classes?</summary>

Because TypeScript supports first-class functions and type aliases for function signatures (`type Strategy = (a: number) => number`), strategies can be defined as pure functions or lambda expressions without the overhead of declaring abstract classes or interfaces.
</details>

<details>
<summary>3. What is the difference between Strategy and State patterns?</summary>

While both patterns share similar UML class structures, their intent differs:
- In the **Strategy pattern**, algorithms are interchangeable and independent, typically chosen from the outside by the client.
- In the **State pattern**, concrete states represent the internal state of the context, and state transitions are frequently triggered by the states themselves as the context evolves.
</details>

<details>
<summary>4. Why should strategy methods receive all necessary data via arguments rather than accessing context properties?</summary>

Passing parameters explicitly keeps strategies stateless and decoupled from the internal structure of the context. This allows strategies to be safely reused across different contexts and avoids circular dependencies.
</details>

<details>
<summary>5. How can you create a registry of strategies that guarantees compile-time completeness?</summary>

By using a mapped type indexed by an exhaustive union:
```typescript
type StrategyName = 'email' | 'sms' | 'push';
type StrategyRegistry = Record<StrategyName, Strategy>;
```
If a new name is added to `StrategyName`, TypeScript flags any missing key in `StrategyRegistry`.
</details>

<details>
<summary>6. What is the difference between Strategy and Template Method?</summary>

Template Method uses inheritance: the base class defines an algorithm skeleton and subclasses override specific steps. Strategy uses composition: the context delegates the entire algorithm to an injected object or function.
</details>

<details>
<summary>7. How do you handle asynchronous strategies in TypeScript?</summary>

Declare the strategy interface method return type as `Promise<T>`:
```typescript
interface Strategy<T> { execute(): Promise<T>; }
```
The context `await`s the result of `execute()`.
</details>

<details>
<summary>8. Can higher-order functions be considered strategies?</summary>

Yes. A higher-order function that accepts a callback algorithm (such as `Array.prototype.sort((a, b) => a - b)` or `Array.prototype.filter(predicate)`) is a direct functional implementation of the Strategy pattern.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): String Formatter Strategy</summary>

**Problem:** Implement a `TextProcessor` class that accepts a string formatting strategy `(s: string) => string`, and provide uppercase and lowercase strategies.

**Solution:**
```typescript
export type TextStrategy = (s: string) => string;
export const uppercaseStrategy: TextStrategy = (s) => s.toUpperCase();
export const lowercaseStrategy: TextStrategy = (s) => s.toLowerCase();

export class TextProcessor {
  constructor(private strategy: TextStrategy) {}
  public format(input: string): string { return this.strategy(input); }
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Math Comparator Strategy</summary>

**Problem:** Create a sorting function that accepts a custom comparator strategy function `(a: number, b: number) => number`.

**Solution:**
```typescript
export function sortNumbers(
  arr: readonly number[],
  comparator: (a: number, b: number) => number
): number[] {
  return [...arr].sort(comparator);
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Caching Strategy Decorator</summary>

**Problem:** Write a higher-order function `withCache<TInput, TOutput>(strategy: (arg: TInput) => Promise<TOutput>): (arg: TInput) => Promise<TOutput>` that caches results based on stringified inputs.

**Solution:**
```typescript
export function withCache<TInput, TOutput>(
  strategy: (arg: TInput) => Promise<TOutput>
): (arg: TInput) => Promise<TOutput> {
  const cache = new Map<string, TOutput>();
  return async (arg: TInput) => {
    const key = JSON.stringify(arg);
    if (cache.has(key)) return cache.get(key)!;
    const res = await strategy(arg);
    cache.set(key, res);
    return res;
  };
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Pluggable Password Hash Strategy</summary>

**Problem:** Design an interface `PasswordHashStrategy` with `hash(plain: string): Promise<string>` and `verify(plain: string, hash: string): Promise<boolean>`, with an implementation for PBKDF2/Bcrypt.

**Solution:**
```typescript
export interface PasswordHashStrategy {
  hash(plain: string): Promise<string>;
  verify(plain: string, hash: string): Promise<boolean>;
}

export class MockBcryptStrategy implements PasswordHashStrategy {
  async hash(plain: string): Promise<string> { return `hashed_${plain}`; }
  async verify(plain: string, hash: string): Promise<boolean> { return hash === `hashed_${plain}`; }
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Type-Safe Strategy Map with Parameter Inference</summary>

**Problem:** Create a dispatcher that accepts a strategy map where each strategy requires different arguments, and a dispatch method that infers required arguments from the chosen strategy key.

**Solution:**
```typescript
export interface EventStrategyMap {
  login: (userId: string) => Promise<void>;
  purchase: (orderId: string, amount: number) => Promise<void>;
  logout: () => Promise<void>;
}

export class EventDispatcher {
  constructor(private strategies: EventStrategyMap) {}

  public async dispatch<K extends keyof EventStrategyMap>(
    event: K,
    ...args: Parameters<EventStrategyMap[K]>
  ): Promise<void> {
    const fn = this.strategies[event] as (...a: any[]) => Promise<void>;
    await fn(...args);
  }
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Fallback Retry Strategy Pipeline</summary>

**Problem:** Implement a composite strategy runner that attempts strategies in order, falling back to the next strategy if the previous one throws an error.

**Solution:**
```typescript
export class FallbackStrategyRunner<TInput, TOutput> {
  constructor(private strategies: ((input: TInput) => Promise<TOutput>)[]) {}

  public async run(input: TInput): Promise<TOutput> {
    let lastError: unknown;
    for (const strategy of this.strategies) {
      try {
        return await strategy(input);
      } catch (err) {
        lastError = err;
      }
    }
    throw new Error(`All strategies failed: ${String(lastError)}`);
  }
}
```
</details>
