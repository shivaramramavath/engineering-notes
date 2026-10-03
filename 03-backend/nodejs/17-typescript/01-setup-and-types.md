# Setup and Types

TypeScript is a superset of JavaScript: every valid `.js` file is valid TypeScript, and you add type annotations gradually. Browsers and Node can't run `.ts` directly (Node has experimental type-stripping, covered below), so a **compiler** (`tsc`) turns `.ts` into `.js`.

## Installing

```bash
mkdir my-api && cd my-api
npm init -y
npm install --save-dev typescript @types/node tsx
npx tsc --init
```

- **`typescript`** — the compiler (`tsc`)
- **`@types/node`** — type definitions for Node's built-in modules (`fs`, `path`, `process`, ...). Without it, `import fs from "fs"` has no types.
- **`tsx`** — runs `.ts` files directly in development, no separate compile step
- **`tsc --init`** — generates a starter `tsconfig.json`

Why `--save-dev`? The compiler is only needed at build time; production runs the compiled JavaScript (`05-production-config.md`).

---

## A minimal `tsconfig.json`

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "outDir": "dist",
    "rootDir": "src",
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true
  },
  "include": ["src"]
}
```

| Option | What it does |
|--------|--------------|
| `target` | Which JavaScript version to emit. `ES2022` is safe on modern Node. |
| `module` / `moduleResolution` | `NodeNext` mirrors how Node itself resolves ESM/CJS |
| `outDir` / `rootDir` | Compiled output goes to `dist/`, source lives in `src/` |
| `strict` | Turns on the whole family of strict checks — **always enable this** |
| `esModuleInterop` | Smoother imports of CommonJS packages |
| `skipLibCheck` | Skips type-checking `.d.ts` files in `node_modules` (faster builds) |

> With `"module": "NodeNext"` and ESM (`"type": "module"` in `package.json`), relative imports need the **`.js` extension** even in `.ts` files: `import { x } from "./utils.js"`. This surprises almost everyone once — TypeScript resolves it to `utils.ts` while writing, and the emitted file really is `utils.js`.

---

## Running TypeScript

```json
// package.json
{
  "type": "module",
  "scripts": {
    "dev": "tsx watch src/index.ts",
    "build": "tsc",
    "start": "node dist/index.js",
    "typecheck": "tsc --noEmit"
  }
}
```

- **`npm run dev`** — runs and restarts on change; **does not type-check** (tsx just strips types)
- **`npm run typecheck`** — checks types without emitting files; run it in CI
- **`npm run build`** — emits JavaScript into `dist/`
- **`npm start`** — runs the compiled output

Recent Node versions can also strip types natively (`node file.ts`), but it supports only a subset of TypeScript syntax and does no type-checking, so `tsx` remains the more forgiving dev choice.

---

## Basic types

```ts
const name: string = "Asha";
const age: number = 30;
const active: boolean = true;

const tags: string[] = ["node", "ts"];
const point: [number, number] = [10, 20];   // tuple: fixed length and positions
```

Most of the time you don't need to write these — **type inference** works them out:

```ts
const count = 5;          // inferred as number
const names = ["a", "b"]; // inferred as string[]

count = "five"; // ❌ Error: Type 'string' is not assignable to type 'number'
```

Rule of thumb: **annotate function parameters and return types; let inference handle local variables.**

```ts
function add(a: number, b: number): number {
  return a + b;
}
```

---

## Union and literal types

A value that can be one of several types:

```ts
let id: string | number;
id = "abc";
id = 123;

type Role = "admin" | "user" | "guest"; // literal union — only these exact strings
let role: Role = "admin";
role = "owner"; // ❌ Error
```

Literal unions are the idiomatic replacement for enums in most backend code — they cost nothing at runtime and read naturally.

---

## Optional properties, `null`, and `undefined`

```ts
function greet(name?: string) {   // name is string | undefined
  return `Hello, ${name ?? "stranger"}`;
}
```

With `strict` on, `null` and `undefined` are **not** assignable to other types. TypeScript forces you to handle them:

```ts
const user = await User.findById(id); // User | null
user.name;                            // ❌ Error: 'user' is possibly 'null'

if (!user) throw new Error("Not found");
user.name;                            // ✅ narrowed to User
```

This is the single biggest bug-catcher in practice: it eliminates most `Cannot read properties of undefined` errors.

---

## `any`, `unknown`, and `never`

| Type | Meaning | Use |
|------|---------|-----|
| `any` | Opt out of type checking entirely | Avoid — it silently spreads |
| `unknown` | "Could be anything — check before use" | Safe replacement for `any` (e.g. parsed JSON, `catch` errors) |
| `never` | "This can't happen" | Exhaustiveness checks, functions that always throw |

```ts
const data: unknown = JSON.parse(input);

data.name;                       // ❌ Error — must narrow first
if (typeof data === "object" && data !== null && "name" in data) {
  // now safe to look at data.name
}
```

---

## Type narrowing

TypeScript tracks what you've checked and narrows the type inside each branch:

```ts
function format(value: string | number) {
  if (typeof value === "string") {
    return value.toUpperCase(); // string here
  }
  return value.toFixed(2);      // number here
}
```

Common narrowing tools: `typeof`, `instanceof`, `in`, truthiness checks, and equality checks.

---

## Type assertions — use sparingly

```ts
const el = value as string;
```

An assertion tells the compiler "trust me." If you're wrong, nothing stops the bug at runtime. Prefer narrowing or validation; reach for `as` only when you genuinely know more than the compiler (and ideally leave a comment saying why).

---

## Common mistakes

- **Using `any` to silence an error** — you've turned the checker off for that value *and everything derived from it*. Use `unknown` and narrow.
- **Turning `strict` off to "make errors go away"** — you lose the main benefit of TypeScript. Fix the code instead.
- **Assuming `npm run dev` type-checks** — `tsx` and `ts-node --transpile-only` skip checking. Run `tsc --noEmit` in CI.
- **Forgetting the `.js` extension in ESM relative imports** — compiles fine, then fails at runtime with `ERR_MODULE_NOT_FOUND`.
- **Forgetting `@types/node`** — Node built-ins show up as untyped or missing.

## Quick summary

- Install `typescript`, `@types/node`, and `tsx`; generate `tsconfig.json` with `tsc --init`
- **Always** enable `"strict": true`
- Annotate function signatures; let inference handle the rest
- Prefer `unknown` over `any`; use narrowing instead of `as`
- Dev runs with `tsx`, production runs compiled JS from `dist/`

## Next

**`02-interfaces-and-generics.md`** covers how to describe the shape of your data — users, posts, API responses — and how generics keep reusable code type-safe.
