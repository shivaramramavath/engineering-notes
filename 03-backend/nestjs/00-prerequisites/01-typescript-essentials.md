# TypeScript Essentials

TypeScript is a statically typed superset of JavaScript that compiles to plain JavaScript. NestJS is written in TypeScript, and its core ideas (constructor injection, DTO classes, decorators, generics-based repositories) are expressed through TypeScript features. This file covers the subset of TypeScript you need to read and write NestJS code confidently, and the one concept that causes the most NestJS confusion: **types are erased at runtime**.

---

## Overview

**What it is.** TypeScript adds a type system and a few syntax extensions (interfaces, enums, access modifiers, decorators) on top of JavaScript. The compiler (`tsc`) checks types and emits JavaScript. Node.js never sees your types.

**Why it exists.** JavaScript detects type mistakes at runtime, often in production. TypeScript moves a large class of mistakes (typos, wrong argument shapes, missing `null` checks) to compile time and gives editors enough information for autocomplete and safe refactoring.

**Where it is used.** Frontend frameworks, Node.js backends, CLIs, libraries. NestJS requires it in practice: the framework relies on decorator metadata that only the TypeScript compiler emits.

**Why you should understand it for NestJS.**

- Constructor injection is declared with TypeScript syntax (`constructor(private readonly users: UsersService) {}`).
- Whether a type is a `class` or an `interface` decides if Nest can inject it, validate it, or serialize it.
- Generics power reusable repositories, pagination wrappers, and base classes.
- `tsconfig.json` flags (`experimentalDecorators`, `emitDecoratorMetadata`, `strict`) directly change framework behavior.

---

## Mental Model

TypeScript is a **compile-time overlay**. You write annotated code, the compiler verifies it, then strips the annotations.

```text
  user.service.ts                 user.service.js
 ┌───────────────────────┐       ┌──────────────────────┐
 │ class UserService {   │       │ class UserService {  │
 │   find(id: number):   │  tsc  │   find(id) {         │
 │     User | undefined  │ ────► │     ...              │
 │ }                     │       │ }                    │
 └───────────────────────┘       └──────────────────────┘
        types + code                  code only
            │                              │
            ▼                              ▼
   type-checked by tsc            executed by Node.js
   (errors stop the build)        (knows nothing of types)
```

Rule of thumb: **if it disappears when you delete the colon annotations, it is a type and does not exist at runtime. If it survives, it is a value.**

| Construct | Exists at runtime? |
|---|---|
| `type`, `interface` | No |
| Type annotations, generics, `as` casts | No |
| `class` | Yes (compiled to a function / ES class) |
| `enum` (non-`const`) | Yes (compiled to an object) |
| `const enum` | No (inlined at compile time, unless `preserveConstEnums`) |
| Decorators | Yes (they are ordinary functions that execute) |

---

## Core Concepts

### Static Typing and Inference

You do not have to annotate everything. TypeScript infers types from initializers and return statements.

```typescript
const port = 3000;              // inferred: number
const name = 'api';             // inferred: "api" (literal type, because const)
let retries = 3;                // inferred: number

function add(a: number, b: number) {
  return a + b;                 // return type inferred: number
}
```

**Guideline.** Annotate function parameters and public API boundaries. Let inference handle local variables.

### Primitive Types, `any`, `unknown`, `never`

| Type | Meaning | Use |
|---|---|---|
| `string`, `number`, `boolean`, `bigint`, `symbol` | Primitives | Everyday values |
| `null`, `undefined` | Absence | Explicit absence (see `strictNullChecks`) |
| `any` | Opt out of type checking | Avoid. It disables checks and spreads silently |
| `unknown` | Type-safe "anything" | Input of unknown shape. Must be narrowed before use |
| `never` | A value that cannot occur | Exhaustiveness checks, functions that always throw |
| `void` | Function returns nothing useful | Return type of side-effect functions |

```typescript
function parse(input: unknown): number {
  if (typeof input === 'number') return input;     // narrowed to number
  if (typeof input === 'string') return Number(input);
  throw new TypeError('Cannot parse');
}
```

`unknown` forces you to prove what a value is. `any` lets you pretend. Request bodies from the network are `unknown` in truth, which is why NestJS validates them (see [Validation Pipe](../03-core-concepts/02-validation-and-serialization/02-validation-pipe.md)).

### Objects, Interfaces, and Type Aliases

