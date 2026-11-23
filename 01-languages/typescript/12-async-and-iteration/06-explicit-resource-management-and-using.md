# Explicit Resource Management and Using in TypeScript

> Comprehensive guide to TypeScript 5.2+ Explicit Resource Management: `using` declarations, `await using`, `Disposable`, `AsyncDisposable`, `Symbol.dispose`, `Symbol.asyncDispose`, `DisposableStack`, and deterministic lifecycle cleanup.

| Metadata | Details |
| :--- | :--- |
| **Category** | Async & Iteration |
| **Difficulty** | Advanced |
| **Prerequisites** | [12-async-and-iteration/01-promises.md](./01-promises.md), [12-async-and-iteration/02-async-await.md](./02-async-await.md) |
| **TypeScript Baseline** | TS 5.2+ (Explicit Resource Management proposal Stage 3) |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "lib": ["ES2022", "ESNext.Disposable"]` |

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
Software frequently allocates non-memory resources that require deterministic, immediate cleanup: database connections, file handles, mutual exclusion locks, transaction boundaries, and temporary directories. Historically in JavaScript, developers relied on `try / finally` blocks to close resources. However, complex multi-resource initialization led to error-prone nested `try/finally` ladders (the "pyramid of cleanup").

TypeScript 5.2 introduced **Explicit Resource Management** (based on the TC39 Stage 3 proposal). Using the `using` and `await using` keywords, variables declare their own cleanup lifecycles. When execution leaves the containing lexical block (whether normally or via thrown exception), TypeScript automatically invokes `[Symbol.dispose]()` or `[Symbol.asyncDispose]()` in reverse declaration order, guaranteeing clean teardown with zero boilerplates.

---

## 2. Core Concepts
- **`using` Declarations**: Declares a variable holding an object implementing the `Disposable` interface (`[Symbol.dispose](): void`). The disposal method is called when leaving the block.
- **`await using` Declarations**: Declares an asynchronous resource implementing `AsyncDisposable` (`[Symbol.asyncDispose](): Promise<void>`). The disposal method is awaited when leaving the block.
- **`Symbol.dispose` and `Symbol.asyncDispose`**: Well-known symbols added to the global `Symbol` namespace representing resource disposal methods.
- **`DisposableStack` and `AsyncDisposableStack`**: Built-in container classes that aggregate multiple disposable resources and clean them up deterministically.
- **Disposal Ordering**: Resources are disposed in **LIFO (Last-In, First-Out)** reverse declaration order (analogous to C++ destructors or Go's `defer`).
- **Suppressed Errors (`SuppressedError`)**: If an error is thrown both inside the block AND during disposal, JavaScript wraps them together in a `SuppressedError` rather than swallowing either exception.

---

## 3. How It Works Internally

```
{
  using resA = acquireResourceA();
  using resB = acquireResourceB();

  // Execute operations on resA and resB
} // <--- Lexical scope exit point!
  // 1. Calls resB[Symbol.dispose]()
  // 2. Calls resA[Symbol.dispose]()
```

When downleveling, the TypeScript compiler surrounds the containing block in a synthetic `try ... finally` wrapper managed by a generated stack. If `resB[Symbol.dispose]()` throws an error while an uncaught exception is already propagating from the body, the runtime constructs a new `SuppressedError(error, suppressed)` to preserve both stack traces.

---

## 4. Syntax & API Reference

```typescript
// 1. Defining a synchronous Disposable resource
class TempDirectory implements Disposable {
  constructor(public readonly path: string) {}

  [Symbol.dispose](): void {
    console.log(`Deleting temporary directory at ${this.path}`);
  }
}

// 2. Defining an asynchronous AsyncDisposable resource
class DatabaseSession implements AsyncDisposable {
  async [Symbol.asyncDispose](): Promise<void> {
    console.log('Closing database connection pool');
  }
}

// 3. Using in a function scope
function doWork(): void {
  using tempDir = new TempDirectory('/tmp/work_123');
  // tempDir[Symbol.dispose]() runs when doWork exits!
}

