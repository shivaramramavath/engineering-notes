# Generic Types

## Definition
Interfaces, type aliases and classes can all take type parameters, making **generic types**: reusable templates that become concrete types once you supply type arguments (`Box<string>`).

## Why It Matters
Containers (`Array<T>`, `Promise<T>`, `Map<K, V>`), API envelopes (`ApiResponse<T>`), results (`Result<T, E>`), repositories and state stores are all generic. Writing your own gives you reusable structure with precise types.

## Prerequisites
[generic-functions.md](00-generic-functions.md), [interfaces](../04-objects-and-interfaces/00-interfaces.md), [classes](../05-classes/00-classes.md)

## Syntax

### Generic interface
```ts
interface Box<T> {
  value: T;
}

const a: Box<string> = { value: "x" };
const b: Box<number> = { value: 1 };
const c: Box = { value: 1 };   // Error: Generic type 'Box<T>' requires 1 type argument(s)
```

### Generic type alias
```ts
type Nullable<T> = T | null;
type Pair<A, B> = [A, B];
type ApiResponse<T> = {
  data: T;
  error?: string;
};
```

### Generic class
```ts
class Stack<T> {
  private items: T[] = [];

  push(item: T): void { this.items.push(item); }
  pop(): T | undefined { return this.items.pop(); }
  peek(): T | undefined { return this.items[this.items.length - 1]; }
  get size(): number { return this.items.length; }
}

const s = new Stack<number>();
s.push(1);
s.push("a");   // Error
```

## Basic Example

```ts
type Result<T, E = Error> =
  | { ok: true; value: T }
  | { ok: false; error: E };

function parseNumber(s: string): Result<number, string> {
  const n = Number(s);
  return Number.isNaN(n) ? { ok: false, error: "not a number" } : { ok: true, value: n };
}
```

## How It Works
Type arguments are substituted for type parameters, producing a new concrete type each time. `Box<string>` and `Box<number>` are different types.

### Inference for classes and functions that return generic types
Type arguments for **classes** are inferred from the constructor arguments:

```ts
class Wrapper<T> {
  constructor(public value: T) {}
}
const w = new Wrapper("hi");   // Wrapper<string>
```
Type **aliases and interfaces are never inferred**; you must write the arguments (except where defaults exist).

### Generic interfaces for functions
```ts
interface Mapper<T, U> {
  (input: T): U;
}
const toLength: Mapper<string, number> = s => s.length;
```

### Generic vs. generic-method placement
```ts
interface A<T> { get(): T }          // T fixed when you use A<string>
interface B { get<T>(): T }          // caller picks T at each call (usually a smell)
```

## Important Concepts

### Constraints and defaults
```ts
interface Repository<T extends { id: string }, ID = string> {
  findById(id: ID): Promise<T | undefined>;
  save(entity: T): Promise<void>;
}
```
See [constraints and defaults](02-generic-constraints-and-defaults.md).

### Built-in generic types you already use
```ts
Array<T>        Promise<T>        Map<K, V>       Set<T>
Record<K, V>    Partial<T>        ReadonlyArray<T>
```

### Nested and recursive generics
```ts
type Tree<T> = { value: T; children: Tree<T>[] };
type Paginated<T> = { items: T[]; nextCursor?: string };
type ApiResult<T> = Result<Paginated<T>, ApiError>;
```

### Static members cannot use class type parameters
```ts
class Box<T> {
  static create(): Box<T> { }   // Error: Static members cannot reference class type parameters
}
```
Give the static method its own type parameter; see [static members](../05-classes/04-static-members.md).

### Generic classes implementing generic interfaces
```ts
class InMemoryRepo<T extends { id: string }> implements Repository<T> {
  private data = new Map<string, T>();
  async findById(id: string) { return this.data.get(id); }
  async save(entity: T) { this.data.set(entity.id, entity); }
}
```

### Variance (preview)
`Box<Dog>` is assignable to `Box<Animal>` only if `Box` uses `T` in a covariant position. Methods taking `T` can make a type invariant under strict checking. See [variance](../14-type-system-internals/02-variance.md).

### Keep type parameters meaningful
```ts
// weak: T does nothing useful
interface Thing<T> { name: string }

// good: T is used in members
interface Cache<K, V> { get(key: K): V | undefined; set(key: K, value: V): void }
```
Unused type parameters make types accept anything and can hide inference problems.

## Common Mistakes
- Forgetting that interface/alias type arguments are not inferred.
- Declaring unused type parameters.
- Putting a type parameter on a method when it should be on the type (or vice versa).
- Using `any` as a type argument (`Box<any>`) to avoid thinking.
- Trying to use `T` in static members.

## Best Practices
- Give type parameters defaults when there is a sensible common choice (`Result<T, E = Error>`).
- Constrain parameters to what the type needs.
- Prefer descriptive names for multi-parameter types (`TKey`, `TValue`).
- Build on built-in generics instead of re-inventing them.

## Interview Questions
- Can type arguments be inferred for an interface? For a class?
- What is the difference between `interface A<T> { f(): T }` and `interface B { f<T>(): T }`?
- Why can't static members use a class's type parameters?
- When would you add a default to a type parameter?

## Quick Reference
```ts
interface Box<T> { value: T }
type Maybe<T> = T | null | undefined
class Stack<T> { items: T[] = [] }
type Res<T, E = Error> = { ok: true; value: T } | { ok: false; error: E }
new Stack<number>()   Box<string>
```

## Related Topics
- [generic-functions.md](00-generic-functions.md)
- [generic-constraints-and-defaults.md](02-generic-constraints-and-defaults.md)
- [Result pattern](../11-error-handling/02-result-pattern.md)
- [Request and response types](../16-type-safe-apis/01-request-response-types.md)