Both describe the shape of data.

```typescript
interface User {
  id: number;
  email: string;
  nickname?: string;          // optional
  readonly createdAt: Date;   // cannot be reassigned
}

type Role = 'admin' | 'editor' | 'viewer';          // union (only type aliases can name this)
type UserWithRole = User & { role: Role };          // intersection
```

| | `interface` | `type` alias |
|---|---|---|
| Object shapes | Yes | Yes |
| Extend | `extends` (and declaration merging) | `&` intersection |
| Unions, tuples, primitives, mapped/conditional types | No | Yes |
| Declaration merging | Yes | No |
| Typical use | Public object contracts, class contracts | Unions, utility compositions |

Both are **erased** at runtime.

**Structural typing.** TypeScript compares shapes, not names. Any object with the right properties satisfies the type.

```typescript
interface HasId { id: number }
const user = { id: 1, email: 'a@b.c' };
const x: HasId = user;   // OK: user has an id: number
```

### Unions, Literal Types, and Narrowing

A union type means "one of these". Narrowing refines the type inside a branch.

```typescript
type Result =
  | { ok: true; data: string }
  | { ok: false; error: Error };

function handle(r: Result) {
  if (r.ok) {
    console.log(r.data);      // narrowed: { ok: true; data: string }
  } else {
    console.error(r.error);   // narrowed: { ok: false; error: Error }
  }
}
```

A shared literal field (`ok` here) is called a **discriminant**, and the pattern is a **discriminated union**. Other narrowing tools: `typeof`, `instanceof`, `in`, truthiness checks, and custom type guards.

```typescript
function isUser(value: unknown): value is User {
  return typeof value === 'object' && value !== null && 'email' in value;
}
```

Exhaustiveness checking with `never`:

```typescript
function assertNever(x: never): never {
  throw new Error(`Unhandled: ${JSON.stringify(x)}`);
}
```

If you add a new member to a union and forget a `switch` case, passing the value to `assertNever` becomes a compile error.

### Functions

```typescript
function greet(name: string, greeting = 'Hello'): string {
  return `${greeting}, ${name}`;
}

const sum = (...nums: number[]): number => nums.reduce((a, b) => a + b, 0);

type Handler = (req: Request, res: Response) => void;

async function load(id: number): Promise<User> { /* ... */ }
```

Async functions always return `Promise<T>`. See [Node.js Async and Event Loop](./03-nodejs-async-and-event-loop.md).

### Generics

Generics let a function, class, or type work with many types while staying type-safe.

```typescript
function first<T>(items: T[]): T | undefined {
  return items[0];
}

const n = first([1, 2, 3]);        // T inferred as number
const s = first(['a', 'b']);       // T inferred as string
```

Constraints restrict what `T` can be:

```typescript
function byId<T extends { id: number }>(items: T[], id: number): T | undefined {
  return items.find((i) => i.id === id);
}
```

Generic classes and interfaces:

```typescript
interface Repository<T, ID = number> {
  findById(id: ID): Promise<T | null>;
  save(entity: T): Promise<T>;
}

class Page<T> {
  constructor(
    public readonly items: T[],
    public readonly total: number,
  ) {}
}
```

NestJS uses this style for repositories, pagination wrappers, and base services.

### Classes

TypeScript classes extend ES2015 classes with access modifiers, `readonly`, `abstract`, and **parameter properties**.

```typescript
class UsersService {
  private readonly cache = new Map<number, User>();

  // Parameter property: declares AND assigns a field in one step
  constructor(private readonly repo: UsersRepository) {}

  async find(id: number): Promise<User | null> {
    return this.cache.get(id) ?? this.repo.findById(id);
  }
}
```

The constructor above is shorthand for:

```typescript
class UsersService {
  private readonly repo: UsersRepository;
  constructor(repo: UsersRepository) {
    this.repo = repo;
  }
}
```

| Modifier | Effect |
|---|---|
| `public` (default) | Accessible anywhere |
| `private` | Accessible only inside the class (compile-time only) |
| `protected` | Accessible in the class and subclasses |
| `readonly` | Assignable only at declaration or in the constructor |
| `abstract` | Cannot be instantiated. Members may be unimplemented |
| `static` | Belongs to the class, not instances |

`private` is erased at runtime. It is **not** a security boundary. Use JavaScript `#field` for runtime privacy.

`implements` verifies that a class satisfies a contract. It does not inherit anything:

