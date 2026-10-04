# Generic Inference

## Definition
**Generic inference** is how TypeScript works out type arguments from the values you pass to a generic function, so you rarely need to write them explicitly. Understanding the rules explains most "why did it infer that?" surprises.

## Why It Matters
Good generic APIs are ones where callers never write `<...>`. Bad inference leads to `unknown`, overly wide types, or confusing errors. Knowing how to steer inference is what separates using generics from designing them.

## Prerequisites
[generic-functions.md](00-generic-functions.md), [generic-constraints-and-defaults.md](02-generic-constraints-and-defaults.md)

## Basic Example

```ts
function wrap<T>(value: T): { value: T } {
  return { value };
}

wrap(1);          // { value: number }
wrap("a");        // { value: string }
wrap([1, 2]);     // { value: number[] }
```

## How It Works

### Inference sites and candidates
TypeScript looks at every place the type parameter appears in the **parameters**, collects **candidates** from the arguments, and picks the best one.

```ts
function pair<T>(a: T, b: T): T[] { return [a, b]; }

pair(1, 2);          // T = number
pair("a", "b");      // T = string
pair(1, "b");        // Error: Argument of type 'string' is not assignable to parameter of type 'number'
```
For the last call, the first argument made `number` the best candidate. Allow both by widening the signature:

```ts
function pair2<T, U = T>(a: T, b: U): (T | U)[] { return [a, b]; }
pair2(1, "b");       // (string | number)[]
```

### Inference from callbacks
```ts
function map<T, U>(items: T[], fn: (item: T) => U): U[] { /* ... */ return []; }

map([1, 2, 3], n => n.toString());   // T = number (from items), U = string (from callback return)
```
Order matters: TypeScript infers `T` from `items` first so that `n` in the callback is typed. Put the data parameter before the callback.

### Return type as an inference site (contextual)
If the result is assigned to a typed target, that target can influence inference:

```ts
function empty<T>(): T[] { return []; }
const xs: string[] = empty();    // T = string
const ys = empty();              // T = unknown
```

### Literal vs. widened inference
```ts
function box<T>(x: T): { value: T } { return { value: x }; }
box("a");                        // { value: string }   (literal widened)

function boxLiteral<T extends string>(x: T): { value: T } { return { value: x }; }
boxLiteral("a");                 // { value: "a" }      (constraint preserves the literal)
```
A constraint to a primitive keeps literal types. For objects and arrays use a `const` type parameter:

```ts
function tuple<const T extends readonly unknown[]>(items: T): T { return items; }

tuple([1, "a"]);   // readonly [1, "a"]
```
Without `const`, the same call gives `(string | number)[]`. (`const` type parameters need TypeScript 5.0+.)

### When there are no candidates
TypeScript falls back to the default, then the constraint, then `unknown`:

```ts
function make<T = string>(): T[] { return []; }
make();   // string[]

function make2<T extends object>(): T[] { return []; }
make2();  // object[]
```

### Partial inference does not exist
You either let TypeScript infer **all** type arguments or provide **all** of them (those without defaults):

```ts
function convert<TIn, TOut>(x: TIn, fn: (x: TIn) => TOut): TOut { return fn(x); }

convert<string>("a", s => s.length);   // Error: Expected 2 type arguments, but got 1
```
Workarounds:
1. Give a trailing parameter a default.
2. **Curry** so each function infers one thing:

```ts
function convertFrom<TIn>() {
  return <TOut>(x: TIn, fn: (x: TIn) => TOut): TOut => fn(x);
}
convertFrom<string>()("a", s => s.length);   // number
```

### `NoInfer<T>` (TypeScript 5.4+)
Prevents a position from contributing inference candidates, so another position decides:

```ts
function createStreetLight<C extends string>(colors: C[], defaultColor?: NoInfer<C>) {
  /* ... */
}

createStreetLight(["red", "yellow", "green"], "red");    // ok
createStreetLight(["red", "yellow", "green"], "blue");   // Error: "blue" is not one of the colors
```
Without `NoInfer`, `"blue"` would also become an inference candidate, `C` would widen to include it, and the mistake would compile.

### Candidates from multiple positions
- Covariant positions (ordinary parameters): TypeScript picks one candidate that all the others are assignable to (the best common supertype). If none qualifies you get an error, as with `pair(1, "b")` above. When the candidates are string or number literals and the constraint allows it, they are combined into a union of literals.
- Contravariant positions (callback parameters): candidates are intersected.

When no single candidate satisfies all uses you get an error and must widen the signature.

### Inference through generic types
TypeScript can infer from structure:

```ts
function unwrap<T>(box: { value: T }): T { return box.value; }
unwrap({ value: 42 });          // number

function first<T>(items: readonly T[]): T | undefined { return items[0]; }

function promiseValue<T>(p: Promise<T>): T { throw new Error(); }
promiseValue(Promise.resolve("x"));   // string
```
For deeper extraction use [`infer`](../10-advanced-types/02-infer.md) in conditional types.

### Inference through classes and `new`
```ts
class Box<T> { constructor(public value: T) {} }
new Box(1);   // Box<number>
```

### Higher-order generics
TypeScript can propagate generic parameters through function composition in simple cases:

```ts
function compose<A, B, C>(f: (a: A) => B, g: (b: B) => C): (a: A) => C {
  return a => g(f(a));
}
const len = compose((s: string) => s.trim(), s => s.length);   // (a: string) => number
```

## Debugging Inference
1. Hover over the call or the result to see the inferred type arguments.
2. Temporarily add explicit type arguments (`f<string>(...)`) to see whether the error moves.
3. Simplify the call into variables to see which argument produced which candidate.
4. Check for `any` in arguments: it infers `any` for `T`.
5. If you got `unknown`, no inference site matched; add the type parameter to a parameter.

## Common Mistakes
- Placing the callback before the data, so the callback parameter cannot be inferred.
- Expecting partial inference.
- Using a type parameter that appears only in the return type.
- Getting `string` where you wanted a literal (add `extends string` or `const T`).
- Passing `any`, which poisons inferred types.

## Best Practices
- Design signatures so callers need no explicit type arguments.
- Put data parameters first, callbacks last.
- Use constraints or `const` type parameters to keep literals.
- Use `NoInfer` to control which argument decides `T`.
- If a function needs both inferred and explicit arguments, curry it.

## Interview Questions
- How does TypeScript infer `T` from multiple arguments?
- Why does `identity("a")` give `string` but `<T extends string>` gives `"a"`?
- Why can't you supply only some type arguments?
- What does `NoInfer` do and why would you use it?

## Quick Reference
```ts
<T extends string>(x: T)            // keep string literals
<const T extends readonly unknown[]>(x: T)   // keep tuple/literals (TS 5.0+)
<T>(items: T[], fn: (x: T) => U)    // data first, callback second
NoInfer<T>                          // do not infer from this position (TS 5.4+)
f()()                               // currying to split inference
```

## Related Topics
- [generic-functions.md](00-generic-functions.md)
- [Type widening and inference](../14-type-system-internals/03-type-widening-and-inference.md)
- [Conditional types and `infer`](../10-advanced-types/02-infer.md)
- [Reading type errors](../18-testing-and-debugging/06-reading-type-errors.md)
