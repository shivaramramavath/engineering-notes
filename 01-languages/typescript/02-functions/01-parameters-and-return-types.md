# Parameters and Return Types

## Definition
Parameter types describe what a function accepts (required, optional, default, rest, destructured); the return type describes what it produces.

## Why It Matters
The function signature is the contract between caller and implementation. Most type errors you will see in day-to-day TypeScript originate at function boundaries.

## Prerequisites
[function-types.md](00-function-types.md)

## Syntax

```ts
function format(
  value: number,               // required
  currency?: string,           // optional
  decimals: number = 2,        // default
  ...tags: string[]            // rest
): string {
  return `${value.toFixed(decimals)} ${currency ?? "USD"}`;
}
```

## Basic Example

```ts
format(10);                      // ok
format(10, "EUR");               // ok
format(10, "EUR", 0, "a", "b");  // ok
format();                        // Error: Expected at least 1 arguments
format("10");                    // Error: string is not assignable to number
```

## Important Concepts

### Required parameters
Every declared parameter must be supplied unless marked optional or given a default. Extra arguments are an error.

### Optional parameters
`?` makes a parameter optional; its type becomes `T | undefined`. Optional parameters must come **after** required ones.

```ts
function greet(name: string, title?: string) {
  return title ? `${title} ${name}` : name;
}
```

### Default parameters
A default makes the parameter optional and infers its type:

```ts
function pad(text: string, width = 10) {   // width: number
  return text.padEnd(width);
}
```
Callers may pass `undefined` explicitly to trigger the default. A defaulted parameter may appear before required ones (callers pass `undefined` to skip it), but this is usually confusing.

### Rest parameters
Rest parameters must be array or tuple types:

```ts
function sum(...nums: number[]): number {
  return nums.reduce((a, b) => a + b, 0);
}

function log(level: string, ...parts: [string, number?]): void {}   // tuple rest
```

### Spreading arguments
```ts
const args: [number, string] = [1, "a"];
function f(n: number, s: string) {}
f(...args);            // ok, tuple keeps arity
```

### Destructured parameters
Annotate the whole parameter, not each piece:

```ts
function area({ width, height }: { width: number; height: number }): number {
  return width * height;
}

type Options = { retries?: number; timeout?: number };
function request(url: string, { retries = 3, timeout = 1000 }: Options = {}) {}
```
For many parameters, prefer an options object over positional parameters.

### Return types: inference
```ts
function double(n: number) {
  return n * 2;       // inferred number
}

function parse(s: string) {
  if (s === "") return null;
  return Number(s);   // inferred number | null
}
```

### Return types: when to annotate
- **Exported / public functions**: yes. It fixes the contract and catches accidental changes.
- **Recursive functions**: yes (inference may fail).
- **Small local helpers**: inference is fine.

```ts
export function loadUser(id: string): Promise<User | undefined> { /* ... */ }
```

### Type predicates and assertion signatures
Return types can also narrow the argument:
```ts
function isString(x: unknown): x is string { return typeof x === "string"; }
function assertDefined<T>(x: T | undefined): asserts x is T {
  if (x === undefined) throw new Error("undefined");
}
```
See [type guards and assertion functions](../03-unions-and-narrowing/05-type-guards-and-assertion-functions.md).

### `void`, `never` and `Promise`
```ts
function log(): void {}
function fail(): never { throw new Error(); }
async function load(): Promise<string> { return "x"; }
```
An `async` function's return type is always `Promise<...>`; see [async-await](../12-async-and-iteration/02-async-await.md).

### Readonly parameters
```ts
function total(items: readonly number[]): number { /* cannot mutate items */ return 0; }
```

## Common Mistakes
- Putting an optional parameter before a required one.
- Leaving parameters unannotated (implicit `any` is an error under `strict`).
- Treating `param?: T` as `T`; inside the function it is `T | undefined`.
- Boolean "flag" parameters (`f(true, false)`) that are unreadable at call sites. Use an options object or separate functions.
- Relying on inferred return types for large public APIs, so a refactor silently changes the contract.

## Best Practices
- Limit positional parameters to about three; beyond that use an options object.
- Prefer default values over `| undefined` handling inside the body.
- Annotate public return types.
- Use `readonly` array parameters when you do not mutate.

## Interview Questions
- How does a default parameter differ from an optional parameter?
- What is the type of `param` when declared `param?: string`?
- Why must rest parameters be array types?
- When should you annotate a return type explicitly?

## Quick Reference
```ts
(a: string)                    // required
(a?: string)                   // optional, string | undefined
(a = "x")                      // default, string
(...rest: number[])            // rest
({ a, b }: { a: number; b: string })   // destructured
(): void   (): never   (): Promise<T>  // special returns
```

## Related Topics
- [function-types.md](00-function-types.md)
- [function-overloads.md](03-function-overloads.md)
- [Generic functions](../06-generics/00-generic-functions.md)
- [Null and undefined](../01-fundamentals/08-null-and-undefined.md)
