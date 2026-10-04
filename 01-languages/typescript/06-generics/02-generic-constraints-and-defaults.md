# Generic Constraints and Defaults

## Definition
- A **constraint** (`T extends X`) restricts which types a type parameter accepts and lets the code inside rely on those capabilities.
- A **default** (`T = X`) supplies a type argument when none is given or inferred.

## Why It Matters
Unconstrained type parameters are opaque: you cannot read any property from `T`. Constraints give you just enough knowledge to be useful while keeping the function generic. Defaults make generic APIs easy to use in the common case.

## Prerequisites
[generic-functions.md](00-generic-functions.md), [generic-types.md](01-generic-types.md)

## Syntax

```ts
function longest<T extends { length: number }>(a: T, b: T): T {
  return a.length >= b.length ? a : b;
}

longest("abc", "de");          // string
longest([1, 2], [1, 2, 3]);    // number[]
longest(1, 2);                 // Error: number has no 'length'

interface Response<T = unknown> { data: T }
```

## Basic Example

```ts
function getId<T extends { id: string }>(item: T): string {
  return item.id;
}

getId({ id: "a", name: "Ada" });   // ok: extra properties allowed
getId({ name: "Ada" });            // Error: 'id' is missing
```

## How It Works

### `T extends X` means "T must be assignable to X"
Inside the function, `T` can use everything `X` offers. The caller still gets their **specific** type back, not just `X`:

```ts
function firstId<T extends { id: string }>(items: T[]): T {
  return items[0];
}
const user = firstId([{ id: "1", name: "Ada" }]);
user.name;   // ok: T is the full object type
```
Compare with `function firstId(items: { id: string }[]): { id: string }`, which would lose `name`.

### Constraining with another type parameter
```ts
function getProp<T, K extends keyof T>(obj: T, key: K): T[K] {
  return obj[key];
}

const user = { id: 1, name: "Ada" };
getProp(user, "name");   // string
getProp(user, "age");    // Error: "age" is not assignable to "id" | "name"
```
This `K extends keyof T` pattern is the most common constraint; see [keyof and typeof](03-keyof-and-typeof.md).

### Common constraint targets

| Constraint | Meaning |
|---|---|
| `T extends object` | Any non-primitive |
| `T extends string \| number` | One of those primitives (keeps literals) |
| `T extends unknown[]` | Any array |
| `T extends readonly unknown[]` | Arrays and tuples, readonly or not |
| `T extends (...args: any[]) => any` | Any function |
| `T extends new (...args: any[]) => any` | Any constructor |
| `T extends Record<string, unknown>` | Object with string keys |
| `T extends { id: string }` | Has an `id` |

### Constructor constraint
```ts
function create<T>(ctor: new () => T): T {
  return new ctor();
}
create(Date);   // Date
```

### Constraints keep literal types
```ts
function box<T>(x: T): { value: T } { return { value: x }; }
box("a");   // { value: string }  (the literal is widened)

function boxLiteral<T extends string>(x: T): { value: T } { return { value: x }; }
boxLiteral("a");   // { value: "a" }  (the constraint preserves the literal)
```
A constraint to a primitive tells TypeScript to preserve the literal type; see [generic inference](05-generic-inference.md).

### Constraints do not narrow `T` to a subtype for returns
You cannot return something that merely satisfies the constraint when the return type is `T`:

```ts
function make<T extends { id: string }>(): T {
  return { id: "x" };   // Error: '{ id: string }' is assignable to the constraint of T, but T could be a different subtype
}
```
Because the caller may choose a more specific `T`. Fix by changing the return type, or by accepting a factory.

### Conditional use of constraints
```ts
function sum<T extends number | bigint>(a: T, b: T): T { /* ... */ throw new Error(); }
```

## Defaults

### Syntax
```ts
interface Paginated<T, Meta = { total: number }> {
  items: T[];
  meta: Meta;
}

const p: Paginated<User> = { items: [], meta: { total: 0 } };
const q: Paginated<User, { cursor: string }> = { items: [], meta: { cursor: "a" } };
```

### Rules
- Parameters with defaults must come **after** required ones.
- A default must satisfy the parameter's constraint.
- A default can reference earlier parameters:

```ts
type Pair<A, B = A> = [A, B];
type P = Pair<string>;   // [string, string]
```

### Constraint + default
```ts
function create<T extends object = {}>(init?: T): T { return (init ?? {}) as T; }
```

### Defaults when inference fails
When there are no inference candidates TypeScript uses the default (or the constraint, or `unknown`):

```ts
function emptyList<T = string>(): T[] { return []; }
emptyList();         // string[]
emptyList<number>(); // number[]
```

### Defaults in generic classes
```ts
class Cache<K = string, V = unknown> {
  private map = new Map<K, V>();
}
const c = new Cache();   // Cache<string, unknown>
```

## Common Mistakes
- Over-constraining (`T extends Record<string, any>`) and rejecting valid inputs such as interfaces.
- Using `extends` with the expectation that the result type is the constraint.
- Defaults that do not satisfy their constraint.
- Placing a defaulted parameter before a required one.
- Constraining to `any` (for functions, `(...args: any[]) => any` is the accepted idiom; elsewhere prefer `unknown`).

## Best Practices
- Constrain to the minimum shape the implementation uses.
- Use `K extends keyof T` for key parameters.
- Prefer `unknown` over `any` in constraints except for function/constructor types.
- Provide defaults only where a common case exists; do not default to `any`.

## Interview Questions
- What does `T extends { length: number }` allow and disallow?
- Why is `function f<T extends X>(): T { return x }` an error?
- How does a constraint help keep literal types?
- What are the ordering rules for defaulted type parameters?

## Quick Reference
```ts
<T extends string | number>
<T, K extends keyof T>
<T extends new (...a: any[]) => any>
<T = string>
<A, B = A>
<T extends object = {}>
```

## Related Topics
- [keyof-and-typeof.md](03-keyof-and-typeof.md)
- [generic-inference.md](05-generic-inference.md)
- [Conditional types](../10-advanced-types/00-conditional-types.md)
- [Mixins](../05-classes/06-mixins.md)
