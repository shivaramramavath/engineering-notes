# Reading Type Errors

TypeScript's error messages have a reputation for being cryptic, but they follow a consistent structure, and the compiler is almost always telling you something accurate. Most of the difficulty is *where to look* in a long message and *what the compiler is comparing*. This note teaches you to read an error (bottom first), recognizes the errors you will see most often with their usual fixes, and gives you techniques for the hard cases: huge generic types, overload failures, and errors far from their cause.

**Prerequisites:**
- [Assignability and subtyping](../14-type-system-internals/01-assignability-and-subtyping.md)
- [Type narrowing](../03-unions-and-narrowing/03-type-narrowing.md)
- [Strict mode](../13-compiler-and-tsconfig/01-strict-mode.md)

---

## Anatomy of an error

```text
src/order.ts:12:5 - error TS2322: Type '{ id: number; user: { name: number; }; }' is not assignable to type 'Order'.
  Types of property 'user' are incompatible.
    Type '{ name: number; }' is not assignable to type 'User'.
      Types of property 'name' are incompatible.
        Type 'number' is not assignable to type 'string'.
```

- **Location:** file, line, column. The error underlines the expression the compiler objected to.
- **Code (`TS2322`):** a stable identifier. Searching for the code plus keywords finds explanations, and the code tells you the *kind* of problem.
- **Headline:** the broad claim ("A is not assignable to B").
- **Indented chain:** each level narrows down **where** inside the types the mismatch is.

### Read it from the bottom

The headline describes the whole comparison. The **last line** is usually the actual cause:

```text
Type 'number' is not assignable to type 'string'.
```

Reading up the chain gives the path: `Order` -> `user` -> `name`. So the real problem is: `order.user.name` is a `number` but must be a `string`. Start at the bottom, then use the lines above to locate it.

The compiler may also truncate very long messages. If you see `...`, use `noErrorTruncation` (below) or hover the type in your editor.

## The errors you will see most

| Code | Typical message | Usual meaning and fix |
|---|---|---|
| **TS2322** | `Type 'X' is not assignable to type 'Y'.` | A value of type X is used where Y is required. Fix the value, the declared type, or narrow/convert. |
| **TS2345** | `Argument of type 'X' is not assignable to parameter of type 'Y'.` | Same, for a function argument. Check the call and the parameter's type. |
| **TS2339** | `Property 'nmae' does not exist on type 'User'.` | Typo, wrong type, or the property exists only on some union members. Narrow first. |
| **TS18048** / **TS2532** | `'user' is possibly 'undefined'.` | `strictNullChecks`: handle `undefined` with a check, `?.`, `??`, or an early return. |
| **TS2741** | `Property 'email' is missing in type '{...}' but required in type 'User'.` | An object literal lacks a required property. Add it, or make it optional if it really is. |
| **TS2554** | `Expected 2 arguments, but got 1.` | Wrong number of arguments. Check optional parameters and overloads. |
| **TS2769** | `No overload matches this call.` | None of the function's signatures accept your arguments. See the overload section. |
| **TS7006** | `Parameter 'x' implicitly has an 'any' type.` | `noImplicitAny`: add a type, or provide a context that types it. |
| **TS7053** | `Element implicitly has an 'any' type because expression of type 'string' can't be used to index type '...'.` | Indexing an object with a plain `string`. Use `keyof`, a typed key, or an index signature. |
| **TS2307** | `Cannot find module 'x' or its corresponding type declarations.` | Wrong path, missing package or types, or module resolution mismatch. |
| **TS2304** | `Cannot find name 'x'.` | Missing import or declaration, or a missing `lib`/types entry. |
| **TS2564** | `Property 'name' has no initializer and is not definitely assigned in the constructor.` | `strictPropertyInitialization`: initialize it, make it optional, or use `!` if something external sets it. |
| **TS2366** | `Function lacks ending return statement and return type does not include 'undefined'.` | Some code path returns nothing. Add a return, or handle all cases (a `never` check). |
| **TS2367** | `This comparison appears to be unintentional because the types 'A' and 'B' have no overlap.` | You compare values that can never be equal. Usually a typo or a wrong literal. |
| **TS2349** | `This expression is not callable.` | Calling something that is not a function (often a union whose members have incompatible signatures). |
| **TS2589** | `Type instantiation is excessively deep and possibly infinite.` | A recursive type hit the depth limit. Simplify, or use tail recursion ([recursive types](../10-advanced-types/05-recursive-types.md)). |

