# Assignability and Subtyping

Almost every type error you see is the compiler saying "this type is not **assignable** to that type". Assignability is the relation TypeScript checks when you pass an argument, return a value, assign to a variable, or compare types in a conditional (`T extends U`). Knowing the rules, and which ones are deliberately loose, turns cryptic errors into predictable ones.

**Prerequisites:**
- [Structural typing](../04-objects-and-interfaces/04-structural-typing.md)
- [Union types](../03-unions-and-narrowing/00-union-types.md) and [intersection types](../03-unions-and-narrowing/01-intersection-types.md)
- [Type erasure and runtime](./00-type-erasure-and-runtime.md)

---

## The core idea: structural compatibility

TypeScript compares types by **shape**, not by name. `S` is assignable to `T` if a value of type `S` can safely be used wherever `T` is expected.

```ts
interface Point2D { x: number; y: number }
interface Point3D { x: number; y: number; z: number }

const p3: Point3D = { x: 1, y: 2, z: 3 };
const p2: Point2D = p3;      // ok: Point3D has everything Point2D requires
const bad: Point3D = p2;     // error: property 'z' is missing
```

A type with **more** properties is a **subtype** of one with fewer. The subtype can stand in for the supertype, never the reverse. Names do not matter: two interfaces with identical members are interchangeable.

## The rules, by kind of type

### Primitives and literals

A literal type is assignable to its base primitive, but not the other way around:

```ts
const a: string = "hello" as "hello";    // ok: "hello" -> string
const b: "hello" = "x" as string;        // error: string -> "hello"
```

### Unions

- **Source is a union:** *every* member must be assignable to the target.
- **Target is a union:** the source needs to be assignable to *at least one* member.

```ts
declare let n: number;
declare let sn: string | number;

sn = n;      // ok: number is one of the members
n = sn;      // error: string is not assignable to number
```

### Intersections