```typescript
interface Logger { log(message: string): void }

class ConsoleLogger implements Logger {
  log(message: string) { console.log(message); }
}
```

### Class vs Interface: the Distinction NestJS Depends On

```typescript
interface CreateUserInterface { email: string }      // erased: no runtime object

class CreateUserDto { email!: string }                // survives: a real constructor function
```

```javascript
// compiled output, simplified
// interface -> nothing
class CreateUserDto {}   // exists
```

Consequences in NestJS:

- A **class** can be a DI token, a validation target (`class-validator`), a Swagger schema source, and a serialization target.
- An **interface** can be none of these, because at runtime it does not exist.
- Rule: **DTOs and anything Nest must inspect at runtime are classes.** Pure compile-time contracts can be interfaces.

### Enums

```typescript
enum Status {
  Active = 'active',
  Disabled = 'disabled',
}
```

Non-`const` enums compile to a runtime object. String enums are safer than numeric enums (numeric enums accept arbitrary numbers in older TypeScript versions and have reverse mappings that surprise people). A common alternative is a union of string literals, which has zero runtime cost:

```typescript
const STATUSES = ['active', 'disabled'] as const;
type Status = (typeof STATUSES)[number];    // 'active' | 'disabled'
```

Use an enum when you need a runtime object to iterate or validate against (for example `@IsEnum(Status)`). Use a literal union when you only need a type.

### Modules (ES Modules Syntax)

```typescript
// users.service.ts
export class UsersService {}
export default class Foo {}

// consumer.ts
import { UsersService } from './users.service';
import type { User } from './user.types';          // type-only: erased completely
import * as path from 'node:path';
```

- A file with a top-level `import`/`export` is a module. Without one, it is a global script.
- `import type` guarantees the import is erased. This matters with decorator metadata (see [TypeScript Decorators](./02-typescript-decorators.md)) where using `import type` on a class you want injected breaks metadata emission.
- NestJS projects typically compile to **CommonJS** (`"module": "commonjs"`) by default. ESM is possible but requires additional configuration. *Verify against the current Nest documentation for your version.*

### Utility Types

Built-in generic helpers for deriving types:

| Utility | Result | Example |
|---|---|---|
| `Partial<T>` | All properties optional | `Partial<User>` for update payloads |
| `Required<T>` | All properties required | |
| `Readonly<T>` | All properties readonly | |
| `Pick<T, K>` | Keep only keys `K` | `Pick<User, 'id' \| 'email'>` |
| `Omit<T, K>` | Remove keys `K` | `Omit<User, 'password'>` |
| `Record<K, V>` | Object with keys `K`, values `V` | `Record<Role, string[]>` |
| `ReturnType<F>` | Return type of function `F` | |
| `Parameters<F>` | Tuple of parameter types | |
| `Awaited<T>` | Unwrap a `Promise` | `Awaited<ReturnType<typeof load>>` |
| `NonNullable<T>` | Remove `null`/`undefined` | |

```typescript
type UpdateUser = Partial<Omit<User, 'id' | 'createdAt'>>;
```

> NestJS provides **runtime** equivalents for DTO classes (`PartialType`, `PickType`, `OmitType`, `IntersectionType` from `@nestjs/mapped-types`). TypeScript's `Partial<T>` is compile-time only and does not carry validation decorators.

### `null`, `undefined`, and Optional Chaining

```typescript
const city = user?.address?.city;        // undefined if any link is null/undefined
const label = user.nickname ?? 'anonymous';  // ?? only falls through for null/undefined
const flag = options.verbose || false;       // || falls through for ALL falsy values (0, '', false)
```

Prefer `??` over `||` for defaults. `0` and `''` are valid values.

The non-null assertion `value!` tells the compiler "trust me". It is checked by nobody. DTO fields use the related **definite assignment assertion** (`email!: string`) because validation, not the constructor, populates them.

---

## How It Works

```text
 .ts source files
        │
        ▼
 ┌───────────────┐   reads tsconfig.json: target, module, strict, decorators...
 │   Parser      │ → builds an AST
 └───────┬───────┘
         ▼
 ┌───────────────┐
 │   Binder      │ → creates symbols and scopes
 └───────┬───────┘
         ▼
 ┌───────────────┐
 │ Type Checker  │ → infers types, reports errors
 └───────┬───────┘
         ▼
 ┌───────────────┐
 │   Emitter     │ → strips types, downlevels syntax to `target`,
 └───────┬───────┘   (optionally) emits decorator metadata,
         │           declaration files (.d.ts), source maps
         ▼
 .js  .d.ts  .js.map
```

