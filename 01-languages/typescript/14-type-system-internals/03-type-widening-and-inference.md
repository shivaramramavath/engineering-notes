# Type Widening and Inference

TypeScript infers types so you do not have to write them, but inference follows rules you should know: **literals widen** to their base types in mutable positions, **contextual typing** flows from the expected type into the expression, and **generic inference** collects candidates from arguments. Most surprises ("why is this `string` and not `"a"`?", "why did it infer a union?") come from these rules.

**Prerequisites:**
- [Types and inference](../01-fundamentals/00-types-and-inference.md)
- [Literal types and const assertions](../03-unions-and-narrowing/02-literal-types-and-const-assertions.md)
- [Generic inference](../06-generics/05-generic-inference.md)

---

## Widening: `const` vs `let`

```ts
const a = "hello";   // type "hello"   (a literal: it can never change)
let b = "hello";     // type string     (widened: it may be reassigned)

const n = 42;        // 42
let m = 42;          // number
```

A `const` binding cannot change, so the narrow literal type is accurate. A `let` binding can be reassigned to another string, so the literal **widens** to `string`.

### Objects and arrays widen their contents

Even with `const`, the *properties* of an object are mutable, so literals inside widen:

```ts
const config = { mode: "dark", retries: 3 };
// { mode: string; retries: number }

const tuple = [1, "a"];
// (string | number)[]
```

That is why this fails:

```ts
type Mode = "dark" | "light";
function setMode(m: Mode) {}

const config = { mode: "dark" };
setMode(config.mode);   // error: string is not assignable to Mode
```

### Keeping literals

Several tools preserve the narrow types:

```ts
// 1. as const: freezes the whole structure into literals and readonly
const config = { mode: "dark", retries: 3 } as const;
// { readonly mode: "dark"; readonly retries: 3 }

// 2. an annotation: tells the compiler the target
const config2: { mode: Mode } = { mode: "dark" };

// 3. satisfies: checks against a type but keeps the inferred literal types
const config3 = { mode: "dark" } satisfies { mode: Mode };
// config3.mode is "dark"

// 4. a single assertion on a value
const config4 = { mode: "dark" as const };
```

See [type assertions and satisfies](../03-unions-and-narrowing/07-type-assertions-and-satisfies.md).

## Literal freshness (widening literal types)

Internally, a literal like `"hello"` has a *widening* literal type. It stays narrow where the declared type does not need to be mutable (`const`) and widens where it does (`let`, mutable properties, array elements). A literal with an explicit annotation, `as const`, or other context becomes *non-widening*.

```ts
let x = "a";                  // string (widened)
let y: "a" | "b" = "a";       // "a" | "b" declared; assigned "a" narrows (see below)
```

## Narrowing on assignment

A variable with a **declared** union type is narrowed by what you assign:

```ts
let value: string | number = "hello";
value.toUpperCase();          // ok: narrowed to string by assignment

value = 42;
value.toFixed(2);             // ok: now number
```

The declared type is still `string | number`. The **narrowed** type tracks control flow ([type narrowing](../03-unions-and-narrowing/03-type-narrowing.md)).

## Inference from initializers and returns

```ts
const items = [1, 2, 3];                // number[]
const pair = [1, "a"];                  // (string | number)[]

function add(a: number, b: number) {    // return type inferred: number
  return a + b;
}

function pick(flag: boolean) {
  return flag ? "yes" : 0;              // inferred: "yes" | 0
}
```

- Array literals infer a **union** of element types (the "best common type" when one fits, otherwise a union).
- Function return types are inferred from the `return` statements and then widened where appropriate.
- **Empty arrays** `[]` under `noImplicitAny` evolve as you push: `const a = []; a.push(1);` becomes `number[]` through control-flow analysis, until it escapes into another scope where an annotation is needed.

For public functions, **annotate return types**. Inference then cannot change your API when the body changes, and error messages point at the function instead of its callers.

## Contextual typing

When an expression appears where a type is expected, that type flows **into** it:

```ts
const numbers = [1, 2, 3];
numbers.map((n) => n * 2);    // n is number: inferred from numbers' element type

window.addEventListener("click", (e) => {
  e.clientX;                  // e is MouseEvent: inferred from the event name
});

const handler: (s: string) => number = (s) => s.length;   // s is string
```

Contextual typing explains why you rarely annotate callback parameters. It also explains why moving a function out of its context loses the types:

```ts
const onClick = (e) => {};    // error under noImplicitAny: e has no context
```

## Generic inference

For `function f<T>(x: T)`, TypeScript infers `T` from the arguments. With several positions, it gathers **candidates** and picks one:

