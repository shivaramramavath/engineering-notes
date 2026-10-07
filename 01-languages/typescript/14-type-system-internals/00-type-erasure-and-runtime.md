# Type Erasure and Runtime

TypeScript's types exist only at compile time. Before your code runs, the compiler **erases** them, and what is left is ordinary JavaScript. That one fact explains most of the "why can't I do this?" questions about TypeScript: why you cannot check `x instanceof SomeInterface`, why `JSON.parse` can lie, why generics cannot be inspected, and why a type assertion does not change a value.

**Prerequisites:**
- [Types and inference](../01-fundamentals/00-types-and-inference.md)
- [Imports and exports](../08-modules/00-imports-and-exports.md) (type-only imports)

---

## What erasure looks like

```ts
// source (TypeScript)
interface User {
  id: number;
  name: string;
}

function greet<T extends User>(user: T, greeting: string = "Hello"): string {
  const label = (user as User).name!;
  return `${greeting}, ${label}`;
}

export type { User };
```

```js
// emitted (JavaScript)
function greet(user, greeting = "Hello") {
  const label = user.name;
  return `${greeting}, ${label}`;
}
```

Everything TypeScript-specific is gone: the interface, the generic parameter, the annotations, the `as` assertion, the `!`, and the type-only export. No trace of `User` remains at runtime.

## What is erased, what is not

| Erased completely | Emits runtime code |
|---|---|
| `interface`, `type` aliases | `enum` (becomes an object) |
| type annotations, generics, `as`, `satisfies`, `!` | `namespace` with values (becomes an object built by a function) |
| `declare` statements, `import type`, `export type` | parameter properties (`constructor(private x: number)`) |
| overload signatures (only the implementation remains) | decorators (calls into helper functions) |
| `abstract`, `readonly`, `private`/`protected` modifiers (access checks are compile-time) | class fields (depending on target and `useDefineForClassFields`) |
| `const enum` (inlined at use sites, usually) | `import`/`export` of values (module format depends on `module`) |

Features in the right column are the ones that do not fit "just delete the types". They are why per-file tools and type-stripping runtimes restrict them (see below).

### Enums are real

```ts
enum Color { Red, Green }
```

```js
var Color;
(function (Color) {
  Color[Color["Red"] = 0] = "Red";
  Color[Color["Green"] = 1] = "Green";
})(Color || (Color = {}));
```

Numeric enums even get a reverse mapping. A `const enum` is substituted inline and produces no object, which is risky across package boundaries and with per-file transpilers. See [enums and const objects](../01-fundamentals/05-enums-and-const-objects.md) for why a union of literals or an `as const` object is often preferred.

### `private` is compile-time only

`private` and `protected` stop the **compiler** from letting you access a member. At runtime the property is an ordinary one. JavaScript's own `#private` fields are enforced at runtime, because they are a real language feature.

```ts
class Account {
  private balance = 0;     // compile-time only
  #secret = "x";           // enforced at runtime
}

(new Account() as any).balance;   // works at runtime
```

## Consequences

### You cannot check types at runtime

```ts
interface Dog { bark(): void }

function isDog(x: unknown) {
  return x instanceof Dog;   // error: 'Dog' only refers to a type, but is being used as a value
}
```

`Dog` does not exist at runtime, so there is nothing to compare against. Use checks JavaScript can perform:

```ts
function isDog(x: unknown): x is Dog {
  return typeof x === "object" && x !== null && "bark" in x
    && typeof (x as { bark: unknown }).bark === "function";
}
```

This is a **type guard**: it runs real JavaScript and tells the compiler what it proved. Other runtime-checkable tools: `typeof`, `instanceof` (for classes, which *are* runtime values), `in`, `Array.isArray`, and a discriminant property in a union ([discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md)).

### Types cannot validate data

```ts
const user: User = JSON.parse(text);   // JSON.parse returns any, so this compiles
user.name.toUpperCase();               // crashes if the JSON has no name
```

The annotation is a claim, not a check. Data from outside the program (HTTP bodies, files, environment variables, storage) needs runtime validation: [trust boundaries](../15-runtime-validation/00-trust-boundaries.md) and [schema validation](../15-runtime-validation/01-schema-validation.md). Libraries such as Zod let you write a runtime schema and **derive** the static type from it, so the two cannot disagree.

### Type assertions do not convert

```ts
const n = "42" as unknown as number;
n + 1;                                // "421": still a string at runtime
```

`as` only changes what the compiler believes. No conversion happens. Use `Number(x)`, `parseInt`, or a parser when you need an actual conversion.

### Generics are erased