Important separation:

1. **Type checking** and **emit** are independent. By default `tsc` emits JavaScript even when type errors exist (unless `noEmitOnError` is set). Build tools such as SWC or esbuild only strip types and do **no** type checking at all.
2. **`target`** controls which JavaScript syntax is emitted (for example `ES2021`). It does not polyfill runtime APIs.
3. **`module`** controls the module format (`commonjs`, `esnext`, `node16`, `nodenext`).

---

## Basic Example

```typescript
// greet.ts
interface Person {
  name: string;
  age?: number;
}

function describe(p: Person): string {
  const age = p.age ?? 'unknown';
  return `${p.name} (age: ${age})`;
}

console.log(describe({ name: 'Ada', age: 36 }));
console.log(describe({ name: 'Linus' }));
```

```bash
npm install --save-dev typescript @types/node
npx tsc --init          # create tsconfig.json
npx tsc                 # type-check and emit .js
node greet.js
```

What happens:

1. `tsc` parses `greet.ts` and checks that `describe` is called with objects matching `Person`.
2. `Person` is erased. `age` is typed `number | undefined`, so `??` is legal and the template string accepts `number | string`.
3. The emitted `greet.js` contains only functions and calls. Calling `describe({ nme: 'x' })` would fail at compile time, not at runtime.

---

## Practical Examples

### 1. Basic: Typed Function with Union Return

```typescript
type Parsed = { ok: true; value: number } | { ok: false; reason: string };

function parsePort(raw: string | undefined): Parsed {
  if (!raw) return { ok: false, reason: 'missing' };
  const value = Number(raw);
  return Number.isInteger(value) && value > 0 && value < 65536
    ? { ok: true, value }
    : { ok: false, reason: 'invalid' };
}
```

### 2. Common: A DTO Class (what NestJS endpoints receive)

```typescript
export class CreateUserDto {
  email!: string;
  password!: string;
  nickname?: string;
}
```

`!` means "assigned later by something other than the constructor" (here, the framework). Without it, `strictPropertyInitialization` reports an error.

### 3. Real-World: Generic Repository and Paginated Result

```typescript
export interface Paginated<T> {
  items: T[];
  total: number;
  page: number;
  pageSize: number;
}

export abstract class BaseRepository<T extends { id: number }> {
  protected abstract readonly table: Map<number, T>;

  async findById(id: number): Promise<T | null> {
    return this.table.get(id) ?? null;
  }

  async list(page: number, pageSize: number): Promise<Paginated<T>> {
    const all = [...this.table.values()];
    const start = (page - 1) * pageSize;
    return { items: all.slice(start, start + pageSize), total: all.length, page, pageSize };
  }
}

class UserRepository extends BaseRepository<User> {
  protected readonly table = new Map<number, User>();
}
```

### 4. Edge Case: Narrowing `unknown` from Untrusted Input

```typescript
function readEmail(body: unknown): string {
  if (
    typeof body === 'object' &&
    body !== null &&
    'email' in body &&
    typeof (body as { email: unknown }).email === 'string'
  ) {
    return (body as { email: string }).email;
  }
  throw new Error('email is required');
}
```

This is the manual form of what a validation layer automates.

### 5. Edge Case: `as` Does Not Validate

```typescript
const data = JSON.parse('{"id":"not-a-number"}') as { id: number };
data.id.toFixed(2);   // compiles, throws TypeError at runtime
```

`as` is a promise to the compiler, not a check. `JSON.parse` returns `any`.

---

## Syntax / API / Commands

### `tsc` CLI

| Command | Purpose |
|---|---|
| `npx tsc --init` | Generate `tsconfig.json` |
| `npx tsc` | Compile using `tsconfig.json` |
| `npx tsc --noEmit` | Type-check only |
| `npx tsc --watch` | Recompile on change |
| `npx tsc --showConfig` | Print the effective, merged configuration |
| `npx tsc --listFiles` | Show every file included in the program |
| `npx tsc --traceResolution` | Debug module resolution |
| `npx tsc --extendedDiagnostics` | Compile-time performance breakdown |

### `tsconfig.json` Options That Matter for NestJS

