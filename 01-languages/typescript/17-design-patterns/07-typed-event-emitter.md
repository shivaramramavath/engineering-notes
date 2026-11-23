# Typed Event Emitter and Observer Pattern in TypeScript

> Strongly-typed event emitters and the Observer pattern in TypeScript: strict event-to-payload mappings, memory leak prevention, Node.js EventEmitter typing, and RxJS-style reactive streams.

| Metadata | Details |
| :--- | :--- |
| **Category** | Design Patterns |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [02-functions/00-function-types.md](../02-functions/00-function-types.md), [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md), [07-utility-types/00-partial-required-readonly.md](../07-utility-types/00-partial-required-readonly.md) |
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

The **Observer Pattern** (and its asynchronous, decoupled variant, the **Publish/Subscribe** pattern) is a behavioral design pattern where an object (the Subject/Emitter) maintains a list of dependents (Observers/Listeners) and notifies them automatically of state changes or events.

In native JavaScript and Node.js `events.EventEmitter`, event names are untyped strings and listener arguments are untyped variadic arguments (`(...args: any[]) => void`). This causes severe developer friction:
1. **Typo Bugs**: Emitting `'userCreated'` while listening for `'user_created'` silently fails with zero compiler warnings.
2. **Payload Discrepancies**: The emitter sends `{ id: string, name: string }`, but the listener expects `(id: number)`. The discrepancy causes runtime crashes.
3. **Memory Leaks**: Forgetting to unsubscribe listeners causes the emitter to retain references indefinitely, preventing garbage collection.

TypeScript enables **Strictly Typed Event Emitters** where an event map interface explicitly defines all valid event names and their exact tuple or payload types.

```
+-----------------------------------------------------------+
|                     Typed EventEmitter                    |
|                                                           |
|  Event Map:                                               |
|  'user:login':  [userId: string, timestamp: number]       |
|  'user:logout': [userId: string]                          |
|  'error':       [err: Error]                              |
+-----------------------------------------------------------+
          |                                      ^
   emit('user:login', 'u_1', 123)       on('user:login', (id, ts) => ...)
   (Checked at compile time!)           (Inferred at compile time!)
```

---

## 2. Core Concepts

### 1. The Event Map Interface
The single source of truth describing every event and its listener signature:
```typescript
export interface AppEventMap {
  'connected': [];
  'data': [chunk: Buffer, encoding: string];
  'error': [error: Error];
  'close': [code: number, reason: string];
}
```

### 2. Generic Typed Emitter Interface
Using mapped types and keyof lookups to constrain `.on()` and `.emit()`:
```typescript
export interface TypedEmitter<TEvents extends Record<string, any[]>> {
  on<K extends keyof TEvents>(event: K, listener: (...args: TEvents[K]) => void): this;
  off<K extends keyof TEvents>(event: K, listener: (...args: TEvents[K]) => void): this;
  emit<K extends keyof TEvents>(event: K, ...args: TEvents[K]): boolean;
}
```

---

## 3. How It Works Internally

The TypeScript compiler uses mapped indexing:
When calling `emitter.emit('data', buffer, 'utf-8')`:
1. `K` is inferred as `'data'`.
2. `...args` is checked against `TEvents['data']`, which resolves to `[chunk: Buffer, encoding: string]`.
3. If an argument is missing or mistyped, `TS2554: Expected 2 arguments, but got 1` is emitted immediately.

```
         emit(event, ...args)
                   |
     Resolve K in keyof TEvents
                   |
   Extract tuple type TEvents[K]
                   |
   CheckSpreadArgs(args, TEvents[K])
```

---

## 4. Syntax & API Reference

### Complete Standalone Typed EventEmitter Implementation
```typescript
export class StrictEventEmitter<TEvents extends Record<string, any[]>> {
  private listeners = new Map<keyof TEvents, Set<(...args: any[]) => void>>();

  public on<K extends keyof TEvents>(event: K, listener: (...args: TEvents[K]) => void): () => void {
    if (!this.listeners.has(event)) {
      this.listeners.set(event, new Set());
    }
    this.listeners.get(event)!.add(listener);

    // Returns an unsubscription teardown function
    return () => this.off(event, listener);
  }

  public off<K extends keyof TEvents>(event: K, listener: (...args: TEvents[K]) => void): void {
    const set = this.listeners.get(event);
    if (set) {
      set.delete(listener);
      if (set.size === 0) this.listeners.delete(event);
    }
  }

  public emit<K extends keyof TEvents>(event: K, ...args: TEvents[K]): boolean {
    const set = this.listeners.get(event);
    if (!set || set.size === 0) return false;
    for (const listener of Array.from(set)) {
      listener(...args);
    }
    return true;
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic Custom Domain Event Emitter
```typescript
export interface UserEvents {
  signup: [userId: string, email: string];
  login: [userId: string, ipAddress: string];
  delete: [userId: string];
}