Exact wording and some code numbers change between TypeScript versions (for example, "possibly undefined" is reported as TS18048 in newer versions and TS2532 in older ones), so search by the shape of the message as well as the number.

## Worked examples

### 1. The nested mismatch (TS2322)

```ts
interface User { name: string }
interface Order { id: number; user: User }

const order: Order = { id: 1, user: { name: 42 } };
```

Read bottom up: `number` is not assignable to `string`, at `user.name`. Fix: pass a string, or change `User.name` if the type is wrong.

### 2. A property on only some union members (TS2339)

```ts
type Shape = { kind: "circle"; radius: number } | { kind: "square"; size: number };

function area(s: Shape) {
  return s.radius * s.radius;      // error: Property 'radius' does not exist on type 'Shape'.
                                   //        Property 'radius' does not exist on type '{ kind: "square"; ... }'.
}
```

The second line names the member that lacks the property. Fix by narrowing first:

```ts
if (s.kind === "circle") return s.radius ** 2;
```

### 3. Possibly undefined (TS18048)

```ts
const el = document.getElementById("app");
el.textContent = "hi";             // error: 'el' is possibly 'null'.
```

`getElementById` returns `HTMLElement | null`. Fix with a check, an early return, or a helper that throws if it is missing. Avoid `el!` unless you have already guaranteed it ([soundness and escape hatches](../14-type-system-internals/04-soundness-and-escape-hatches.md)).

### 4. Indexing with a string (TS7053)

```ts
const scores = { alice: 1, bob: 2 };
function get(name: string) {
  return scores[name];             // error: Element implicitly has an 'any' type because expression of type 'string' can't be used to index type '{ alice: number; bob: number; }'.
}
```

The compiler cannot know that `name` is `"alice" | "bob"`. Fix by typing the parameter (`name: keyof typeof scores`), by declaring the object as `Record<string, number>`, or by checking that the key is valid first.

### 5. Missing return path (TS2366)

```ts
function label(status: "ok" | "error" | "pending"): string {
  switch (status) {
    case "ok": return "OK";
    case "error": return "Error";
  }
}                                  // error: Function lacks ending return statement...
```

You forgot `"pending"`. Add the case, or end with a `never` check so future union members are caught ([exhaustiveness checking](../03-unions-and-narrowing/06-exhaustiveness-checking.md)).

### 6. Inference gone wrong (TS2345)

```ts
const items = [];                   // evolves, but if it escapes it becomes any[]
function add(list: string[], x: string) {}
add(items, 5);                      // error: Argument of type 'number' is not assignable to parameter of type 'string'.
```

The error points at the second argument. Compare the **argument type** with the **parameter type** in the message, then find where the argument's type came from.

## Overload errors (TS2769)

```text
No overload matches this call.
  Overload 1 of 2, '(a: string): string', gave the following error.
    Argument of type 'number' is not assignable to parameter of type 'string'.
  Overload 2 of 2, '(a: boolean): boolean', gave the following error.
    Argument of type 'number' is not assignable to parameter of type 'boolean'.
```

The compiler tried each signature and lists why each failed. Practical reading:

- Look for the overload **closest to what you meant** and read its specific error.
- If every overload fails on the same argument, that argument is the problem.
- Sometimes the issue is a *different* argument than the one underlined. TypeScript reports against one overload's failure, but another argument may be wrong.
- Many libraries (event handlers, `addEventListener`, ORMs) use overloads heavily. Check the documentation for the intended call form ([function overloads](../02-functions/03-function-overloads.md)).

## Techniques for hard errors

### Isolate with named intermediate values

When an expression is long, break it into steps with annotations to see which step breaks:

```ts
const query = buildQuery(filters);              // hover: what type is this?
const rows: UserRow[] = await db.run(query);    // does the mismatch appear here?
const users: User[] = rows.map(toUser);         // or here?
```

An annotation (`const x: Expected = value`) turns a vague downstream error into a precise one at the point where the types diverge.

### Hover and inspect the inferred type

Hover in the editor to see what the compiler *thinks* a value is. Often the surprise is there: `string` instead of a literal, `any[]` instead of a tuple, `unknown` where you expected a type, or a union you did not intend ([type widening and inference](../14-type-system-internals/03-type-widening-and-inference.md)). In the Playground, add `// ^?` under an expression to print its type.

### Print the full type