| Option | Typical value | Why it matters |
|---|---|---|
| `experimentalDecorators` | `true` | Enables the legacy decorator syntax NestJS uses |
| `emitDecoratorMetadata` | `true` | Emits `design:*` metadata that the DI container reads |
| `target` | `ES2021` or newer | Syntax level of emitted JavaScript |
| `module` | `commonjs` | Module format Nest starters use by default |
| `outDir` | `./dist` | Where compiled files go |
| `strict` | `true` (recommended) | Enables the strict family of checks |
| `strictNullChecks` | `true` | `null`/`undefined` are not assignable everywhere |
| `strictPropertyInitialization` | `true` | Class fields must be initialized, or marked with `!` / `?` |
| `esModuleInterop` | `true` | Allows `import x from 'cjs-lib'` ergonomics |
| `skipLibCheck` | `true` | Skip checking `.d.ts` files (faster builds) |
| `incremental` | `true` | Reuse previous compilation info |
| `sourceMap` | `true` | Map stack traces back to `.ts` |
| `declaration` | `true` (libraries) | Emit `.d.ts` |
| `baseUrl` / `paths` | optional | Import aliases (runtime resolution needs extra config) |

A minimal NestJS-style configuration:

```jsonc
{
  "compilerOptions": {
    "module": "commonjs",
    "target": "ES2021",
    "outDir": "./dist",
    "experimentalDecorators": true,
    "emitDecoratorMetadata": true,
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "incremental": true,
    "sourceMap": true
  },
  "include": ["src/**/*"]
}
```

> The `nest new` starter has historically shipped with a relaxed strictness profile. Compare against the current generated `tsconfig.json` for your Nest CLI version, and consider enabling `strict` on new projects.

What `strict: true` enables (non-exhaustive, *verify against the TypeScript handbook for your version*): `noImplicitAny`, `strictNullChecks`, `strictFunctionTypes`, `strictBindCallApply`, `strictPropertyInitialization`, `noImplicitThis`, `useUnknownInCatchVariables`, `alwaysStrict`.

---

## Important Rules

1. **Types are erased.** Never rely on an `interface`, `type`, or generic parameter existing at runtime.
2. **`as` and `!` are unchecked.** They silence the compiler. They do not change runtime behavior.
3. **Compile-time types do not validate runtime data.** Input from HTTP, files, databases, and `JSON.parse` must be validated at the boundary.
4. **Classes are both a type and a value.** Interfaces are only a type.
5. **`private` is compile-time only.** Use `#field` for real runtime privacy.
6. **Structural typing.** Compatibility is decided by shape, not by declared name or inheritance.
7. **`any` is contagious.** One `any` can erase type safety through a whole call chain. Prefer `unknown`.
8. **Type checking and emit are separate.** A build tool that only strips types (SWC, esbuild) will happily emit code that `tsc --noEmit` would reject. Run the type check separately in CI.
9. **`target` is syntax, not APIs.** Using a newer runtime API (for example `structuredClone`) compiles only if the `lib` setting includes its typings, and runs only if the Node.js version provides it.
10. **Generic type arguments are inferred when possible.** Do not write `first<number>(xs)` when `first(xs)` suffices.

---

## Under the Hood

### Type Erasure

After compilation, no type information exists at runtime, except what decorators and `emitDecoratorMetadata` explicitly emit. This is the root reason for several NestJS behaviors:

- Generic type parameters cannot be read at runtime (`Repository<User>` and `Repository<Order>` are the same class).
- Interfaces cannot be injection tokens. You must use a class, a string, or a `Symbol` as the token (see [Injection Tokens](../03-core-concepts/04-modules-and-di/06-injection-tokens-and-optional-dependencies.md)).
- Validation must be re-expressed as runtime decorators (`@IsEmail()`).

### Downleveling

The `target` option rewrites newer syntax into older syntax. With `target: ES5`, classes become functions, `async` becomes a generator-based state machine, and so on. Modern Node.js supports modern syntax natively, so a high `target` produces smaller, faster output.

### Declaration Files

`.d.ts` files contain only types for existing JavaScript. `@types/node` provides typings for Node.js built-ins. Libraries ship their own `.d.ts` or rely on `@types/<package>`.

### Where the Class Compiles To

```typescript
class A {
  constructor(private readonly b: string) {}
}
```