async function doAsyncWork(): Promise<void> {
  await using db = new DatabaseSession();
  // await db[Symbol.asyncDispose]() runs when doAsyncWork exits!
}
```

---

## 5. Practical Examples

### Example 1: Deterministic File Handle Locker
```typescript
class FileLock implements Disposable {
  private locked = false;

  constructor(public readonly filename: string) {
    console.log(`Acquiring lock on ${filename}`);
    this.locked = true;
  }

  [Symbol.dispose](): void {
    if (this.locked) {
      console.log(`Releasing lock on ${this.filename}`);
      this.locked = false;
    }
  }
}

function processSensitiveFile(path: string): void {
  using lock = new FileLock(path);
  // Do file operations; lock is released even if an error is thrown here!
}
```

### Example 2: Transaction Boundary with AsyncDisposable
```typescript
interface Client {
  query(sql: string): Promise<void>;
  rollback(): Promise<void>;
  commit(): Promise<void>;
}

class TransactionScope implements AsyncDisposable {
  private committed = false;

  constructor(private client: Client) {}

  markSuccessful(): void {
    this.committed = true;
  }

  async [Symbol.asyncDispose](): Promise<void> {
    if (!this.committed) {
      console.warn('Transaction aborted: rolling back');
      await this.client.rollback();
    } else {
      console.log('Transaction committed successfully');
      await this.client.commit();
    }
  }
}

async function transferFunds(client: Client): Promise<void> {
  await using tx = new TransactionScope(client);
  await client.query('DEBIT 100');
  await client.query('CREDIT 100');
  tx.markSuccessful(); // Without this, auto-rollback occurs!
}
```

### Example 3: Edge Case - Handling SuppressedError
```typescript
// If both the try block and the dispose handler throw:
class FaultyResource implements Disposable {
  [Symbol.dispose](): void {
    throw new Error('Disposal failed');
  }
}

try {
  using res = new FaultyResource();
  throw new Error('Body failed');
} catch (e: unknown) {
  if (typeof SuppressedError !== 'undefined' && e instanceof SuppressedError) {
    console.error('Primary error:', e.error.message);        // "Disposal failed"
    console.error('Suppressed error:', e.suppressed.message); // "Body failed"
  }
}
```

### Example 4: Compiler Failure Case - Non-Disposable in `using`
```typescript
class PlainService {
  cleanup(): void {}
}