- **Target is an intersection:** the source must be assignable to *every* part.
- **Source is an intersection:** it is assignable if *any* part is assignable (it has at least that part's properties).

### Objects

The target's properties must exist in the source, with assignable types. Extra properties in the source are fine. Optional target properties may be missing. `readonly` is **ignored** for assignability.

```ts
type Target = { id: number; name?: string; readonly tag: string };

const ok: Target = { id: 1, tag: "x" };                       // name is optional
const rw: { id: number; name?: string; tag: string } = ok;    // readonly -> mutable: allowed
```

### Functions

Compared by parameters and return type:

```ts
type Handler = (a: number, b: string) => void;

const fewer: Handler = (a: number) => {};          // ok: fewer parameters is fine
const more: Handler = (a: number, b: string, c: boolean) => {};   // error: extra required parameter

const returns: () => void = () => 42;              // ok: a void return type ignores the result
```

- A function that takes **fewer** parameters can be used where more are provided (callers pass extras that are ignored). That is why `array.map(x => ...)` works without declaring `index`.
- Return types are compared covariantly: the source's return must be assignable to the target's.
- Parameter types are compared contravariantly under `strictFunctionTypes`. See [variance](./02-variance.md).

### Arrays and tuples

`string[]` is assignable to `(string | number)[]` (covariant, see [variance](./02-variance.md)). Tuples have fixed length, so `[number, string]` is not assignable to `[number]`, but is assignable to `(number | string)[]`.

### Classes and nominal-ish behavior

Classes are compared structurally too, **except** for `private` and `protected` members. A class with a private member is only compatible with types that declare that member from the **same declaration**:

```ts
class A { private secret = 1; }
class B { private secret = 1; }

const a: A = new B();   // error: types have separate declarations of a private property 'secret'
```

This is one of the few places where TypeScript behaves nominally. Branded types ([branded types](../10-advanced-types/07-branded-types.md)) emulate the effect for other types.

### The special types

```text
                unknown        (top: everything is assignable to it)
                   |
      +------------+------------+
      |            |            |
   object       string ...    null, undefined (with strictNullChecks, separate)
      |
   (all object types, arrays, functions, classes)
                   |
                 never         (bottom: assignable to everything)
```

- **`unknown`** accepts any value, but gives nothing back until narrowed.
- **`never`** is assignable to every type (it has no values), and nothing except `never` is assignable to it.
- **`any`** is assignable to and from everything (except `never`). It switches checking off for that value. It is not part of the hierarchy above. It is an escape.
- **`void`** is for "no useful return value". Only `undefined` is assignable to it (but see the function-return case above).
- `{}` accepts any non-nullish value, including primitives. `object` accepts only non-primitives. `Object` (capital) is almost never what you want.

## Freshness and excess property checks

Object **literals** get an extra check: properties that do not exist in the target are an error.

```ts
interface Options { timeout: number }

const o1: Options = { timeout: 1, retries: 3 };
// error: Object literal may only specify known properties

const raw = { timeout: 1, retries: 3 };
const o2: Options = raw;       // ok: not a fresh literal, so no excess check
```

The excess-property check applies only to *fresh* literals assigned directly. It exists to catch typos, not as a sound rule: structural subtyping says the extra property is harmless.

## Weak types

A type whose properties are **all optional** is "weak". Assigning something that shares **no** properties with it is an error, even though structurally it would be allowed:

```ts
interface Config { port?: number; host?: string }

const c: Config = { prot: 3000 };      // error: typo is caught
```

## Assignability vs subtyping vs comparability

TypeScript uses several related relations:

| Relation | Used for | Notable looseness |
|---|---|---|
| **Assignability** | assignments, arguments, returns | `any` goes both ways, optional properties, numeric enums vs `number`, void returns |
| **Subtype** | picking the best common type, choosing overloads, union reduction | stricter, `any` handled differently |
| **Comparability** | type assertions (`as`) and `===` checks | even more lenient: allows assertion if either direction is assignable |
| **Identity** | `Equal`-style checks, some declaration merging | exact |

You mostly meet **assignability** and, with `as`, **comparability**:

```ts
const x = "hello" as number;           // error: neither type is comparable to the other
const y = ("hello" as unknown) as number;   // allowed: unknown is comparable to everything
```

This is why `as unknown as T` works as an escape hatch. See [soundness and escape hatches](./04-soundness-and-escape-hatches.md).

## Generics and conditional types

For type arguments, assignability of `Foo<A>` to `Foo<B>` depends on the **variance** of `Foo`'s type parameters ([variance](./02-variance.md)). In `T extends U ? X : Y`, the test is assignability, which is why `"a"` extends `string` and why `any` behaves specially there ([conditional types](../10-advanced-types/00-conditional-types.md)).

## Reading assignability errors

Errors are nested, and the **last line is usually the cause**:

```text
Type '{ id: number; user: { name: number } }' is not assignable to type 'Order'.
  Types of property 'user' are incompatible.
    Type '{ name: number }' is not assignable to type 'User'.
      Types of property 'name' are incompatible.
        Type 'number' is not assignable to type 'string'.
```

Read bottom up: a `number` was supplied where a `string` was expected, at `order.user.name`. See [reading type errors](../18-testing-and-debugging/06-reading-type-errors.md).

## Important rules and misconceptions

- **"Having extra properties makes a type incompatible."** Not for ordinary assignment. Only fresh object literals trigger the excess check.
- **"`readonly` affects assignability."** It does not. A `readonly` property is assignable to a mutable one.
- **"Names matter."** They do not (except private/protected members and `unique symbol`).
- **"`any` is a top type."** It is both top and bottom for checking purposes and disables errors. `unknown` is the safe top type.
- **"Subtype means subclass."** Subtyping here is about shape. A structurally compatible type is a subtype with no `extends` relationship.
- **Optionality is part of the shape.** `{ a?: string }` and `{ a: string | undefined }` are different (and distinct under `exactOptionalPropertyTypes`).

## Common mistakes

- Expecting an extra property to cause an error on a non-literal value.
- Expecting `readonly` to prevent assigning to a mutable alias.
- Relying on `{}` or `Object` to mean "any object".
- Using `any` where `unknown` would force a check.
- Misreading errors from the top instead of the last line.
- Using `as` to force assignability between unrelated types instead of fixing the data.

## Debugging

- Hover both types and compare them property by property.
- Isolate with a small assignment: `const x: Target = value;` and read the nested message bottom up.
- Use type-level tests (`Expect<Equal<A, B>>`, `A extends B ? true : false`) to check relationships ([type testing](../18-testing-and-debugging/04-type-testing.md)).
- If an object literal triggers an excess property error you believe is wrong, check for a typo, or assign to a variable first to see whether the problem is only freshness.

## Quick summary

- `S` is assignable to `T` if `S` can safely be used where `T` is expected. The check is structural.
- Unions need every source member to fit (source) or one member to fit (target). Intersections work the opposite way.
- Extra properties are fine, except for fresh object literals. `readonly` is ignored. Optional properties may be missing.
- Functions: fewer parameters is fine, returns are covariant, parameters are contravariant under `strictFunctionTypes`.
- `unknown` is the top type, `never` the bottom, `any` the escape hatch. Private and protected members make classes nominal.
- Assertions use the more lenient *comparability* relation. Read nested errors from the bottom up.

**Next:** [Variance](./02-variance.md)