const userEmitter = new StrictEventEmitter<UserEvents>();

// Type-safe subscription:
const unsubscribe = userEmitter.on('signup', (id, email) => {
  console.log(`User ${id} registered with ${email}`);
});

// Type-safe emission:
userEmitter.emit('signup', 'u_101', 'alex@example.com');
```

### Example 2: Real-World Node.js `EventEmitter` Type Augmentation
```typescript
import { EventEmitter } from 'events';

export type TypedNodeEmitter<T extends Record<string, any[]>> = {
  on<K extends keyof T>(event: K, listener: (...args: T[K]) => void): TypedNodeEmitter<T>;
  once<K extends keyof T>(event: K, listener: (...args: T[K]) => void): TypedNodeEmitter<T>;
  emit<K extends keyof T>(event: K, ...args: T[K]): boolean;
  removeListener<K extends keyof T>(event: K, listener: (...args: T[K]) => void): TypedNodeEmitter<T>;
};

export function createTypedNodeEmitter<T extends Record<string, any[]>>(): TypedNodeEmitter<T> {
  return new EventEmitter() as unknown as TypedNodeEmitter<T>;
}
```

### Example 3: Edge Case - Asynchronous Event Emitter with Error Boundary
```typescript
export class AsyncEventEmitter<TEvents extends Record<string, any[]>> {
  private handlers = new Map<keyof TEvents, ((...args: any[]) => Promise<void>)[]>();

  public on<K extends keyof TEvents>(event: K, fn: (...args: TEvents[K]) => Promise<void>): void {
    if (!this.handlers.has(event)) this.handlers.set(event, []);
    this.handlers.get(event)!.push(fn);
  }

  public async emitParallel<K extends keyof TEvents>(event: K, ...args: TEvents[K]): Promise<void> {
    const list = this.handlers.get(event) ?? [];
    await Promise.all(list.map((fn) => fn(...args)));
  }

  public async emitSeries<K extends keyof TEvents>(event: K, ...args: TEvents[K]): Promise<void> {
    const list = this.handlers.get(event) ?? [];
    for (const fn of list) {
      await fn(...args);
    }
  }
}
```

### Example 4: Failure Case - Catching Event Typo and Argument Mismatch
```typescript
interface SocketEvents {
  message: [content: string, timestamp: number];
}

const socket = new StrictEventEmitter<SocketEvents>();

// @ts-expect-error TS2345: Argument of type '"msg"' is not assignable to parameter of type '"message"'.
socket.emit('msg', 'hello', Date.now());

// @ts-expect-error TS2554: Expected 3 arguments, but got 2. Missing 'timestamp'.
socket.emit('message', 'hello');
```

### Example 5: Idiomatic Observer with Subscription Tokens
```typescript
export interface IObserver<T> {
  next(value: T): void;
  error?(err: Error): void;
  complete?(): void;
}

export interface ISubscription {
  unsubscribe(): void;
}

export class ObservableSubject<T> {
  private observers = new Set<IObserver<T>>();

  public subscribe(observer: IObserver<T>): ISubscription {
    this.observers.add(observer);
    return {
      unsubscribe: () => {
        this.observers.delete(observer);
      }
    };
  }

