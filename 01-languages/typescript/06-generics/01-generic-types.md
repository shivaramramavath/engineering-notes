# Generic Types

> Generic types parameterize interfaces, type aliases, and classes over type arguments, enabling expressive, reusable data structures and domain models.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00-generic-functions.md](file:///E:/Learning/engineering-notes/01-languages/typescript/06-generics/00-generic-functions.md), [04-type-aliases.md](file:///E:/Learning/engineering-notes/01-languages/typescript/01-fundamentals/04-type-aliases.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noImplicitAny: true` |

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
- **Problem Statement:** Data structures in enterprise applications (such as paginated API responses, Result/Either monads, linked lists, key-value caches, and state containers) share common container logic, but hold different payload types. Without generic types, developers must create duplicate types (`UserPagination`, `OrderPagination`, `ProductPagination`), bloating codebases and increasing maintenance overhead.
- **Historical Context:** TypeScript has supported generic interfaces and classes since 0.9, and generic type aliases since 1.4. Generic types bring functional type theory (parametric polymorphism) into everyday TypeScript application engineering.
- **Real-World Relevance:** Modern libraries and frameworks rely entirely on generic types: `Promise<T>`, `Array<T>`, `Record<K, V>`, `Ref<T>` (Vue), `Observable<T>` (RxJS), and `Result<T, E>` (neverthrow).

---

## 2. Core Concepts
- **Formal Invariants:**
  - Type Constructor: A generic type is not a concrete type itself; it is a "type constructor" (a function at the type level) that takes type arguments and produces a concrete type.
  - Instantiation: Applying type arguments to a generic type produces a `TypeReference` (`Box<string>`).
  - Scoping: Type parameters declared on a generic type (`interface Container<T>`) are in scope across all properties and method signatures of that declaration.
  - Invariance in Static Members: In generic classes, type parameters are NOT in scope for `static` members because static members belong to the constructor function, not individual generic instances.
- **Mental Model:** Think of a generic type as a mold. You pour a concrete type (like `User`) into the mold (like `PaginatedResponse<T>`), and it produces a custom tailored shape (`PaginatedResponse<User>`).
- **Soundness vs Ergonomics:** TypeScript evaluates structural compatibility between generic instantiations by comparing their resolved structural members, not their type argument names.

---

## 3. How It Works Internally
- **Type Reference Creation:** In `checker.ts`, referencing `Box<string>` allocates a `TypeReference` object with `target: Box` and `typeArguments: [string]`.
- **Lazy Member Resolution:** The members of a generic type instantiation are resolved lazily on demand when a property access is checked.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                  Generic Type Instantiation in Compiler                  |
|                                                                          |
|   Generic Declaration:                                                   |
|   interface Result<T, E> {                                               |
|     ok: boolean;                                                         |
|     value?: T;                                                           |
|     error?: E;                                                           |
|   }                                                                      |
|                                                                          |
|   Instantiation: Result<User, Error>                                     |
|          │                                                               |
|          ▼                                                               |
|   TypeReference Node:                                                    |
|   ├── Target: Symbol(Result)                                             |
|   ├── Type Arguments: [ Type(User), Type(Error) ]                        |
|   └── Evaluated Members (Lazy):                                          |
|       ├── ok: boolean                                                    |
|       ├── value?: User                                                   |
|       └── error?: Error                                                  |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Generic Interface:**
  ```typescript
  interface KeyValuePair<K, V> {
    key: K;
    value: V;
  }
  ```
- **Generic Type Alias:**
  ```typescript
  type Nullable<T> = T | null;
  type Result<T, E = Error> =
    | { ok: true; value: T }
    | { ok: false; error: E };
  ```
- **Generic Class:**
  ```typescript
  class Box<T> {
    constructor(public content: T) {}
  }
  ```
- **tsconfig Dependencies:** Operates under standard configurations.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Model a standard paginated response envelope.
- **Code:**
```typescript
export interface Paginated<T> {
  items: T[];
  totalCount: number;
  page: number;
  pageSize: number;
  hasMore: boolean;
}

type User = { id: string; name: string };
type Product = { sku: string; priceCents: number };

const userPage: Paginated<User> = {
  items: [{ id: 'u1', name: 'Alice' }],
  totalCount: 100,
  page: 1,
  pageSize: 1,
  hasMore: true,
};

const productPage: Paginated<Product> = {
  items: [{ sku: 'PROD-99', priceCents: 1500 }],
  totalCount: 50,
  page: 1,
  pageSize: 1,
  hasMore: true,
};
```
- **Output / Type Evaluation:**
```typescript
// userPage.items is User[]; productPage.items is Product[]
```
- **Explanation:** A single generic interface models all pagination responses across the enterprise.

### Example 2: Real-World Enterprise Pattern (Result Monad for Error Handling)
- **Goal:** Eliminate throwing exceptions in business logic using a generic Result type.
- **Code:**
```typescript
export type Result<T, E = Error> =
  | { readonly ok: true; readonly value: T }
  | { readonly ok: false; readonly error: E };

export function Ok<T>(value: T): Result<T, never> {
  return { ok: true, value };
}

export function Err<E>(error: E): Result<never, E> {
  return { ok: false, error };
}

function parsePort(input: string): Result<number, string> {
  const port = parseInt(input, 10);
  if (isNaN(port) || port < 1 || port > 65535) {
    return Err(`Invalid port: ${input}`);
  }
  return Ok(port);
}

const res = parsePort('8080');
if (res.ok) {
  console.log('Valid port:', res.value); // number
} else {
  console.error('Failure:', res.error); // string
}
```
- **Output / Type Evaluation:**
```typescript
// Discriminated union narrows cleanly based on ok boolean flag.
```
- **Explanation:** Generic Result types model synchronous and asynchronous errors with compile-time exhaustiveness checking.

### Example 3: Edge Case / Boundary Condition (Static Members Cannot Reference Class Type Parameters)
- **Goal:** Understand why static members cannot use class type parameters.
- **Code:**
```typescript
class CacheManager<T> {
  private cache = new Map<string, T>();

  // Instance methods CAN reference T:
  set(key: string, item: T): void {
    this.cache.set(key, item);
  }

  // Static methods CANNOT reference class type parameter T!
  // @ts-expect-error TS2302: Static members cannot reference class type parameters.
  static defaultItem: T;

  // Idiomatic: Make the static method generic itself!
  static createEmpty<V>(): CacheManager<V> {
    return new CacheManager<V>();
  }
}
```
- **Output / Type Evaluation:**
```typescript
// TS2302 prevents referencing instance type parameters in static value space.
```
- **Explanation:** Static members belong to the single constructor function object, which is not parameterized per instance.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate error when required generic type argument is omitted.
- **Code:**
```typescript
interface Container<T> {
  data: T;
}

// @ts-expect-error TS2314: Generic type 'Container<T>' requires 1 type argument(s).
const item: Container = { data: 123 };
```
- **Real Compiler Error:**
```
error TS2314: Generic type 'Container<T>' requires 1 type argument(s).
```
- **Explanation:** Generic types without defaults require explicit type arguments when referenced as type annotations.

### Example 5: Idiomatic Modern Usage (Generic State Container with Immutable Transitions)
- **Goal:** Build an observable state store preserving immutable state transitions.
- **Code:**
```typescript
export class Store<TState extends object> {
  #state: TState;
  #listeners = new Set<(state: TState) => void>();

  constructor(initialState: TState) {
    this.#state = Object.freeze({ ...initialState });
  }

  getState(): TState {
    return this.#state;
  }

  setState(updater: (prev: TState) => Partial<TState>): void {
    const update = updater(this.#state);
    this.#state = Object.freeze({ ...this.#state, ...update });
    for (const listener of this.#listeners) {
      listener(this.#state);
    }
  }

  subscribe(listener: (state: TState) => void): () => void {
    this.#listeners.add(listener);
    return () => this.#listeners.delete(listener);
  }
}

type AppState = { user: string; theme: 'dark' | 'light' };
const store = new Store<AppState>({ user: 'Alice', theme: 'dark' });

store.setState((prev) => ({ theme: 'light' }));
console.log(store.getState()); // { user: "Alice", theme: "light" }
```
- **Output / Type Evaluation:**
```typescript
// End-to-end type safety on store updates and state reads.
```
- **Explanation:** Generic state containers enforce type consistency across updates and subscriptions.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Structural Equivalence on Empty Generic Types:** If an interface declares a generic parameter that is never used in its members (`interface Empty<T> {}`), then `Empty<string>` and `Empty<number>` are 100% structurally assignable to each other. TypeScript checks the members, not the type arguments.
2. **Recursive Generic Types:** When modeling recursive types (`type Tree<T> = { value: T; children: Tree<T>[] }`), indirection through an array or object property is required to avoid infinite instantiation depth.
3. **Default Type Argument Order:** In type parameter lists (`<T, U = string>`), required type parameters cannot follow optional type parameters with defaults.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Unused generic type parameters | `Empty<A>` structurally matches `Empty<B>` | Use type parameter in properties | Ensures real type distinction |
| Referencing class generic in static member | `TS2302: Static members cannot reference T` | Make static method independently generic | Static members belong to constructor |
| Omitting type argument without default | `TS2314: Generic type requires 1 type argument` | Provide type argument or add default | Resolves type constructor to concrete type |
| Placing optional generic before required | `TS2706: Required type parameter after optional` | Move defaults to the end | Enforces positional type argument order |
| Overusing complex nested generic types | Slows compiler; unreadable hover tooltips | Decompose into smaller named types | Improves maintainability and compiler speed |
| Throwing errors instead of Result types | Unhandled runtime rejections in production | Use generic `Result<T, E>` | Forces compile-time error handling |

---

## 8. Performance & Type-Checker Complexity
- **Type Instantiation Memory:** Each distinct instantiation (`Box<string>`, `Box<number>`) creates a cached `TypeReference`. Highly generative types (e.g. nested mapped types producing thousands of variants) consume compiler memory.
- **Structural Comparison:** When checking `A<T> = B<U>`, the compiler performs variance checks before falling back to full member comparison.
- **Isolated Declarations:** Exported generic types must have explicit annotations under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Nodes:** `ts.InterfaceDeclaration`, `ts.TypeAliasDeclaration`, `ts.ClassDeclaration` containing `typeParameters`.
- **Type Checker Representation:** Instantiations are represented by `TypeReference` objects where `flags & TypeFlags.Object` and `objectFlags & ObjectFlags.Reference`.

---

## 10. Best Practices & Design Guidelines
- **Use Meaningful Generic Parameter Names:** Use `TData`, `TError`, `TKey` instead of single letters when modeling complex multi-parameter types.
- **Provide Sensible Defaults:** Provide defaults for secondary parameters (`<T, E = Error>`).
- **Leverage Discriminated Unions for Monadic Types:** Always use a discriminant like `ok: true | false` for Result and Either types.

---

## 11. Real-World Case Study
- **Scenario:** A financial reporting platform defined separate response types for 45 microservices: `InvoicePage`, `TransactionPage`, `UserPage`, `AuditPage`. When the API gateway introduced cursor-based pagination (`nextCursor: string | null`), engineers had to modify 45 separate files, and 8 files were missed, causing client rendering bugs in production.
- **Implementation:** Unified all responses using a single generic `CursorPage<T>`:
  ```typescript
  export interface CursorPage<T> {
    data: T[];
    nextCursor: string | null;
    total: number;
  }
  ```
- **Outcome:** Reduced duplicate interface declarations by 1,200 lines, and guaranteed that any future pagination protocol change updates 100% of services simultaneously.

---

## 12. Interview Questions

### Q1: What is a type constructor in type theory and how does TypeScript implement it?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In type theory, a type constructor is an abstraction that takes one or more types as arguments and returns a new type (an analogy to functions in value space). In TypeScript, any generic interface, class, or type alias (e.g. `List<T>` or `Promise<T>`) is a type constructor. `List` by itself cannot be used to type a runtime variable; it must be instantiated with a type argument (such as `List<string>`) to produce a concrete type.
</details>

### Q2: Why are `interface A<T> {}` and `interface B<T> {}` interchangeable regardless of `T`?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

TypeScript uses a structural type system. Type compatibility is determined by comparing the actual members of the resolved types. Because neither interface declares any members using `T`, both `A<string>` and `A<number>` evaluate to empty object types (`{}`). Because both empty shapes have zero required properties, they are completely interchangeable. To tie the type argument to the structural shape, `T` must be used in at least one property or method.
</details>

### Q3: Why can static methods on a generic class not reference the class's type parameters?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Type parameters on a class (e.g. `class Box<T>`) belong to **instances** of the class. When an instance is constructed (`new Box<string>()`), `T` is bound for that specific object. Static methods and properties, however, belong to the **constructor function itself** (value space), which exists only once in memory regardless of how many instances are created with different type arguments. Because there is no single `T` for the class as a whole, static members cannot reference `T`. If a static method needs generics, it must declare its own type parameters (`static create<U>(): Box<U>`).
</details>

### Q4: What is the difference between a generic type alias and a generic interface?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

- **Generic Interface:** Can only define object and function contracts. Supports declaration merging (multiple declarations of the same interface merge their type parameters and members).
- **Generic Type Alias:** Can represent any type expression (unions, primitives, tuples, conditional types, mapped types). Cannot be merged via declaration merging.
</details>

### Q5: How do default type parameters work in generic types?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

A generic type can specify default type arguments using `=`:
```typescript
type Response<T = unknown> = { data: T; status: number };
```
If a consumer references `Response` without specifying a type argument, TypeScript uses `unknown`. Default type arguments must follow required type arguments in the declaration list.
</details>

### Q6: Can a generic class implement a generic interface?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes. A generic class can implement a generic interface:
```typescript
interface Repository<T> { find(id: string): T | null; }
class BaseRepo<T> implements Repository<T> {
  find(id: string): T | null { return null; }
}
```
The class forwards its own type parameter `T` to the interface contract.
</details>

### Q7: What is the `neverthrow` Result pattern and how is it modeled with generic types?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The Result pattern represents operations that can succeed or fail without throwing exceptions. It is modeled as a generic discriminated union:
```typescript
type Result<T, E> =
  | { ok: true; value: T }
  | { ok: false; error: E };
```
Consumers must inspect `res.ok` before accessing `value` or `error`, turning runtime exceptions into compile-time checked branches.
</details>

### Q8: What is a recursive generic type alias?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

A recursive generic type alias is a type that references itself in its own definition. For example:
```typescript
type NestedArray<T> = T | NestedArray<T>[];
```
TypeScript 3.7+ supports deferred evaluation of recursive type aliases inside array, tuple, and object property positions.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Generic Key-Value Container (Easy)
- **Problem Statement:** Author a generic interface `Entry<K, V>` holding `key: K` and `value: V`, and a function to construct it.
- **Test Case:**
```typescript
const e = createEntry('id', 101); // Entry<string, number>
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
interface Entry<K, V> {
  readonly key: K;
  readonly value: V;
}

function createEntry<K, V>(key: K, value: V): Entry<K, V> {
  return { key, value };
}
```
</details>

### Exercise 2: Generic Queue Class (Easy)
- **Problem Statement:** Implement a generic class `Queue<T>` with `enqueue(item: T)`, `dequeue(): T | undefined`, and `size: number`.
- **Test Case:**
```typescript
const q = new Queue<string>();
q.enqueue('a');
console.log(q.dequeue()); // "a"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class Queue<T> {
  private items: T[] = [];

  enqueue(item: T): void {
    this.items.push(item);
  }

  dequeue(): T | undefined {
    return this.items.shift();
  }

  get size(): number {
    return this.items.length;
  }
}
```
</details>

### Exercise 3: Either Monad (Medium)
- **Problem Statement:** Author a generic `Either<L, R>` type with helper functions `Left(val: L)` and `Right(val: R)`.
- **Test Case:**
```typescript
const e1 = Left('error');
const e2 = Right(42);
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type Either<L, R> =
  | { readonly kind: 'left'; readonly left: L }
  | { readonly kind: 'right'; readonly right: R };

function Left<L>(left: L): Either<L, never> {
  return { kind: 'left', left };
}

function Right<R>(right: R): Either<never, R> {
  return { kind: 'right', right };
}
```
</details>

### Exercise 4: Generic Tree Structure (Medium)
- **Problem Statement:** Define a generic tree node `TreeNode<T>` with `value: T` and `children: TreeNode<T>[]`, and a function to compute total leaf count.
- **Test Case:**
```typescript
const tree: TreeNode<number> = { value: 1, children: [{ value: 2, children: [] }] };
console.log(countNodes(tree)); // 2
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
interface TreeNode<T> {
  value: T;
  children: TreeNode<T>[];
}

function countNodes<T>(node: TreeNode<T>): number {
  return 1 + node.children.reduce((sum, child) => sum + countNodes(child), 0);
}
```
</details>

### Exercise 5: Compile-Time Deep Flatten Array Type (Hard)
- **Problem Statement:** Author a recursive generic type alias `DeepFlat<T>` that unpacks arbitrarily nested arrays into their base element type.
- **Test Case:**
```typescript
type Nested = number[][][][];
type Flat = DeepFlat<Nested>; // number
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type DeepFlat<T> = T extends readonly (infer U)[] ? DeepFlat<U> : T;
```
</details>

### Exercise 6: Generic Event Stream Pipe (Hard)
- **Problem Statement:** Build a class `EventStream<T>` supporting a `.map<U>(fn: (val: T) => U): EventStream<U>` method that transforms emitted events.
- **Test Case:**
```typescript
const s1 = new EventStream<number>();
const s2 = s1.map((n) => n.toString());
s2.subscribe((str) => console.log(str)); // string
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
class EventStream<T> {
  private subscribers: ((val: T) => void)[] = [];

  subscribe(fn: (val: T) => void): () => void {
    this.subscribers.push(fn);
    return () => {
      this.subscribers = this.subscribers.filter((s) => s !== fn);
    };
  }

  emit(val: T): void {
    for (const s of this.subscribers) s(val);
  }

  map<U>(fn: (val: T) => U): EventStream<U> {
    const child = new EventStream<U>();
    this.subscribe((val) => child.emit(fn(val)));
    return child;
  }
}
```
</details>