```ts
function create<T>(): T {
  return new T();   // error: 'T' only refers to a type
}
```

There is no `T` at runtime. If you need to construct something or branch on the type, pass a runtime value that represents it:

```ts
function create<T>(Ctor: new () => T): T {
  return new Ctor();
}
```

The same applies to `typeof T`, `T[]` checks, and "does `T` extend `string`?" at runtime. Conditional types are compile-time only, so a function cannot behave differently based on `T` itself.

### Overloads exist only for the compiler

```ts
function parse(x: string): number;
function parse(x: number): string;
function parse(x: string | number) { /* the only code that runs */ }
```

The overload signatures disappear. A single implementation handles every call.

### Type-only imports vanish

An import used only as a type is removed from the output. This is why `import type` and the `verbatimModuleSyntax` option exist: they make the distinction explicit, so a side-effect import is not dropped by accident. See [imports and exports](../08-modules/00-imports-and-exports.md).

## The two meanings of `typeof`

```ts
const config = { port: 3000 };

const t = typeof config;          // runtime: the string "object"
type Config = typeof config;      // type level: { port: number }
```

The same keyword does different things in a value position and a type position. The second form is erased. Only the first exists at runtime.

## Runtime type information, deliberately

When you need types at runtime, make the **runtime value the source of truth** and derive types from it:

```ts
const ROLES = ["admin", "editor", "viewer"] as const;
type Role = (typeof ROLES)[number];         // "admin" | "editor" | "viewer"

function isRole(x: string): x is Role {
  return (ROLES as readonly string[]).includes(x);
}
```

The array exists at runtime for the check. The type is derived from it, so they stay in sync.

`emitDecoratorMetadata` is the one place where TypeScript emits some type information: for decorated declarations, it records limited metadata (such as constructor parameter classes), which dependency injection frameworks read. Interfaces and generics still record as `Object`. This is why DI containers use classes and tokens rather than interfaces. See [decorators](../05-classes/07-decorators.md).

## Running TypeScript directly

Some runtimes and tools execute `.ts` files by **stripping** types without checking them (recent Node versions can do this, as can Deno and Bun, and bundlers like esbuild and SWC). Stripping works when erasing the types leaves valid JavaScript, so it handles annotations, interfaces, and generics, but not features that need code generation: `enum`, `namespace` with values, and parameter properties. TypeScript's `erasableSyntaxOnly` option (TS 5.8+) makes the compiler report those so your code stays strip-compatible. Check your runtime's current support and flags.

Because these tools do not type-check, run `tsc --noEmit` in CI to keep type safety.

## Important rules and misconceptions

- **"TypeScript checks types at runtime."** It does not. Nothing in the output enforces your annotations.
- **"An interface is like a Java interface."** It is not a runtime thing. There is no `instanceof` and no reflection.
- **"Generics are like templates and exist at runtime."** They are erased.
- **"A cast converts the value."** It only silences the compiler.
- **"`readonly` and `private` protect data."** They guard against compile-time mistakes. Use `Object.freeze` and `#private` for runtime enforcement.
- **"If it compiles, the data is valid."** Only the code is checked against your declarations, not the data that arrives later.

## Common mistakes

- Using `instanceof` with an interface or type alias.
- Trusting `JSON.parse(...) as T` or `await res.json() as T` for external data.
- Writing `new T()` or `T.name` inside a generic function.
- Using `as` as if it were a conversion.
- Relying on `private` for security.
- Using `enum` or `namespace` and then moving to a type-stripping runtime.
- Assuming type information is available to reflection (it is not, except limited decorator metadata).

## Debugging

- **See the emitted JavaScript.** Run `tsc --outDir /tmp/out` (or use the TypeScript Playground's ".JS" tab) and read what remains. This settles most "does this exist at runtime?" questions.
- **If a check "works in types" but not in practice,** look for the runtime test. If none exists, the check is only a claim.
- **If a value has the wrong shape at runtime,** find where it entered the program and add validation there.
- **Use source maps** (`sourceMap: true`, and `node --enable-source-maps`) so runtime errors point back to TypeScript lines.

## Quick summary

- Types, interfaces, generics, assertions, and annotations are erased. The output is plain JavaScript.
- A few features emit code: `enum`, value `namespace`, parameter properties, decorators, class fields, and module syntax.
- You cannot test for an interface or generic at runtime. Use `typeof`, `instanceof` (classes), `in`, discriminants, and type guards.
- Assertions and annotations are claims, not checks. Validate external data with runtime schemas.
- To have both runtime and compile-time information, define the runtime value first and derive the type from it.

**Next:** [Assignability and subtyping](./01-assignability-and-subtyping.md)