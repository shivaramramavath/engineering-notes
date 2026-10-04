# Function Types

## Definition
A **function type** describes the parameters a function accepts and the value it returns, independently of any particular implementation.

## Why It Matters
Functions are values in JavaScript: they are stored in variables, passed as arguments and returned. Typing them correctly is what makes callbacks, higher-order functions and APIs safe.

## Prerequisites
[01-fundamentals](../01-fundamentals/README.md)

## Syntax

### Function type expression
```ts
type Add = (a: number, b: number) => number;

const add: Add = (a, b) => a + b;   // a and b inferred from Add
```
The parameter names in a type are only for documentation; they do not need to match the implementation.

### Declarations and expressions
```ts
function greet(name: string): string {
  return `Hi ${name}`;
}

const greet2 = (name: string): string => `Hi ${name}`;
```

### Call signature in an object type
Use this when the function also has properties:
```ts
type Counter = {
  (): number;           // callable
  reset(): void;        // property
  count: number;
};
```

### Construct signature
```ts
type UserFactory = new (name: string) => User;
```

### Generic function types
```ts
type Identity = <T>(value: T) => T;
const id: Identity = value => value;
```

## Basic Example

```ts
type Predicate = (value: number) => boolean;

function filterNumbers(values: number[], keep: Predicate): number[] {
  return values.filter(keep);
}

filterNumbers([1, 2, 3, 4], n => n % 2 === 0);   // [2, 4]
filterNumbers([1, 2], n => n.toUpperCase());      // Error: 'toUpperCase' does not exist on 'number'
```

## How It Works

### Assignability: parameters
A function with **fewer** parameters can be used where more are expected, because the extra arguments are simply ignored:

```ts
type Handler = (a: number, b: number) => void;

const h1: Handler = (a) => {};                 // ok
const h2: Handler = (a, b, c) => {};           // Error: requires more parameters than provided
```

### Assignability: return types
The implementation's return type must be assignable to the declared return type:

```ts
type Make = () => { id: number };
const make: Make = () => ({ id: 1, extra: true });   // ok: more specific return is fine
```

### Parameter variance
With `strictFunctionTypes` (part of `strict`), function **parameters** are checked contravariantly for function-typed properties and variables:

```ts
type Animal = { name: string };
type Dog = { name: string; bark(): void };

let handleAnimal: (a: Animal) => void = a => {};
let handleDog: (d: Dog) => void = d => d.bark();

handleDog = handleAnimal;   // ok: accepts anything an animal handler accepts
handleAnimal = handleDog;   // Error under strictFunctionTypes
```
Method-style declarations (`handle(a: Animal): void` inside an object type) stay bivariant for historical reasons. Details: [variance](../14-type-system-internals/02-variance.md).

## Important Concepts

### `Function` is too loose
```ts
let f: Function;   // any callable; calls return any and are unchecked
```
Use a specific signature, or `(...args: never[]) => unknown` when you truly need "any function".

### Inferring from existing functions
```ts
function createUser(name: string, age: number) {
  return { name, age };
}

type Args = Parameters<typeof createUser>;     // [name: string, age: number]
type Result = ReturnType<typeof createUser>;   // { name: string; age: number }
```
See [function and class utilities](../07-utility-types/03-function-and-class-utilities.md).

### Functions are objects
You can attach properties (call signature + properties above) and inspect `fn.length` or `fn.name`.

### Arrow functions vs. function declarations
Both can be typed identically. Differences are about `this`, hoisting and `arguments`, not types; see [this-parameters.md](04-this-parameters.md).

## Common Mistakes
- Using `Function` or `any` instead of a signature.
- Forgetting that a type expression needs the arrow (`=>`), not a colon, for the return type: `(a: number) => string`.
- Expecting parameter names to need to match between the type and the implementation.
- Writing `type F = (a) => void` without a parameter type (implicit `any`).

## Best Practices
- Name reusable signatures with a [type alias](../01-fundamentals/04-type-aliases.md).
- Prefer function type expressions unless you need properties or overloads on the callable.
- Annotate parameters and exported return types; let inference handle the rest.

## Interview Questions
- How do you type a function that has both a call signature and properties?
- Why can a function with fewer parameters be assigned to a type with more?
- What does `strictFunctionTypes` change?
- Why is `Function` discouraged?

## Quick Reference
```ts
(a: number, b: number) => number            // function type
{ (x: string): number; prop: boolean }      // call signature + property
new (x: string) => Foo                      // constructor type
<T>(x: T) => T                              // generic function type
Parameters<typeof fn>  ReturnType<typeof fn>
```

## Related Topics
- [parameters-and-return-types.md](01-parameters-and-return-types.md)
- [callbacks.md](02-callbacks.md)
- [Generic functions](../06-generics/00-generic-functions.md)
- [Variance](../14-type-system-internals/02-variance.md)