  public next(value: T): void {
    for (const obs of this.observers) {
      obs.next(value);
    }
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Listener Memory Leaks**: In long-lived emitters (like web servers or web sockets), failing to remove event listeners when a connection closes causes uncollected memory leaks. Always return an `unsubscribe` callback.
2. **Re-entrant Emissions**: Emitting an event from inside a listener for that same event causes an infinite recursion stack overflow.
3. **Synchronous Exception Halts**: In standard Node `EventEmitter`, an unhandled error inside a listener halts execution and prevents subsequent listeners from executing. Consider wrapping listeners in try/catch or using `Promise.allSettled`.
4. **Mutating Listeners Array during Emit**: If a listener calls `.off()` to unsubscribe itself during execution of `.emit()`, iterating over the raw array can skip subsequent listeners. Always copy the listeners set (`Array.from(set)`) before iteration.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Loose String Events** | `emitter.on('msg', (data: any) => ...)` | Strongly-typed event map | Typos slip into production silently |
| **Missing Unsubscribe** | Calling `.on()` in React useEffect without cleanup | Return `unsubscribe()` in cleanup | Major memory leak and retained DOM closures |
| **Silent Exception Swallow** | Empty `catch {}` in async listeners | Global `'error'` event or structured log | Critical failures go unnoticed |
| **Mutating Array in Loop** | `for (let fn of this.listeners) fn();` | `for (let fn of [...this.listeners]) fn();` | Unsubscribing during emit skips listeners |
| **Passing Mutable Objects** | `emit('event', mutableState)` | `emit('event', Object.freeze({...state}))` | Observers mutate state behind each other's backs |
| **Unbounded Listeners** | Adding 1,000 listeners without warnings | Max listener limit warning | Hidden memory leak |

---

## 8. Performance & Type-Checker Complexity

- **Set vs Array for Listeners**: Using `Set<Function>` for listener storage provides $O(1)$ removal via `set.delete(listener)`, whereas `Array.splice` takes $O(N)$ time.
- **Type Checking**: Tuple types in event maps (`[string, number]`) check in sub-millisecond time and infer cleanly.

---

## 9. Compiler Internals

When evaluating variadic tuple types:
```typescript
emit<K extends keyof TEvents>(event: K, ...args: TEvents[K]): boolean;
```
The compiler unpacks the tuple `TEvents[K]` into named positional parameters in the generated signature shown in IDE tooltips, ensuring accurate autocomplete for each argument name.

---

## 10. Best Practices & Design Guidelines

1. **Always Return an Unsubscribe Closure**: Design `.on()` to return `() => void` so consumers can tear down cleanly with one line.
2. **Snapshot Listener Collections**: Clone listeners before iteration to prevent bugs when listeners unsubscribe during emit.
3. **Freeze Event Payloads**: Emit immutable payloads using `Object.freeze()` to prevent observers from altering data.
4. **Differentiate Sync vs Async**: Never mix synchronous and asynchronous listener expectations; pick either immediate dispatch or promise-settled dispatch.

---

## 11. Real-World Case Study

### WebSocket Real-Time Chat Room Gateway
A WebSocket server coordinates real-time user joins, chats, and typing notifications:

```typescript
export interface ChatRoomEvents {
  'user:joined': [userId: string, timestamp: number];
  'user:left': [userId: string];
  'message:new': [messageId: string, authorId: string, text: string];
  'user:typing': [userId: string, isTyping: boolean];
}

export class ChatRoom {
  private events = new StrictEventEmitter<ChatRoomEvents>();

  public on<K extends keyof ChatRoomEvents>(
    event: K,
    listener: (...args: ChatRoomEvents[K]) => void
  ): () => void {
    return this.events.on(event, listener);
  }

  public broadcastMessage(messageId: string, authorId: string, text: string): void {
    this.events.emit('message:new', messageId, authorId, text);
  }

  public userJoin(userId: string): void {
    this.events.emit('user:joined', userId, Date.now());
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. How do you implement a strongly-typed EventEmitter in TypeScript without external libraries?</summary>

By defining an event map interface where keys are event names and values are tuple parameter lists:
```typescript
interface EventMap { 'data': [Buffer]; 'error': [Error]; }
```
Then parameterize the emitter class:
```typescript
class Emitter<T extends Record<string, any[]>> {
  on<K extends keyof T>(event: K, listener: (...args: T[K]) => void): void;
  emit<K extends keyof T>(event: K, ...args: T[K]): boolean;
}
```
</details>

<details>
<summary>2. Why does modifying the listener array during `emit` cause bugs, and how do you prevent it?</summary>

If a listener removes itself using `.off()` while the loop is running, the array length shrinks and subsequent elements shift down by one index, causing the loop to skip the next listener. This is solved by creating a snapshot before iterating: `for (const fn of Array.from(this.listeners))`.
</details>

<details>
<summary>3. What is the difference between the Observer pattern and the Publish/Subscribe (PubSub) pattern?</summary>

In the **Observer pattern**, the Subject holds direct references to its observers and notifies them directly. In the **PubSub pattern**, publishers and subscribers never know about each other; they communicate exclusively through an intermediate message broker or event bus.
</details>

<details>
<summary>4. Why should `.on()` return an unsubscribe function instead of forcing callers to invoke `.off()`?</summary>

Returning an `unsubscribe` function encapsulates both the event name and the function reference, which is especially useful when listeners are anonymous arrow functions or when cleaning up inside React `useEffect` hooks.
</details>

<details>
<summary>5. How can you implement `once(event, listener)`?</summary>

By wrapping the original listener in a wrapper function that first unsubscribes itself and then calls the original listener:
```typescript
public once<K extends keyof T>(event: K, listener: (...args: T[K]) => void) {
  const unsubscribe = this.on(event, (...args) => {
    unsubscribe();
    listener(...args);
  });
}
```
</details>

<details>
<summary>6. What causes memory leaks in Node.js EventEmitters?</summary>

Adding listeners without removing them. When a listener closure captures local variables (e.g., HTTP request/response objects or DOM elements), the emitter keeps the entire closure in memory, preventing garbage collection.
</details>

<details>
<summary>7. How do you handle errors thrown inside event listeners in asynchronous emitters?</summary>

Use `Promise.allSettled(listeners.map(fn => fn(...args)))` to ensure that even if one listener throws or rejects, all other listeners execute, and all collected errors can be aggregated into an `AggregateError`.
</details>

<details>
<summary>8. How do you make event payloads immutable?</summary>

Wrap emitted payloads in `Readonly<T>` at the type level and `Object.freeze()` at runtime to prevent listeners from modifying shared state.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic `once()` Implementation</summary>

**Problem:** Add a `.once(event, listener)` method to `StrictEventEmitter`.

**Solution:**
```typescript
public once<K extends keyof TEvents>(event: K, listener: (...args: TEvents[K]) => void): void {
  const teardown = this.on(event, (...args) => {
    teardown();
    listener(...args);
  });
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Event Count Tracker</summary>

**Problem:** Add a method `listenerCount(event: string): number` returning the number of active listeners for an event.

**Solution:**
```typescript
public listenerCount<K extends keyof TEvents>(event: K): number {
  return this.listeners.get(event)?.size ?? 0;
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Max Listeners Warning</summary>

**Problem:** Implement a `setMaxListeners(n: number)` method that logs a console warning if active listeners exceed the threshold.

**Solution:**
```typescript
export class CheckedEmitter<T extends Record<string, any[]>> extends StrictEventEmitter<T> {
  private maxListeners = 10;
  public override on<K extends keyof T>(event: K, listener: (...args: T[K]) => void): () => void {
    const unsub = super.on(event, listener);
    if (this.listenerCount(event) > this.maxListeners) {
      console.warn(`MaxListenersExceededWarning: Possible memory leak detected on event "${String(event)}"`);
    }
    return unsub;
  }
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Wildcard Event Emitter</summary>

**Problem:** Implement a special `'*'` event listener that triggers on any emitted event, passing the event name as the first argument.

**Solution:**
```typescript
export class WildcardEmitter<T extends Record<string, any[]>> extends StrictEventEmitter<T> {
  private wildcardListeners = new Set<(event: keyof T, ...args: any[]) => void>();

  public onAny(listener: (event: keyof T, ...args: any[]) => void): () => void {
    this.wildcardListeners.add(listener);
    return () => this.wildcardListeners.delete(listener);
  }

  public override emit<K extends keyof T>(event: K, ...args: T[K]): boolean {
    const result = super.emit(event, ...args);
    for (const wl of Array.from(this.wildcardListeners)) {
      wl(event, ...args);
    }
    return result;
  }
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Asynchronous Parallel Event Bus with Timeouts</summary>

**Problem:** Build an async emitter where `emitAsync(event, ...args, timeoutMs)` runs all listeners concurrently, rejecting if any listener exceeds the timeout.

**Solution:**
```typescript
export class TimeoutAsyncEmitter<T extends Record<string, any[]>> {
  private listeners = new Map<keyof T, ((...args: any[]) => Promise<void>)[]>();

  public on<K extends keyof T>(event: K, fn: (...args: T[K]) => Promise<void>): void {
    if (!this.listeners.has(event)) this.listeners.set(event, []);
    this.listeners.get(event)!.push(fn);
  }

  public async emitWithTimeout<K extends keyof T>(event: K, timeoutMs: number, ...args: T[K]): Promise<void> {
    const list = this.listeners.get(event) ?? [];
    const timeoutPromise = new Promise<never>((_, rej) =>
      setTimeout(() => rej(new Error(`Event ${String(event)} timed out after ${timeoutMs}ms`)), timeoutMs)
    );
    await Promise.race([Promise.all(list.map((fn) => fn(...args))), timeoutPromise]);
  }
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Type-Safe RxJS-Like Pipeable Observable</summary>

**Problem:** Implement a minimal observable class with a `.pipe(operator)` method that can transform emitted values.

**Solution:**
```typescript
export type Operator<A, B> = (source: MiniObservable<A>) => MiniObservable<B>;

export class MiniObservable<T> {
  constructor(private subscribeFn: (observer: (val: T) => void) => () => void) {}

  public subscribe(observer: (val: T) => void): () => void {
    return this.subscribeFn(observer);
  }

  public pipe<R>(op: Operator<T, R>): MiniObservable<R> {
    return op(this);
  }
}

export function map<T, R>(fn: (val: T) => R): Operator<T, R> {
  return (source) => new MiniObservable((observer) =>
    source.subscribe((val) => observer(fn(val)))
  );
}
```
</details>