With a modern `target`, this emits an ES class whose constructor assigns `this.b = b`. With `useDefineForClassFields` and a modern target, field declaration semantics follow the ES standard rather than legacy assignment semantics, which can change behavior with decorated properties. *Verify the effect of `useDefineForClassFields` for your target and decorator setup.*

---

## Common Patterns

### Discriminated Unions for State and Results

Use when a value can be one of several shapes and each shape carries different data (shown above in "Unions, Literal Types, and Narrowing").

### Type Guards at Boundaries

Convert `unknown` to a trusted type in one place, then use the trusted type everywhere inside.

### `as const` for Literal Configuration

```typescript
const ROLES = ['admin', 'editor', 'viewer'] as const;
type Role = (typeof ROLES)[number];
```

`as const` makes the array `readonly` and each element a literal type.

### `satisfies` to Validate Without Widening

```typescript
const routes = {
  home: '/',
  users: '/users',
} satisfies Record<string, string>;

routes.users;   // type is the literal '/users', not just string
```

### Abstract Base Classes for Shared Behavior

Use when subclasses share implementation and must implement a few members (see `BaseRepository` above). Prefer composition (injecting a collaborator) over deep inheritance.

### Type-Only Imports

```typescript
import type { User } from './user.entity';
```

Use for types that must never be treated as runtime dependencies.

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Using an `interface` as a DTO | Validation never runs. `class-validator` ignores it | The interface is erased, so no runtime metadata exists | Use a `class` with validation decorators |
| Using `any` to "fix" a compile error | Bug appears later at runtime | `any` disables type checking | Use `unknown` and narrow, or fix the type |
| `JSON.parse(x) as T` | `TypeError` far from the cause | `as` performs no validation | Validate at runtime (schema or class-validator) |
| `import type` on an injected class | Nest: "can't resolve dependencies" | Type-only imports are erased, so no `design:paramtypes` entry points to the class | Use a normal `import` for classes used in injected constructor parameters |
| Forgetting `!` or `?` on DTO fields under `strict` | `Property has no initializer` | `strictPropertyInitialization` | Mark `!` (framework assigns) or `?` (optional) |
| Using `\|\|` for defaults | `0`, `''`, `false` replaced by default | `\|\|` tests truthiness | Use `??` |
| Expecting `private` to hide data at runtime | Property visible in `console.log` / JSON | `private` is erased | Use `#field` or don't serialize it |
| Numeric `enum` accepting arbitrary numbers | Invalid values pass type check | Numeric enum assignability rules | Prefer string enums or literal unions |
| Treating a build with SWC/esbuild as type-checked | Type errors reach production | Those tools strip types only | Run `tsc --noEmit` in CI |
| Mutating a `readonly` array via casts | Surprising shared-state bugs | `readonly` is compile-time only | Copy before mutation |
| Circular imports between files | `undefined` at runtime at module load | Evaluation order of CommonJS/ESM | Extract shared code, or use lazy references |

---

## Debugging

### Reading Compiler Errors

```text
src/users/users.service.ts:12:5 - error TS2322:
  Type 'string | undefined' is not assignable to type 'string'.
```

Read bottom-up: the last line is the specific mismatch, earlier lines give context.

### Frequently Encountered Errors

| Error | Meaning | Typical Fix |
|---|---|---|
| `TS2322` | Type not assignable | Narrow, widen the target type, or fix the value |
| `TS2339` | Property does not exist on type | Check spelling, narrow the union, add the property to the type |
| `TS2345` | Argument not assignable to parameter | Fix the argument or generic constraint |
| `TS2531` / `TS18047` | Object is possibly `null` | Add a check, use `?.`, or `!` if truly certain |
| `TS2564` | Property has no initializer | Initialize, `?`, or `!` |
| `TS7006` | Parameter implicitly has `any` | Add a type annotation |
| `TS1238` / `TS1241` | Decorator signature mismatch | Check `experimentalDecorators` and decorator target |
| `TS2307` | Cannot find module | Check path, `moduleResolution`, installed `@types` |

### Commands

```bash
npx tsc --noEmit                    # type-check without output
npx tsc --showConfig                # confirm which options are really applied
npx tsc --traceResolution | grep users.service   # why a module resolved the way it did
npx tsc --extendedDiagnostics       # where compile time goes
```

### Techniques

