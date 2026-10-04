# `this` Parameters

## Definition
In JavaScript, `this` depends on **how** a function is called, not where it is defined. TypeScript lets you declare the expected type of `this` with a fake first parameter named `this`, which is erased at compile time.

## Why It Matters
Losing `this` is a classic JavaScript bug (`undefined` or `window` instead of your object). TypeScript can catch it, but only if `this` is typed and `noImplicitThis` is enabled (part of `strict`).

## Prerequisites
[function-types.md](00-function-types.md), [callbacks.md](02-callbacks.md), basic [classes](../05-classes/00-classes.md)

## Syntax

```ts
function describe(this: { name: string }): string {
  return `I am ${this.name}`;
}

const user = { name: "Ada", describe };
user.describe();      // ok

describe();           // Error: 'this' context of type 'void' is not assignable to { name: string }
```

The `this` parameter does not take up a real argument slot; it exists only for the type checker and disappears in the emitted JavaScript.

## Basic Example

```ts
type Button = { label: string; onClick(this: Button): void };

const button: Button = {
  label: "Save",
  onClick() {
    console.log(this.label);   // this: Button
  },
};

const handler = button.onClick;
handler();   // Error: 'this' context of type 'void' is not assignable to method's 'this' of type 'Button'
```

## How It Works

### Why `this` gets lost
```ts
class Counter {
  count = 0;
  inc() { this.count++; }
}

const c = new Counter();
const fn = c.inc;
fn();                // runtime: TypeError (this is undefined)
setTimeout(c.inc);   // same problem
```
`noImplicitThis` plus a `this: Counter` annotation on `inc` makes the compiler flag these calls.

### Fixes
```ts
// 1. Arrow function class property (this bound lexically)
class Counter {
  count = 0;
  inc = () => { this.count++; };
}

// 2. bind
const fn = c.inc.bind(c);

// 3. wrap at the call site
setTimeout(() => c.inc());
```
Trade-off: arrow properties create a new function per instance (more memory, not on the prototype, harder to override in subclasses).

### Arrow functions have no `this` of their own
They capture `this` from the surrounding scope, so a `this` parameter is not allowed on them:

```ts
const f = (this: Foo) => {};   // Error: An arrow function cannot have a 'this' parameter
```

### `noImplicitThis`
Without annotation, `this` inside a standalone function is `any` unless `noImplicitThis` is on, in which case using it is an error:

```ts
function show() {
  console.log(this.name);   // Error under noImplicitThis: 'this' implicitly has type 'any'
}
```

### `this` types in classes
A method can return `this` for fluent APIs; the type follows subclasses:

```ts
class QueryBuilder {
  protected parts: string[] = [];
  where(c: string): this {
    this.parts.push(c);
    return this;
  }
}
class UserQuery extends QueryBuilder {
  active(): this { return this.where("active = 1"); }
}
new UserQuery().where("a").active();   // still UserQuery
```

### Utility types
```ts
ThisParameterType<typeof describe>   // { name: string }
OmitThisParameter<typeof describe>   // () => string
```

### `ThisType<T>` (object literal methods)
With `noImplicitThis`, the marker type `ThisType<T>` sets the type of `this` inside methods of an object literal. Libraries such as older Vue options-API typings use it; you rarely need it in application code.

```ts
type Methods = { greet(): void } & ThisType<{ name: string }>;
const m: Methods = {
  greet() { console.log(this.name); },
};
```

## Common Mistakes
- Passing a class method as a callback and losing `this`.
- Adding a `this` parameter to an arrow function.
- Expecting the `this` parameter to be a real argument at runtime.
- Leaving `noImplicitThis` off, so mistakes compile.
- Converting every method to an arrow property "to be safe" and then fighting inheritance.

## Best Practices
- Keep `strict` (includes `noImplicitThis`).
- Prefer arrow functions for callbacks that need the surrounding `this`.
- In classes, bind at the call site or use arrow properties only for methods designed to be passed around (event handlers).
- Avoid `this`-dependent standalone functions; take the object as a normal parameter instead.

## Interview Questions
- What is a `this` parameter and does it exist at runtime?
- Why do arrow functions not accept a `this` parameter?
- How can `this` be lost in a class method, and how do you prevent it?
- What does `noImplicitThis` check?

## Quick Reference
```ts
function f(this: Foo, x: number) {}   // typed this
method(): this                         // polymorphic this (fluent)
fn.bind(obj)                           // fix this
ThisParameterType<F>  OmitThisParameter<F>
```

## Related Topics
- [callbacks.md](02-callbacks.md)
- [Classes](../05-classes/00-classes.md)
- [Strict mode](../13-compiler-and-tsconfig/01-strict-mode.md)
- [function-and-class-utilities.md](../07-utility-types/03-function-and-class-utilities.md)