```ts
function first<T>(a: T, b: T): T { return a; }

first(1, 2);          // T = number (literals 1 and 2 widen to number)
first("a", "b");      // T = string
first(1, "b");        // error: "b" is not assignable to number
```

The last call fails because the candidates `number` and `string` have no common supertype, so the first candidate wins and the second argument errors. To allow it, widen explicitly: `first<string | number>(1, "b")`.

### Literals and `extends`

A `T` constrained to a primitive keeps literals:

```ts
function id<T>(x: T): T { return x; }
const a = id("hi");              // type "hi" (a const result keeps the literal)

function pickKey<K extends string>(k: K): K { return k; }
const b = pickKey("name");       // "name"
```

When `T` has a primitive constraint (`string`, `number`, `extends string`), the inferred literal is preserved instead of widened. This is how functions that need exact key names (`get("user.name")`) work.

### `const` type parameters

TypeScript 5.0 added `const` modifiers on type parameters. They make the argument be inferred as if it had `as const`:

```ts
function tuple<T extends readonly unknown[]>(x: T): T { return x; }
tuple([1, "a"]);                  // (string | number)[]

function tupleConst<const T extends readonly unknown[]>(x: T): T { return x; }
tupleConst([1, "a"]);             // readonly [1, "a"]
```

Use it when callers should not need to write `as const` themselves.

### Controlling inference sites

When one argument should not influence inference of `T`, use `NoInfer<T>` (TS 5.4+):

```ts
function create<C extends string>(colors: C[], initial: NoInfer<C>) {}

create(["red", "green"], "blue");   // error: "blue" is not "red" | "green"
```

See [function and class utilities](../07-utility-types/03-function-and-class-utilities.md).

### Inference from return position

The expected return type is also an inference site:

```ts
const strings: string[] = new Array();   // T inferred as string from the annotation
const p: Promise<number> = new Promise((resolve) => resolve(1));
```

When inference yields `unknown` or `{}`, look for a missing context or add an explicit type argument.

## When inference fails

| Symptom | Cause | Fix |
|---|---|---|
| type is `string`, you wanted `"a"` | widened by `let`/mutable property | `as const`, `satisfies`, or an annotation |
| type is a wide union of unrelated things | mixed array or conflicting candidates | annotate the array, or split it |
| `unknown` or `{}` for a generic | no inference candidates | pass the type argument explicitly |
| parameter implicitly `any` | no contextual type | annotate the parameter, or provide a typed context |
| `'x' implicitly has type 'any' because it does not have a type annotation and is referenced in its own initializer` | circular inference | annotate the variable or function return |
| "Type instantiation is excessively deep" | recursive inference | simplify types, or annotate ([recursive types](../10-advanced-types/05-recursive-types.md)) |

## Important rules and misconceptions

- **`const x = {...}` does not make the contents constant.** Only the binding is constant. Use `as const` or `Object.freeze` for the contents.
- **Inference is not magic "best guess".** It follows fixed rules: initializers, contextual types, return statements, and candidate collection.
- **Annotating can *narrow* what is inferred.** `const x: string | number = 1` is declared `string | number` (and narrowed to `number` by assignment).
- **`as const` is not the same as `readonly`.** It also makes literals non-widening and arrays into readonly tuples.
- **More annotations are not always better.** Over-annotating can widen types and defeat narrowing. Annotate function boundaries and exported values, and let local variables infer.

## Common mistakes

- Passing `config.mode` (widened to `string`) to a parameter expecting a union of literals.
- Letting a public function's return type be inferred, then changing the body and silently changing the API.
- Expecting `let x = []` to be useful without ever pushing or annotating.
- Writing `as Mode` to fix a widening error instead of using `as const` or `satisfies`.
- Using an array literal of mixed shapes and getting a broad union instead of a tuple.

## Debugging

- **Hover** the variable or expression to see the inferred type.
- Use the Playground's `// ^?` annotation, or a helper type to print results.
- Write a one-off annotation to learn the target: `const probe: number = value;` and read the error, which reveals the actual type.
- `type Probe = typeof value;` and hover `Probe` for a stable view.
- When a generic infers `unknown`, add the type argument explicitly (`f<string>(...)`) to confirm everything else is correct, then work out why inference failed.

## Quick summary

- Literals widen in mutable positions: `let`, object properties, and array elements. `const` bindings keep literals.
- Keep literals with `as const`, `satisfies`, an annotation, or `const` type parameters (TS 5.0+).
- Contextual typing flows expected types into callbacks and literals. Lose the context and parameters need annotations.
- Generic inference collects candidates from arguments and the expected return type. Conflicting candidates produce errors, and `NoInfer` stops an argument from contributing.
- Annotate public boundaries, and let local variables infer.

**Next:** [Soundness and escape hatches](./04-soundness-and-escape-hatches.md)