- **Hover in the editor** to see the inferred type. Most "why is this a union?" questions answer themselves.
- **Isolate**: copy the failing expression into a scratch file and add types incrementally.
- **Check the effective config** with `--showConfig`. Editors, `tsc`, and test runners may each use a different `tsconfig`.
- **Restart the TS server** in your editor after changing `tsconfig.json` or installing types.

---

## Performance

TypeScript's performance concerns are compile-time, not runtime. Types add zero runtime cost.

| Concern | Technique |
|---|---|
| Slow cold builds | `skipLibCheck`, narrower `include`, project references |
| Slow rebuilds | `incremental: true`, `--watch`, `tsBuildInfoFile` |
| Slow editor | Avoid extremely large union types and deeply recursive conditional types |
| Slow tests | Use a transpiler such as SWC for tests, run `tsc --noEmit` separately |
| Runtime speed | Choose a `target` that matches your Node.js version to avoid unnecessary downleveling |

Diagnose with `tsc --extendedDiagnostics` and `tsc --generateTrace <dir>` (inspect the trace in a Chromium-based trace viewer). *Verify flag availability for your TypeScript version.*

---

## Security

- **Types are not validation.** Compile-time types give no protection against malformed or malicious input. Validate request data at runtime.
- **`as` and `!` hide trust assumptions.** Review every use on external data.
- **`any` from `JSON.parse`** is a common entry point for untyped data into typed code.
- **`private` is not a secret.** Never store secrets in a "private" field and then serialize the object.
- **Dependencies:** `@types/*` packages are community-maintained and can be out of sync with the real library. Types can lie about runtime behavior.

---

## Production Considerations

- Compile in CI with `tsc --noEmit` (or `nest build`) so type errors fail the pipeline.
- Ship compiled JavaScript from `dist/`. Do not run `ts-node` in production (slow start, higher memory).
- Enable `sourceMap` and run Node.js with `--enable-source-maps` so stack traces point to `.ts` lines.
- Align `target` with the Node.js version in your Docker image.
- Pin TypeScript to a specific minor version in `package.json`. Minor releases can add new errors on existing code.
- Treat upgrading TypeScript as a normal dependency upgrade: run the type check and tests, fix new errors deliberately.

---

## Best Practices

### Recommended

```typescript
function process(input: unknown): string {
  if (typeof input !== 'string') throw new TypeError('expected string');
  return input.trim();
}

export class CreateUserDto {
  email!: string;
}

const timeout = options.timeout ?? 5_000;
```

### Avoid

```typescript
function process(input: any): string {      // disables all checking
  return input.trim();
}

export interface CreateUserDto {            // erased: cannot be validated by decorators
  email: string;
}

const timeout = options.timeout || 5000;    // replaces 0 with 5000
```

Why: the recommended versions keep the compiler useful, preserve runtime information where the framework needs it, and avoid truthiness bugs.

Additional recommendations:

- Turn on `strict`.
- Prefer `readonly` for fields that should not change after construction.
- Prefer string literal unions or string enums over numeric enums.
- Prefer `unknown` over `any` for untrusted data.
- Keep types close to where they are used. Export only what other modules need.
- Name generic parameters meaningfully when there is more than one (`TEntity`, `TKey`), single letters (`T`) when there is one obvious role.

---

## Version / Compatibility Notes

| Feature | Introduced | Note |
|---|---|---|
| `unknown` type | TypeScript 3.0 | |
| Optional chaining `?.` and nullish coalescing `??` | TypeScript 3.7 | Emitted natively for modern targets |
| `useUnknownInCatchVariables` (part of `strict`) | TypeScript 4.4 | `catch (e)` is `unknown` |
| `satisfies` operator | TypeScript 4.9 | |
| Standard (TC39) decorators | TypeScript 5.0 | **Different** from the legacy `experimentalDecorators` model. NestJS uses the legacy model (see [TypeScript Decorators](./02-typescript-decorators.md)) |
| `const` type parameters | TypeScript 5.0 | |
| `using` declarations | TypeScript 5.2 | Requires runtime/`lib` support |

- The table lists first-introduction versions as commonly documented. *Verify exact versions in the TypeScript release notes before relying on them for a specific toolchain.*
- Supported TypeScript versions depend on your NestJS and Nest CLI versions. *Check the peer dependency ranges of your installed `@nestjs/*` packages.*
- TypeScript's major tooling is evolving (including work on a native compiler implementation). *Verify current status and compatibility with NestJS tooling before adopting new compiler versions.*

---

## Real-World Use Cases