function test() {
  // @ts-expect-error TS2840: The resource must implement the 'Disposable' interface.
  using service = new PlainService();
}
```

### Example 5: Idiomatic DisposableStack Pattern
```typescript
export function executeWithResources(): void {
  using stack = new DisposableStack();

  // Register multiple cleanups on demand
  stack.defer(() => console.log('Cleanup 1'));
  stack.defer(() => console.log('Cleanup 2'));

  // Use stack to adopt resources
  const dir = stack.use(new TempDirectory('/tmp/cache'));
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Requires Polyfills**: `Symbol.dispose` and `Symbol.asyncDispose` are not defined in older JavaScript runtimes (pre-Node 20 or older browsers). You must include `core-js` or define polyfills:
   ```typescript
   (Symbol as any).dispose ??= Symbol('dispose');
   (Symbol as any).asyncDispose ??= Symbol('asyncDispose');
   ```
2. **`using` Cannot Be Re-assigned**: Variables declared with `using` are `const`-like and cannot be re-assigned.
3. **`using` at Top Level**: Cannot be used without an enclosing block or module scope.
4. **Disposal Order is LIFO**: The last resource declared is the first resource disposed. If Resource B depends on Resource A during its disposal, declare Resource A *before* Resource B.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| `using x = obj;` where `obj` has no `[Symbol.dispose]` | **TS2840**: Does not implement Disposable | Implement `[Symbol.dispose]()` |
| Calling `await using` in a synchronous function | **TS2841**: 'await using' requires async function | Mark containing function `async` |
| Forgetting `Symbol.dispose` polyfill | `undefined` symbol at runtime; cleanup never executes | Polyfill `Symbol.dispose` at app entry |
| Manually calling `res[Symbol.dispose]()` | Double disposal | Let scope exit invoke disposal automatically |
| Re-assigning `using` variable | **TS2842**: Cannot assign to 'using' declaration | Declare as separate resource or use `DisposableStack` |

---

## 8. Performance & Type-Checker Complexity
- Transpiled `using` declarations generate lightweight inline `try/finally` blocks with zero allocation overhead.
- The TypeScript type checker enforces `Disposable` or `AsyncDisposable` compliance through straightforward property lookup on `Symbol.dispose`, running in $\mathcal{O}(1)$ time.

---

## 9. Compiler Internals
TypeScript 5.2 added the `UsingDeclaration` AST syntax node. The binder checks if the variable declaration contains the `using` or `await using` modifier flags. The type checker enforces that the initializer type contains a callable member whose name is `[Symbol.dispose]` or `[Symbol.asyncDispose]`.

---

## 10. Best Practices & Design Guidelines
- Add `"lib": ["ES2022", "ESNext.Disposable"]` to `tsconfig.json` when using TypeScript 5.2+.
- Always polyfill `Symbol.dispose` and `Symbol.asyncDispose` at the entry point of your application until Node.js and browsers natively ship global support.
- Make disposers idempotent: calling `[Symbol.dispose]()` multiple times should have no adverse side-effects.
- Use `DisposableStack` when resources are acquired dynamically inside loops or conditional branches.
- Use `await using` whenever cleanup involves network I/O or database queries.

---

## 11. Real-World Case Study
A distributed file indexing daemon generated hundreds of temporary unpack directories on disk. When background unzip tasks failed due to corrupt archives, the `catch` blocks failed to clean up the temporary disk directories, filling up root partition storage and taking the cluster offline.

**Refactored with `using`:**
```typescript
class TempWorkspace implements Disposable {
  constructor(public readonly dirPath: string) {
    fs.mkdirSync(dirPath, { recursive: true });
  }

  [Symbol.dispose](): void {
    if (fs.existsSync(this.dirPath)) {
      fs.rmSync(this.dirPath, { recursive: true, force: true });
      console.log(`Pruned ${this.dirPath}`);
    }
  }
}

export function indexArchive(archivePath: string): void {
  using workspace = new TempWorkspace(`/tmp/archive_${Date.now()}`);
  unpackAndIndex(archivePath, workspace.dirPath);
  // Directory is guaranteed to be deleted even if unpackAndIndex throws!
}
function unpackAndIndex(_a: string, _d: string) { /* ... */ }
```
Disk leakage was completely eliminated.

---

## 12. Interview Questions

<details>
<summary>1. What is Explicit Resource Management in TypeScript 5.2?</summary>

It is a language feature based on a TC39 Stage 3 proposal that adds `using` and `await using` declarations to deterministically clean up resources (by invoking `Symbol.dispose` or `Symbol.asyncDispose`) when leaving lexical scope.
</details>

<details>
<summary>2. In what order are multiple `using` variables disposed?</summary>

In reverse order of declaration (LIFO - Last In, First Out). The variable declared last is disposed first.
</details>

<details>
<summary>3. What is a `SuppressedError`?</summary>

It is a special error class instantiated when an exception is thrown inside a block and another exception is thrown while running the disposer for that block. The disposal error is the primary error, and the original block error is stored on `.suppressed`.
</details>

<details>
<summary>4. What compiler configuration is required to use `using`?</summary>

TypeScript 5.2 or later, with `"lib"` containing `"ESNext.Disposable"` (or targeting `ESNext`), and runtime polyfills for `Symbol.dispose` if running on runtimes that lack native support.
</details>

<details>
<summary>5. How does `await using` differ from `using`?</summary>

`using` calls synchronous `[Symbol.dispose]()` and can be used in any function. `await using` calls asynchronous `[Symbol.asyncDispose]()`, awaits the returned promise, and can only be used inside `async` functions or top-level module code.
</details>

<details>
<summary>6. What is `DisposableStack`?</summary>

It is a standard utility class that manages a collection of disposable resources and custom teardown functions, providing `.use(resource)` and `.defer(callback)` methods to clean up everything when the stack itself is disposed.
</details>

<details>
<summary>7. Can you reassign a `using` variable?</summary>

No. Variables declared with `using` are immutable bindings, similar to `const`.
</details>

<details>
<summary>8. How do you implement custom cleanup for an existing third-party object that doesn't implement `Disposable`?</summary>

Using `DisposableStack`:
```typescript
using stack = new DisposableStack();
stack.defer(() => legacyClient.destroy());
```
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Basic Disposable Class**: Create a `MemoryBuffer` class implementing `Disposable` that sets its `length` property to 0 on dispose.
   <details>
   <summary>Solution</summary>

   ```typescript
   export class MemoryBuffer implements Disposable {
     constructor(public length: number) {}

     [Symbol.dispose](): void {
       this.length = 0;
     }
   }
   ```
   </details>

2. **Scoped Benchmark Timer**: Implement a `ScopeTimer` class implementing `Disposable` that logs the elapsed time when the block exits.
   <details>
   <summary>Solution</summary>

   ```typescript
   export class ScopeTimer implements Disposable {
     private start = performance.now();
     constructor(private label: string) {}

     [Symbol.dispose](): void {
       const duration = (performance.now() - this.start).toFixed(2);
       console.log(`[${this.label}] took ${duration}ms`);
     }
   }
   ```
   </details>

### Medium
3. **Async Disposable Mutex**: Implement an `AsyncMutex` where calling `acquire()` returns an object implementing `AsyncDisposable` that releases the mutex on scope exit.
   <details>
   <summary>Solution</summary>

   ```typescript
   export class AsyncMutex {
     private locked = false;
     private queue: Array<() => void> = [];

     async acquire(): Promise<AsyncDisposable> {
       if (this.locked) {
         await new Promise<void>(res => this.queue.push(res));
       }
       this.locked = true;

       return {
         [Symbol.asyncDispose]: async () => {
           if (this.queue.length > 0) {
             const next = this.queue.shift()!;
             next();
           } else {
             this.locked = false;
           }
         },
       };
     }
   }
   ```
   </details>

4. **Dynamic Cleanup Stack**: Write a function that processes an array of database rows, opens a temporary file for each row, and ensures all file handles are closed using a single `DisposableStack`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function processRows(rows: string[]): void {
     using stack = new DisposableStack();

     for (const row of rows) {
       stack.defer(() => console.log(`Cleaned file for row: ${row}`));
     }
     // All files are closed when processRows exits
   }
   ```
   </details>

### Hard
5. **Polyfill Package for Explicit Resource Management**: Write a standalone polyfill script that safely attaches `Symbol.dispose`, `Symbol.asyncDispose`, and minimal `DisposableStack` polyfills to the global environment if they do not exist.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function setupDisposablePolyfills(): void {
     (Symbol as any).dispose ??= Symbol.for('dispose');
     (Symbol as any).asyncDispose ??= Symbol.for('asyncDispose');

     if (typeof (globalThis as any).DisposableStack === 'undefined') {
       (globalThis as any).DisposableStack = class {
         private cleanups: Array<() => void> = [];

         use<T extends Disposable>(resource: T): T {
           this.defer(() => resource[Symbol.dispose]());
           return resource;
         }

         defer(callback: () => void): void {
           this.cleanups.push(callback);
         }

         [Symbol.dispose](): void {
           while (this.cleanups.length > 0) {
             const cleanup = this.cleanups.pop()!;
             cleanup();
           }
         }
       };
     }
   }
   ```
   </details>

6. **Self-Healing Resource Pool with Auto-Disposal**: Implement a generic `ResourcePool<T>` with a method `withResource<R>(action: (res: T) => Promise<R>): Promise<R>` that acquires a resource from the pool, wraps it in an `AsyncDisposable` that returns the resource to the pool upon completion, even during errors.
   <details>
   <summary>Solution</summary>

   ```typescript
   export class ResourcePool<T> {
     private available: T[] = [];

     constructor(items: T[]) {
       this.available = [...items];
     }

     async withResource<R>(action: (res: T) => Promise<R>): Promise<R> {
       if (this.available.length === 0) {
         throw new Error('No resources available in pool');
       }
       const resource = this.available.pop()!;

       await using handle: AsyncDisposable = {
         [Symbol.asyncDispose]: async () => {
           this.available.push(resource);
         },
       };

       return await action(resource);
     }
   }
   ```
   </details>
