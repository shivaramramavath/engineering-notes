# Index Signatures

## Definition
An **index signature** describes an object whose property names are not known in advance but whose keys and values all follow a pattern: `{ [key: string]: T }`.

## Why It Matters
Dictionaries, lookup tables, parsed JSON maps and query-string objects all have dynamic keys. Index signatures type them, but they also weaken safety if misused.

## Prerequisites
[interfaces.md](00-interfaces.md), [objects](../01-fundamentals/03-objects.md)

## Syntax

```ts
interface Dictionary {
  [key: string]: number;
}

const scores: Dictionary = { ada: 10, linus: 12 };
scores.grace = 15;           // ok
scores.bob = "high";         // Error: string is not assignable to number
```

The name in brackets (`key`) is documentation only.

## Basic Example

```ts
function countWords(text: string): Record<string, number> {
  const counts: Record<string, number> = {};
  for (const word of text.split(/\s+/)) {
    counts[word] = (counts[word] ?? 0) + 1;
  }
  return counts;
}
```

## How It Works

### Key types allowed
`string`, `number`, `symbol`, template literal patterns, and unions of those.

```ts
interface A { [k: string]: unknown }
interface B { [k: number]: string }               // array-like
interface C { [k: `data-${string}`]: string }     // pattern keys
interface D { [k: symbol]: unknown }
```
A `number` index is converted to a string at runtime, so a `string` index signature also covers numeric keys; the number index type must be assignable to the string index type.

### Every known property must match the signature
```ts
interface Bad {
  [key: string]: number;
  name: string;      // Error: 'string' is not assignable to 'number'
}

interface Good {
  [key: string]: number | string;
  name: string;
  age: number;
}
```
For mixed shapes consider a union or an intersection:

```ts
type Headers = { "content-type": string } & { [key: string]: string };
```

### Indexed access returns `T`, even for missing keys
```ts
const scores: Record<string, number> = {};
const x = scores["nobody"];   // number (but actually undefined at runtime)
x.toFixed();                  // compiles, crashes
```

Fix with `noUncheckedIndexedAccess`, which makes index results `T | undefined`:

```ts
// tsconfig: "noUncheckedIndexedAccess": true
const y = scores["nobody"];   // number | undefined
y?.toFixed();
```
This option also affects arrays (`arr[0]` becomes `T | undefined`).

## Important Concepts

### `Record<K, V>`
`Record<string, number>` is the usual way to write a string-keyed index signature, and `Record<Union, V>` creates an object with exactly those keys:

```ts
type Role = "admin" | "user";
const perms: Record<Role, string[]> = {
  admin: ["read", "write"],
  user: ["read"],
};   // missing a key is an error
```
When you know the keys, use a union (or a mapped type) instead of an index signature; you get precise keys and autocompletion.

### Index signature vs. mapped type
```ts
type Flags = { [K in "a" | "b" | "c"]: boolean };   // exactly a, b, c
type Open = { [key: string]: boolean };             // anything
```

### `Map` vs. object
Prefer `Map<K, V>` when:
- keys are not strings or you need non-string keys,
- keys are added and removed frequently,
- keys come from user input (avoid `__proto__`/prototype collisions),
- you need reliable insertion order and `.size`.

### Checking for keys
```ts
if ("ada" in scores) {}                          // includes prototype chain
if (Object.hasOwn(scores, "ada")) {}             // own properties only
const v = scores.ada;                            // undefined if missing
```

### Prototype pollution risk
Using user-controlled keys on plain objects can hit `__proto__` or `constructor`. For untrusted keys use `Map` or `Object.create(null)`. See [input validation](../23-security/01-input-validation.md).

### Iterating
```ts
for (const [key, value] of Object.entries(scores)) { /* key: string, value: number */ }
```
`Object.keys` returns `string[]`, not `(keyof T)[]`, because objects may have extra keys at runtime.

### Readonly index signatures
```ts
interface Lookup { readonly [key: string]: string }
```

### Index signatures and `unknown`
`{ [key: string]: unknown }` accepts any object-like value and is a safer alternative to `any` for "bag of properties" parameters.

## Common Mistakes
- Using `string` index signatures when the set of keys is known.
- Forgetting indexed access returns `T`, not `T | undefined`.
- Mixing incompatible known property types with an index signature.
- Using objects as maps for user-supplied keys.
- Assuming `Object.keys(obj)` gives `(keyof T)[]`.

## Best Practices
- Use `Record<UnionOfKeys, V>` for known keys; `Record<string, V>` or `Map` for open-ended keys.
- Turn on `noUncheckedIndexedAccess`.
- Validate dictionaries that come from outside the program.
- Prefer `Map` for dynamic key sets.

## Interview Questions
- What is an index signature, and what key types are allowed?
- Why does `obj[key]` return `T` and not `T | undefined`, and how do you change that?
- When would you use `Map` instead of an object with an index signature?
- Difference between `Record<string, T>` and `{ [key: string]: T }`? (none; they are equivalent)

## Quick Reference
```ts
{ [key: string]: T }
Record<string, T>
Record<"a" | "b", T>
{ [key: `prefix-${string}`]: T }
{ readonly [key: string]: T }
```

## Related Topics
- [interfaces.md](00-interfaces.md)
- [pick-omit-record.md](../07-utility-types/01-pick-omit-record.md)
- [Mapped types](../10-advanced-types/03-mapped-types.md)
- [Compiler options](../13-compiler-and-tsconfig/00-compiler-options.md)
