# Objects

## Definition
An **object type** describes the shape of an object: its property names and the type of each property.

## Why It Matters
Most real data (API responses, configs, domain entities) is objects. Typing their shape lets the compiler catch missing, misspelled or wrongly-typed properties.

## Prerequisites
[primitive-types.md](01-primitive-types.md), [arrays-and-tuples.md](02-arrays-and-tuples.md)

## Syntax

```ts
const user: { id: number; name: string } = {
  id: 1,
  name: "Ada",
};
```

Properties are separated by `;` or `,` (either works; pick one and be consistent).

## Basic Example

```ts
function printUser(user: { id: number; name: string }) {
  console.log(user.id, user.name.toUpperCase());
}

printUser({ id: 1, name: "Ada" });          // ok
printUser({ id: 1 });                       // Error: Property 'name' is missing
printUser({ id: 1, name: "Ada", age: 36 }); // Error: Object literal may only specify known properties
```

## Important Concepts

### Optional properties
```ts
type Config = { host: string; port?: number };

const c: Config = { host: "localhost" };
c.port; // number | undefined
```

### Readonly properties
```ts
const point: { readonly x: number; readonly y: number } = { x: 1, y: 2 };
point.x = 5; // Error: Cannot assign to 'x' because it is a read-only property
```
`readonly` is compile-time only; it does not freeze the object at runtime.

### Nested objects
```ts
type Order = {
  id: string;
  customer: { name: string; email: string };
  items: { sku: string; qty: number }[];
};
```

### Extra properties: literals vs. variables
Fresh object literals are checked strictly; values coming from variables are not:

```ts
const input = { id: 1, name: "Ada", extra: true };
printUser(input);  // ok: extra properties allowed when not a fresh literal
```
This is structural typing; see [structural typing](../04-objects-and-interfaces/04-structural-typing.md).

### Dynamic keys
```ts
const scores: { [name: string]: number } = { ada: 10 };   // index signature
const scores2: Record<string, number> = { ada: 10 };       // equivalent, preferred
```

### The `object`, `{}` and `Object` types: avoid
| Type | Meaning |
|---|---|
| `object` | Any non-primitive value |
| `{}` | Any value except `null`/`undefined` (includes strings, numbers!) |
| `Object` | Wrapper type; avoid |

Prefer a specific shape, `Record<string, unknown>`, or `unknown`.

### Accessing a property that may not exist
```ts
const cfg: Config = { host: "a" };
cfg.timeout; // Error: Property 'timeout' does not exist on type 'Config'
```

## Common Mistakes
- Using `{}` or `Object` thinking they mean "any object".
- Treating `readonly` as runtime immutability.
- Forgetting that `prop?: T` means `T | undefined`.
- Repeating the same inline shape in many places; name it with a [type alias](04-type-aliases.md) or an [interface](../04-objects-and-interfaces/00-interfaces.md).

## Best Practices
- Name shapes you use more than once.
- Keep object types small and composable.
- Prefer `Record<string, T>` for dictionaries.
- Mark data you do not mutate as `readonly`.

## Interview Questions
- Difference between `object`, `{}` and `Object`?
- What does `?` mean on a property, and how does it differ from `| undefined`?
- Why does passing a variable with extra properties compile but a fresh literal does not?

## Quick Reference
```ts
{ a: string; b?: number; readonly c: boolean }
{ [key: string]: number }
Record<string, number>
object
```

## Related Topics
- [type-aliases.md](04-type-aliases.md)
- [Interfaces](../04-objects-and-interfaces/00-interfaces.md)
- [Readonly and optional properties](../04-objects-and-interfaces/02-readonly-and-optional-properties.md)
- [Index signatures](../04-objects-and-interfaces/03-index-signatures.md)