Long types are truncated in messages. Two options:

```jsonc
{ "compilerOptions": { "noErrorTruncation": true } }   // show full types in errors (can be huge)
```

Or flatten intersections and mapped types so hover shows a clean shape:

```ts
type Expand<T> = { [K in keyof T]: T[K] } & {};
type Shown = Expand<ComplexType>;       // hover Shown
```

Editor extensions that reformat TypeScript errors into readable, colored forms exist too (search for "pretty TypeScript errors" for your editor).

### Reduce to a minimal example

Copy the failing types and call into a new file (or the Playground) and delete everything unrelated. If the error disappears when you remove something, that something is the cause. A minimal example is also what you want when asking for help or filing an issue.

### Check generics from the outside in

For generic function errors, ask three questions:

1. **What was `T` inferred as?** Hover the call, or specify it explicitly (`f<string>(x)`) to see if the error changes.
2. **Does `T` satisfy its constraint?** Constraint failures are reported against the constraint type.
3. **Which argument is giving `T` its candidates?** Conflicting candidates produce an error on the later argument ([type widening and inference](../14-type-system-internals/03-type-widening-and-inference.md)).

### Remember the direction of the check

"A is not assignable to B" has a direction. For function parameters it is **reversed** under `strictFunctionTypes` (contravariance), so an error about a callback's parameter type can look backwards until you recall that ([variance](../14-type-system-internals/02-variance.md)).

### Errors far from the cause

A wrong type often flows quietly through several steps and fails somewhere distant. Trace backwards: where did this value come from, and where was its type first decided? Look for an `as` assertion, an `any`, or a function whose return type is inferred and changed. **Adding explicit return types** to functions along the path makes errors appear at the function that is actually wrong.

## What to do and not do

**Do:**

- Fix the cause: correct the value, the type, or the logic.
- Narrow with checks, guards, `in`, `instanceof`, or discriminants.
- Use `satisfies` to check a literal against a type while keeping its precise type.
- Add explicit annotations at function boundaries.

**Avoid as a first resort:**

- `as any` or `as unknown as T` to silence the error. It removes the compiler's help and moves the bug to runtime.
- `// @ts-ignore`. If a suppression is truly needed, use `// @ts-expect-error` with a reason, so it is flagged when it becomes unnecessary.
- Non-null `!` where you have not actually guaranteed the value.

An error that you "just want to go away" is usually pointing at a real mismatch. Spend a minute understanding it before reaching for a cast.

## Common mistakes

- Reading only the first line of a multi-line error.
- Fixing the underlined expression when the mistake is elsewhere (a wrong declared type, an earlier inference).
- Silencing errors with `any`, then hitting the same problem at runtime.
- Ignoring the **error code**, which makes searching and recognizing patterns harder.
- Fighting inference instead of annotating at a boundary.
- Misreading "not assignable" direction in callback and generic errors.
- Debugging a wall of generic type output instead of isolating a smaller case.
- Treating TypeScript errors as noise rather than information about real mismatches.

## Debugging

- Hover every name in the failing expression and compare what you see with what you expect.
- Copy the error code and the key phrase into a search to find explanations and known causes.
- Reproduce in the Playground at the same TypeScript version, since messages and behavior vary by version.
- Run `tsc --noEmit --pretty` in the terminal for clearer formatted output, and `--noErrorTruncation` for full types.
- If the editor and `tsc` disagree, check the TypeScript version and which `tsconfig` each uses ([compiler options](../13-compiler-and-tsconfig/00-compiler-options.md)).
- If an error appeared after an upgrade, compare the release notes. New checks and stricter inference are common causes.

## Quick summary

- An error has a location, a code, a headline, and an indented chain. **Read the last line first**: it is usually the root mismatch, and the lines above give the path to it.
- Learn the common codes: TS2322/2345 (not assignable), TS2339 (property missing), TS18048 (possibly undefined), TS2769 (overloads), TS7006/7053 (implicit any), TS2307 (cannot find module), TS2564 (uninitialized property).
- Isolate by naming intermediate values and annotating, and inspect inferred types with hover or `// ^?`.
- For long types, use `noErrorTruncation` or an `Expand` helper. For overloads, find the closest signature and read its error.
- Trace errors back to where the type was decided: assertions, `any`, and inferred returns. Fix the cause rather than silencing with casts.

**Next:** [19 React and Frontend](../19-react-and-frontend/README.md)
