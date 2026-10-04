# Types and Inference

## Definition
A **type** describes the set of values a variable can hold and the operations allowed on them. An **annotation** is a type you write (`let x: number`). **Inference** is the compiler working out the type for you from the code.

## Why It Matters
TypeScript's value comes from catching mistakes before runtime. Knowing when the compiler already knows a type, and when you must state it, keeps code safe without noise.

## Prerequisites
[00-setup](../00-setup/README.md)

## Syntax

```ts
let age: number = 30;          // explicit annotation
let name = "Ada";              // inferred as string
const id = 7;                  // inferred as the literal type 7
```

Annotation goes after the name with a colon.

## Basic Example

```ts
let count = 0;
count = "five"; // Error: Type 'string' is not assignable to type 'number'
```

No annotation was written; the compiler inferred `number` from `0`.

## How It Works

### `let` widens, `const` does not
```ts
let a = "hi";    // string       (can be reassigned, so it widens)
const b = "hi";  // "hi"         (a literal type; can never change)
```

### Inference from context
```ts
const nums = [1, 2, 3];                  // number[]
const doubled = nums.map(n => n * 2);    // n is inferred as number
```
`n` has no annotation. It is inferred from the type of `nums`. This is called **contextual typing**.

### Return types are inferred
```ts
function add(a: number, b: number) {
  return a + b;   // return type inferred: number
}
```

### Uninitialised variables become `any` (avoid)
```ts
let value;        // implicitly any under some settings; with strict, evolves by assignment
```
Always annotate or initialise.

## When to annotate

| Situation | Annotate? |
|---|---|
| Initialised local variable | No, let inference work |
| Function parameters | **Yes**, always (inference cannot know them) |
| Function return types | Optional for private helpers; **recommended for exported/public functions** |
| Empty array / object that fills later | Yes (`const xs: string[] = []`) |
| Value that must match a specific type | Yes, so errors appear at the assignment |

```ts
const items = [];            // any[] evolving; unclear
const items: string[] = [];  // clear
```

## Common Mistakes
- Annotating everything: `const x: number = 5` adds noise and can hide a more precise literal type.
- Leaving function parameters unannotated and relying on `any` (an error under `noImplicitAny`, which `strict` enables).
- Expecting `let x = "a"` to be typed `"a"`; it is `string`.

## Best Practices
- Annotate function parameters and public return types; infer the rest.
- Hover in the editor to check what was inferred.
- Prefer annotating the **boundary** (function signatures) and trusting inference inside.

## Interview Questions
- What is the difference between type annotation and type inference?
- Why is `const x = "a"` typed differently from `let x = "a"`?
- When should you annotate a return type explicitly?

## Quick Reference
```ts
let a: string = "x";     // annotation
let b = "x";             // inferred string
const c = "x";           // inferred "x"
function f(p: number): string { return String(p); }
```

## Related Topics
- [primitive-types.md](01-primitive-types.md)
- [Literal types](../03-unions-and-narrowing/02-literal-types-and-const-assertions.md)
- [Type widening](../14-type-system-internals/03-type-widening-and-inference.md)
- [Generic inference](../06-generics/05-generic-inference.md)
