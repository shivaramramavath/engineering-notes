# Callbacks

## Definition
A **callback** is a function passed as an argument to another function, to be called later (or repeatedly) by it.

## Why It Matters
Array methods, event handlers, timers, Node APIs and most libraries are built on callbacks. Typing them well gives you inferred parameters inside the callback body and errors when the shape is wrong.

## Prerequisites
[function-types.md](00-function-types.md), [parameters-and-return-types.md](01-parameters-and-return-types.md)

## Syntax

```ts
function repeat(times: number, action: (index: number) => void): void {
  for (let i = 0; i < times; i++) action(i);
}

repeat(3, i => console.log(i));   // i inferred as number
```

## Basic Example

```ts
type Callback<T> = (error: Error | null, result?: T) => void;

function readConfig(path: string, done: Callback<string>): void {
  // ...
  done(null, "contents");
}

readConfig("a.json", (err, data) => {
  if (err) return console.error(err.message);
  console.log(data?.length);
});
```

## How It Works

### Contextual typing
When a function expression is passed where a function type is expected, its parameters are typed from that expected type, so you need no annotations:

```ts
const nums = [1, 2, 3];
nums.map(n => n.toFixed(1));   // n: number
nums.map(n => n.toUpperCase()); // Error
```
Contextual typing only applies when the callback is written inline or assigned to a typed variable. If you define it separately, annotate it:

```ts
const handler = (n) => n * 2;           // Error: implicit any
const handler2 = (n: number) => n * 2;  // ok
nums.map(handler2);
```

### `void` return types accept any return value
```ts
type Visitor = (value: number) => void;

const v: Visitor = n => n * 2;    // ok: return value ignored by the type
[1, 2].forEach(n => results.push(n));  // push returns number, still fine
```
The caller cannot use the returned value because the type says `void`. A callback that must return something should declare it (`=> boolean`, `=> T`).

### Fewer parameters are fine
```ts
[1, 2, 3].forEach(n => console.log(n));   // ignores index and array
```

### Optional callback parameters
When *you* write a function that calls a callback, do not mark callback parameters optional unless the callback might truly not receive them:

```ts
// Bad: forces the callback to check
type Bad = (value: number, index?: number) => void;

// Good: the argument is always provided
type Good = (value: number, index: number) => void;
```

### Generic callbacks
```ts
function mapValues<T, U>(items: T[], fn: (item: T) => U): U[] {
  return items.map(fn);
}

const lengths = mapValues(["a", "bb"], s => s.length);   // number[]
```
`T` is inferred from `items`, then `U` from the callback's return. See [generic functions](../06-generics/00-generic-functions.md).

### Async callbacks
```ts
type AsyncTask<T> = () => Promise<T>;

async function retry<T>(task: AsyncTask<T>, attempts: number): Promise<T> {
  let lastError: unknown;
  for (let i = 0; i < attempts; i++) {
    try { return await task(); }
    catch (e) { lastError = e; }
  }
  throw lastError;
}
```
Passing an `async` function to a callback typed `=> void` compiles, but any rejection is **unhandled**. Prefer `=> Promise<void> | void` where you intend to await it.

### Event handlers
Library types (`MouseEvent`, `Request`) provide the callback signature; inline handlers are typed contextually. See [React event types](../19-react-and-frontend/01-event-types.md).

### Error-first callbacks (Node style)
```ts
type NodeCallback<T> = (err: NodeJS.ErrnoException | null, value?: T) => void;
```
Today most Node APIs also offer Promise versions; prefer those in new code.

## Common Mistakes
- Writing a separate callback without parameter types and getting implicit `any`.
- Using `Function` as the callback type.
- Passing an `async` callback to a `void`-returning parameter and losing errors.
- Making callback parameters optional "just in case".
- Forgetting `this` is not preserved when passing a method: `items.forEach(obj.method)`. See [this-parameters.md](04-this-parameters.md).

## Best Practices
- Define a named alias for callback shapes used in more than one place.
- Let contextual typing do the work for inline callbacks.
- Use generics so callback parameter and return types flow from the inputs.
- Prefer Promises/async over callback nesting for asynchronous work.

## Interview Questions
- How does TypeScript know the type of `n` in `[1,2,3].map(n => ...)`?
- Why does `forEach(n => arr.push(n))` compile when `forEach` expects `void`?
- What is the danger of passing an `async` function where a `void` callback is expected?
- When does contextual typing not apply?

## Quick Reference
```ts
(item: T, index: number) => void          // visitor
(item: T) => boolean                      // predicate
(err: Error | null, data?: T) => void     // error-first
() => Promise<T>                          // async task
```

## Related Topics
- [function-types.md](00-function-types.md)
- [Generic functions](../06-generics/00-generic-functions.md)
- [Promises](../12-async-and-iteration/01-promises.md)
- [Concurrency patterns](../12-async-and-iteration/05-concurrency-patterns.md)
