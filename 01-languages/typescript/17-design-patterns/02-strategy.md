# Strategy

The **strategy** pattern lets you swap one piece of behavior for another without changing the code that uses it. You define a common shape for the behavior (an interface or a function type), write several interchangeable implementations, and let the caller pick one. In TypeScript, a strategy is very often just a **function**, which makes the pattern lighter than in class-heavy languages.

**Prerequisites:**
- [Function types](../02-functions/00-function-types.md)
- [Callbacks](../02-functions/02-callbacks.md)
- [Interfaces](../04-objects-and-interfaces/00-interfaces.md)

---

## The problem

```ts
function shippingCost(method: string, weightKg: number): number {
  if (method === "standard") return 5 + weightKg * 0.5;
  if (method === "express")  return 15 + weightKg * 1.2;
  if (method === "pickup")   return 0;
  throw new Error(`Unknown method: ${method}`);
}
```

Every new shipping method means editing this function. The function knows about all methods, mixes unrelated rules, and gets harder to test and read as it grows.

## The pattern: a strategy is a function

```ts
type ShippingStrategy = (weightKg: number) => number;

const standard: ShippingStrategy = (w) => 5 + w * 0.5;
const express: ShippingStrategy  = (w) => 15 + w * 1.2;
const pickup: ShippingStrategy   = () => 0;

function checkoutTotal(subtotal: number, weightKg: number, shipping: ShippingStrategy): number {
  return subtotal + shipping(weightKg);
}

checkoutTotal(100, 2, express);    // 117.4
```

`checkoutTotal` is the **context**: it uses a strategy without knowing which one. Each strategy is small, independent, and easy to test. Adding a new method means writing a new function, not editing the existing ones.

This is the same idea as passing a comparator to `Array.prototype.sort`, a predicate to `filter`, or a handler to `map`. Callbacks *are* strategies ([callbacks](../02-functions/02-callbacks.md)).

## Choosing a strategy by key

When the choice comes from data (a setting, a request field), keep the strategies in a typed table:

```ts
type ShippingMethod = "standard" | "express" | "pickup";

const shippingStrategies: Record<ShippingMethod, ShippingStrategy> = {
  standard: (w) => 5 + w * 0.5,
  express:  (w) => 15 + w * 1.2,
  pickup:   () => 0,
};

function shippingCost(method: ShippingMethod, weightKg: number): number {
  return shippingStrategies[method](weightKg);
}
```

`Record<ShippingMethod, ...>` requires an entry for every method, so adding `"drone"` to the union is a compile error until a strategy exists. Compared with the original `if` chain, there is no unknown-method branch at all, because the type rules it out. If the method comes from untrusted input, validate it first ([trust boundaries](../15-runtime-validation/00-trust-boundaries.md)).

## Strategies with a richer interface

When a strategy needs several operations, or its own state, use an interface:

```ts
interface Compressor {
  readonly name: string;
  compress(data: Uint8Array): Promise<Uint8Array>;
  decompress(data: Uint8Array): Promise<Uint8Array>;
}

class GzipCompressor implements Compressor {
  readonly name = "gzip";
  async compress(data: Uint8Array) { /* ... */ return data; }
  async decompress(data: Uint8Array) { /* ... */ return data; }
}

class Archiver {
  constructor(private readonly compressor: Compressor) {}

  async pack(data: Uint8Array) {
    return this.compressor.compress(data);
  }
}

new Archiver(new GzipCompressor());
```

Use an object or class when strategies are stateful, need configuration (a compression level), or have multiple related methods. Use a plain function when there is one operation. If a strategy interface has a single method, it is probably a function type in disguise.

## Generic strategies

A strategy type can be generic over its input and output:

```ts
type Strategy<In, Out> = (input: In) => Out;

type Validator<T> = Strategy<T, string[]>;     // returns a list of error messages

const nonEmpty: Validator<string> = (s) => (s.length > 0 ? [] : ["must not be empty"]);
const maxLen = (n: number): Validator<string> => (s) => (s.length <= n ? [] : [`max ${n} characters`]);

function validate<T>(value: T, ...rules: Validator<T>[]): string[] {
  return rules.flatMap((rule) => rule(value));
}

validate("hello", nonEmpty, maxLen(3));    // ["max 3 characters"]
```

`maxLen` is a function that **returns** a strategy (a closure), a convenient way to configure one ([factory](./00-factory.md)).

## Strategy vs a discriminated union and `switch`

Both choose behavior by a tag. They suit different situations:

| | Strategy (functions or objects) | Discriminated union + `switch` |
|---|---|---|
| Set of variants | **open**: anyone can add one, even from another module or plugin | **closed**: all variants are known in one place |
| Adding a variant | write a new strategy, no edits to the context | add a union member and update every `switch` (the compiler lists them) |
| Adding a new *operation* | edit every strategy | add one new `switch` function |
| Variants carry different data | each strategy holds its own | each union member carries its own fields |

Choose **strategy** when the set of behaviors should be extensible or injected (plugins, user-defined rules, test doubles). Choose a **union and `switch`** when the set is fixed and you want the compiler to force every operation to handle every case ([discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md)). This is the classic "expression problem" trade-off.

## Strategy and dependency injection

Strategy is dependency injection for behavior: the context receives its collaborator instead of choosing it. That makes it easy to test, since you can pass a fake:

```ts
const freeShipping: ShippingStrategy = () => 0;
expect(checkoutTotal(100, 10, freeShipping)).toBe(100);
```

When strategies are chosen at startup and shared across many classes, a container or a composition root wires them ([dependency injection](./05-dependency-injection.md)).

## Practical examples

- **Pricing and discounts:** per customer tier, seasonal rules.
- **Authentication:** password, OAuth, API key, each implementing "given a request, return a user or null".
- **Serialization:** JSON, CSV, and binary exporters behind `serialize(data)`.
- **Sorting and filtering:** comparator and predicate functions.
- **Retry and backoff:** a function from attempt number to delay (`(n) => 2 ** n * 100`).
- **Caching or storage backends:** in-memory, file, remote, behind one interface.

## Common mistakes

- Creating a strategy interface and class hierarchy for two cases that an `if` would handle.
- A strategy that needs so much context from the caller that its signature is a long list of parameters. That suggests the strategy boundary is in the wrong place.
- Strategies that secretly share mutable state.
- Strategy tables keyed by `string` instead of a union, losing exhaustiveness and allowing unknown keys.
- A strategy that quietly depends on global configuration, which defeats swappability and tests.
- Using a strategy where the variants are fixed and a `switch` with exhaustiveness checking would be safer.
- Looking up a strategy by a user-provided string without validating it first.

## Debugging

- If the wrong behavior runs, log which strategy was selected and by which key.
- If a table lookup returns `undefined` at runtime, the key came from unvalidated input or a type assertion. Check where the key originated.
- If tests are hard to write, the strategy probably reaches for something global. Pass it in.
- When a strategy type is awkward to implement, check whether the shared signature is too wide or too narrow for what the variants need.

## Quick summary

- A strategy is an interchangeable piece of behavior behind a shared signature. The context uses it without knowing which one.
- In TypeScript, strategies are usually plain functions or small objects. Callbacks like comparators are strategies.
- Select by key with `Record<Union, Strategy>` so adding a variant forces a new strategy.
- Use objects or classes when strategies hold state or have several operations. Use generics for reusable strategy shapes.
- Prefer strategy for open, extensible, or injected behavior. Prefer a union and exhaustive `switch` for closed sets.

**Next:** [Adapter](./03-adapter.md)
