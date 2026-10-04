# Function Overloads

## Definition
**Overloads** let a single function declare several call signatures, each with its own parameter and return types, backed by one implementation.

## Why It Matters
Some functions legitimately return different types depending on the arguments (`document.createElement("a")` returns `HTMLAnchorElement`). Overloads express that relationship precisely. They are also easy to misuse.

## Prerequisites
[parameters-and-return-types.md](01-parameters-and-return-types.md), basic [union types](../03-unions-and-narrowing/00-union-types.md)

## Syntax

```ts
// overload signatures (visible to callers)
function parse(input: string): number;
function parse(input: string[]): number[];

// implementation signature (NOT visible to callers)
function parse(input: string | string[]): number | number[] {
  return Array.isArray(input) ? input.map(Number) : Number(input);
}
```

## Basic Example

```ts
const a = parse("42");          // number
const b = parse(["1", "2"]);    // number[]
const c = parse(5);             // Error: No overload matches this call
```

## How It Works
- Callers see only the overload signatures. The implementation signature is hidden.
- The implementation must be compatible with **every** overload (its parameters and return type must be broad enough).
- TypeScript tries overloads **top to bottom** and picks the first match, so put the most specific signatures first.
- The implementation body is checked against the implementation signature only. TypeScript does not verify that each overload returns the right type for its inputs; you must keep them consistent.

## Important Concepts

### Order matters
```ts
function f(x: string): string;
function f(x: unknown): number;     // catch-all last
function f(x: unknown): string | number {
  return typeof x === "string" ? x : 0;
}
```
Putting `unknown` first would match everything and hide the specific signature.

### Distinct return per argument shape
```ts
function createElement(tag: "a"): HTMLAnchorElement;
function createElement(tag: "canvas"): HTMLCanvasElement;
function createElement(tag: string): HTMLElement;
function createElement(tag: string): HTMLElement {
  return document.createElement(tag);
}
```

### Overloads in classes and interfaces
```ts
class Store {
  get(key: string): string | undefined;
  get(key: string, fallback: string): string;
  get(key: string, fallback?: string) {
    return this.data[key] ?? fallback;
  }
  private data: Record<string, string> = {};
}
```

### Often better alternatives

**1. Union parameter** (return type does not depend on input):
```ts
// Overloads not needed
function len(x: string | unknown[]): number {
  return x.length;
}
```

**2. Generic with a conditional/mapped return**:
```ts
function wrap<T extends string | string[]>(x: T): T extends string ? number : number[];
```

**3. Optional parameter** instead of two overloads that differ only by an extra argument:
```ts
function pad(s: string, width?: number): string { return s; }
```

**4. Separate, well-named functions** (`parseOne`, `parseMany`) are often the clearest API.

### When overloads are the right choice
- The return type depends on the argument *type or literal* and a generic would be hard to read.
- You are typing an existing JavaScript API with multiple call forms.
- Different argument counts imply different meaning (e.g., `on(event, handler)` vs `on(handler)`).

### Declaring overloads for function types
```ts
type Parser = {
  (input: string): number;
  (input: string[]): number[];
};
```

## Common Mistakes
- Believing the implementation signature is callable; it is not.
- Writing overloads that differ only in return type (the compiler cannot choose between them).
- Placing broad signatures before specific ones.
- Using overloads where a union parameter or an optional parameter is enough.
- Letting overloads and implementation drift apart; the compiler will not catch a wrong return.

## Best Practices
- Prefer unions, generics and optional parameters first; use overloads when the return type truly varies.
- Keep the number of overloads small (2-4).
- Order from most specific to most general.
- Add tests (including type tests) for each overload; see [type testing](../18-testing-and-debugging/04-type-testing.md).

## Interview Questions
- Can callers see the implementation signature?
- In what order does TypeScript try overloads?
- When would you choose a union parameter over overloads?
- Does TypeScript check that overload return types match the implementation body?

## Quick Reference
```ts
function f(a: string): string;      // overload 1
function f(a: number): number;      // overload 2
function f(a: string | number) {    // implementation (hidden)
  return a;
}
```

## Related Topics
- [parameters-and-return-types.md](01-parameters-and-return-types.md)
- [Union types](../03-unions-and-narrowing/00-union-types.md)
- [Generic functions](../06-generics/00-generic-functions.md)
- [Conditional types](../10-advanced-types/00-conditional-types.md)