- **NestJS controllers and services:** typed constructor injection and DTOs.
- **Shared contract types** between a backend and a TypeScript frontend (monorepos).
- **ORM entity definitions:** TypeORM, Prisma, and Mongoose models rely on generated or declared types.
- **Typed configuration:** a `Config` interface plus validated environment variables.
- **SDKs:** generated typed API clients from OpenAPI specs.
- **Refactoring at scale:** renaming a field and letting the compiler list every call site that breaks.

---

## Interview Questions

### Beginner

1. What is TypeScript, and how does it relate to JavaScript?
   - A typed superset of JavaScript that compiles to JavaScript. Types are checked at compile time and erased in output.
2. What is the difference between `interface` and `type`?
   - Both describe shapes. Interfaces support declaration merging and `extends`. Type aliases can also name unions, tuples, primitives, and mapped/conditional types.
3. What is the difference between `any` and `unknown`?
   - `any` turns off checking. `unknown` accepts any value but must be narrowed before use.
4. What does the `?` mean in `nickname?: string`?
   - The property is optional (its type is `string | undefined`).

### Intermediate

1. What does `constructor(private readonly repo: Repo) {}` do?
   - It declares a private readonly field `repo` and assigns the constructor argument to it.
2. Why can a class be used for validation or injection, but an interface cannot?
   - A class exists at runtime. An interface is erased.
3. What does `strict: true` enable, and why use it?
   - A family of checks, including `noImplicitAny` and `strictNullChecks`. It catches whole categories of bugs at compile time.
4. What is a discriminated union and how does narrowing work on it?
5. What is the difference between `Partial<T>` (TypeScript) and `PartialType()` (NestJS mapped types)?
   - `Partial<T>` is compile-time only. `PartialType()` returns a class that also keeps validation and Swagger metadata.

### Advanced

1. Why does a build with SWC or esbuild not catch type errors, and how do you compensate?
   - They only strip types. Run `tsc --noEmit` in CI.
2. What are the consequences of type erasure for generics?
   - Generic type arguments cannot be read at runtime, so frameworks cannot distinguish `Repo<A>` from `Repo<B>` without an explicit token.
3. How do legacy decorators (`experimentalDecorators`) differ from standard decorators in TypeScript 5+?
4. What does `import type` do, and how can it break dependency injection?
5. Explain structural typing and a case where it surprises people.
   - Extra properties are allowed when assigning a variable (as opposed to an object literal, where excess property checks apply).

---

## Quick Reference

```text
interface / type     → compile-time shapes, erased at runtime
class                → type AND runtime value (injectable, validatable)
unknown vs any       → unknown must be narrowed; any disables checking
?? vs ||             → ?? only for null/undefined; || for all falsy values
private              → compile-time only; #field for real privacy
as / !               → unchecked assertions, no runtime effect
strict: true         → enable noImplicitAny, strictNullChecks, etc.
experimentalDecorators + emitDecoratorMetadata → required for NestJS
tsc --noEmit         → type-check only (use in CI)
Partial/Pick/Omit    → compile-time utility types
```

---

## Key Takeaways

- TypeScript is a compile-time layer. Types never exist at runtime.
- Classes are runtime values. Interfaces are not. This single fact explains most NestJS DTO and DI behavior.
- Parameter properties (`constructor(private readonly x: X) {}`) are the syntax of NestJS dependency injection.
- Use `unknown` and runtime validation for untrusted input. Never trust `as`.
- Enable `strict` and run `tsc --noEmit` in CI, even when a faster transpiler builds your code.
- Generics are the basis of reusable repositories, pagination wrappers, and base services.
- `tsconfig.json` options such as `experimentalDecorators` and `emitDecoratorMetadata` change framework behavior directly.

---

## Related Topics

```text
JavaScript basics
      ↓
[01 TypeScript Essentials]
      ↓
02 TypeScript Decorators
      ↓
02-fundamentals (Modules, Controllers, Providers)
```

- [Prerequisites Overview](./README.md)
- [TypeScript Decorators](./02-typescript-decorators.md)
- [Node.js Async and Event Loop](./03-nodejs-async-and-event-loop.md)
- [Providers and Services](../02-fundamentals/04-providers-and-services.md)
- [DTOs](../03-core-concepts/02-validation-and-serialization/01-dto.md)
- [Metadata and Reflection (internals)](../06-internals/03-metadata-and-reflection.md)
