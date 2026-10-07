# Branded Types

TypeScript is **structurally** typed: two types with the same shape are interchangeable. That is usually a feature, but it means a `UserId` and an `OrderId` that are both `string` can be swapped without any error. A **branded type** (also called an opaque or nominal-ish type) adds a phantom marker to a base type so the compiler treats otherwise identical values as different. The brand exists only at compile time.

**Prerequisites:**
- [Structural typing](../04-objects-and-interfaces/04-structural-typing.md)
- [Intersection types](../03-unions-and-narrowing/01-intersection-types.md)
- [Type guards and assertion functions](../03-unions-and-narrowing/05-type-guards-and-assertion-functions.md)

---

## The problem

```ts
function getOrder(userId: string, orderId: string) { /* ... */ }

const userId = "u_1";
const orderId = "o_9";

getOrder(orderId, userId);   // compiles: both are just strings
```

Argument order bugs like this are invisible to the compiler as long as both parameters are `string`.

## The pattern

Intersect the base type with an object type carrying a unique tag:

```ts
type Brand<T, B extends string> = T & { readonly __brand: B };

type UserId  = Brand<string, "UserId">;
type OrderId = Brand<string, "OrderId">;

function getOrder(userId: UserId, orderId: OrderId) { /* ... */ }

declare const u: UserId;
declare const o: OrderId;

getOrder(u, o);   // ok
getOrder(o, u);   // error: OrderId is not assignable to UserId
getOrder("u_1", "o_9");   // error: string is not assignable to UserId
```

A plain `string` is no longer accepted where a `UserId` is required, and the two brands cannot be mixed up. A `UserId` still works anywhere a `string` is expected, because it *is* a `string` plus a tag.

The `__brand` property **does not exist at runtime**. It is a phantom: the value is still just a string. Nothing is added to the JavaScript output.

### Variant using a `unique symbol`

```ts
declare const brand: unique symbol;

type Brand<T, B> = T & { readonly [brand]: B };
```

The symbol key cannot collide with a real property and does not show up in autocomplete for the underlying value. Either form works; the string-key form is simpler to read in error messages.

## Creating branded values

The brand has to come from somewhere. Do it in one place, ideally behind validation, using a **smart constructor**:

```ts
type Email = Brand<string, "Email">;

function parseEmail(input: string): Email {
  if (!/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(input)) {
    throw new Error(`Invalid email: ${input}`);
  }
  return input as Email;     // the only cast in the codebase for this type
}

function sendWelcome(to: Email) { /* ... */ }

sendWelcome("a@b.com");              // error
sendWelcome(parseEmail("a@b.com"));  // ok
```

The cast `as Email` is acceptable here because it sits right after the check. Everywhere else, the type system guarantees that an `Email` was validated.

A **type guard** works when you want a boolean check instead of a throw:

```ts
function isEmail(value: string): value is Email {
  return /^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(value);
}

if (isEmail(input)) {
  sendWelcome(input);   // narrowed to Email
}
```

Or an **assertion function**: `function assertEmail(v: string): asserts v is Email`.

## What brands are good for

- **IDs:** `UserId`, `OrderId`, `ProductId`. The most common use.
- **Validated data:** `Email`, `NonEmptyString`, `PositiveInt`, `Slug`. The type is proof that validation happened.
- **Sanitized values:** `SafeHtml` versus raw `string`, so unescaped input cannot reach a sink by accident.
- **Units:** `Meters` versus `Feet`, `Cents` versus `Dollars`.
- **Trust levels:** `Untrusted<string>` versus validated values (see [trust boundaries](../15-runtime-validation/00-trust-boundaries.md)).

```ts
type Meters = Brand<number, "Meters">;
type Feet   = Brand<number, "Feet">;

declare function climb(height: Meters): void;
declare const f: Feet;
climb(f);   // error
```

## Lighter variant: flavoring

If forcing every call site to go through a constructor is too strict, make the tag optional. Plain values are still accepted, but two different flavors do not mix:

```ts
type Flavor<T, F extends string> = T & { readonly __flavor?: F };

type UserId  = Flavor<string, "UserId">;
type OrderId = Flavor<string, "OrderId">;

declare const o: OrderId;
const u: UserId = "u_1";   // ok: plain strings are accepted
const bad: UserId = o;     // error: different flavor
```

It catches swapped IDs without requiring a constructor everywhere, at the cost of weaker guarantees. Use branding when the type must prove validation happened. Use flavoring for IDs that are never validated.

## Using schema libraries

Validation libraries can produce brands as part of parsing, so the cast and the check live in the same place. For example, Zod provides `.brand()` on schemas. See [Zod](../15-runtime-validation/02-zod.md).

## Important rules and misconceptions

**Brands are erased.** There is no runtime check and no runtime tag. A branded value is indistinguishable from the raw value once the program runs. JSON serialization and deserialization drop the brand, so data coming back from the network needs to be branded again after validation.

**A brand is only as good as the places that create it.** If code casts freely (`x as UserId`), the guarantee disappears. Keep the casts in a small number of constructors.

**Arithmetic loses the brand.** `a + b` for two `Meters` values is `number`. Re-brand the result explicitly or provide helper functions (`addMeters`).

**Branded values widen back.** A `UserId` is assignable to `string`, so passing it to a function that accepts `string` is fine (this is usually what you want).

**Brands are about assignability, not behavior.** They do not add methods or change how the value works.

## Common mistakes

- **Casting at call sites** (`"u_1" as UserId`) instead of using a constructor. This defeats the purpose.
- **Branding but never validating.** The type then claims something that is not true. A `NonEmptyString` brand must come from a check.
- **Forgetting that arithmetic returns the base type.**
- **Brand name collisions.** Two unrelated brands using the same tag string become the same type. Use descriptive, unique tags.
- **Branding too much.** Not every `string` needs a brand. Add them where mix-ups are plausible or where validation matters.
- **Trusting brands on data from outside** without validating it first (JSON, query strings, storage).

## Debugging

- Error messages mention the missing brand property: `Type 'string' is not assignable to type 'UserId'` followed by a note about `__brand`. That means a raw value reached a branded parameter. Find where it should have been validated or constructed.
- When two branded types unexpectedly interchange, they likely share a tag string, or one is a flavor.
- Hover the alias to see the intersection. If it looks messy, wrapping the brand in a named alias helps tooltips.

## Quick summary

- Branded types add a phantom tag (`T & { __brand: B }`) so structurally identical types become incompatible.
- Create brands through one validated constructor, type guard, or assertion function. Avoid scattered casts.
- Good for IDs, validated values, units, and sanitized strings. No runtime cost, no runtime protection.
- Flavoring (optional tag) is a looser variant that blocks mix-ups but still accepts plain values.
- Brands do not survive serialization, so re-validate data from outside.

**Next:** [Type-level programming](./08-type-level-programming.md)