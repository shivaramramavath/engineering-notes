# Type Guards and Assertion Functions

## Definition
- A **type guard** is a function whose return type is a **type predicate** (`x is T`). When it returns `true`, the compiler narrows the argument to `T`.
- An **assertion function** uses `asserts x is T` or `asserts condition`. If it returns normally, the argument is narrowed afterwards; otherwise it throws.

## Why It Matters
Built-in narrowing (`typeof`, `instanceof`, `in`) does not cover every case, and narrowing does not pass through ordinary helper functions. Custom guards let you package a check once and reuse the narrowing everywhere, including validating `unknown` input.

## Prerequisites
[type-narrowing.md](03-type-narrowing.md), [function types](../02-functions/00-function-types.md)

## Syntax

```ts
// type guard
function isString(x: unknown): x is string {
  return typeof x === "string";
}

// assertion function
function assertIsString(x: unknown): asserts x is string {
  if (typeof x !== "string") throw new TypeError("Expected string");
}

// assertion on a condition
function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}
```

## Basic Example

```ts
function process(value: string | number) {
  if (isString(value)) {
    value.toUpperCase();   // string
  } else {
    value.toFixed(2);      // number
  }
}

function run(input: unknown) {
  assertIsString(input);
  input.toUpperCase();     // string from here on
}
```

## How It Works
The compiler trusts the predicate: in the `true` branch the argument has type `T`, in the `false` branch it is narrowed to exclude `T`. For `asserts`, the code after the call is narrowed because execution only continues when the assertion passed.

**The compiler does not verify that your function body matches the predicate.** A wrong guard compiles fine and creates unsound types:

```ts
function isNumber(x: unknown): x is number {
  return true;   // lies; no error
}
```

## Important Concepts

### Guards for object shapes
```ts
type User = { id: number; name: string };

function isUser(x: unknown): x is User {
  return (
    typeof x === "object" &&
    x !== null &&
    "id" in x && typeof x.id === "number" &&
    "name" in x && typeof x.name === "string"
  );
}
```
For anything non-trivial, prefer a schema library (Zod and others) to hand-written guards; see [schema validation](../15-runtime-validation/01-schema-validation.md).

### Guards for discriminated members
```ts
type Event = { kind: "click"; x: number } | { kind: "key"; code: string };

const isClick = (e: Event): e is Extract<Event, { kind: "click" }> => e.kind === "click";
```

### Filtering arrays
```ts
const items: (string | undefined)[] = ["a", undefined, "b"];

const defined = items.filter((x): x is string => x !== undefined);   // string[]
```
Recent TypeScript versions can infer this predicate for simple callbacks like `x => x !== undefined`, but writing it explicitly stays clear and works on older versions.

### Generic guards
```ts
function isDefined<T>(x: T | null | undefined): x is T {
  return x != null;
}

const values = [1, null, 2].filter(isDefined);   // number[]
```

### Guards for classes and `this`
```ts
class FileEntry {
  isDirectory(): this is DirEntry { return false; }
}
```

### Assertion functions: invariants and parsing
```ts
function assertDefined<T>(x: T | undefined, name = "value"): asserts x is T {
  if (x === undefined) throw new Error(`${name} is undefined`);
}

const el = document.getElementById("app");
assertDefined(el, "#app");
el.focus();   // HTMLElement
```
Assertion functions must be declared with an explicit type (a function declaration or a typed `const`); an untyped arrow assigned to a `const` will not work as an assertion.

### Guard vs. assertion

| | Type guard | Assertion function |
|---|---|---|
| Return | `boolean` | `void` (throws on failure) |
| Use | `if (isX(v)) { ... }` | `assertX(v); ...` |
| Failure handling | Caller chooses | Throws |

### Negative guards
```ts
if (!isString(v)) {
  v;   // excludes string
}
```

## Common Mistakes
- Writing a guard whose body does not match the predicate (the compiler cannot catch it).
- Guards that only check part of a shape (`typeof x === "object"` and nothing else).
- Forgetting `x !== null` when checking objects.
- Using `as` instead of validating input.
- Not annotating an assertion function properly.

## Best Practices
- Keep guards small and test them (unit tests and [type tests](../18-testing-and-debugging/04-type-testing.md)).
- Prefer discriminated unions so you rarely need custom guards.
- Use schema libraries at boundaries; reserve hand-written guards for simple cases.
- Make `assert*` helpers throw clear errors.

## Security
A lying guard is as unsafe as `any`: it tells the compiler data is valid when it may not be. See [unsafe types and assertions](../23-security/00-unsafe-types-and-assertions.md).

## Interview Questions
- What does `x is string` mean as a return type?
- Does the compiler check that a type guard's body is correct?
- Difference between a type guard and an assertion function?
- How do you filter `undefined` out of an array and get `T[]`?

## Quick Reference
```ts
(x: unknown): x is T           // type guard
(x: unknown): asserts x is T   // assertion function
(c: unknown): asserts c        // asserts truthiness
arr.filter((x): x is T => ...)
```

## Related Topics
- [type-narrowing.md](03-type-narrowing.md)
- [type-assertions-and-satisfies.md](07-type-assertions-and-satisfies.md)
- [Trust boundaries](../15-runtime-validation/00-trust-boundaries.md)
