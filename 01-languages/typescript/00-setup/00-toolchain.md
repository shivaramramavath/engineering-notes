# Toolchain

## Definition
The set of tools needed to write, type-check, compile and run TypeScript: a JavaScript runtime (Node.js), a package manager, the TypeScript compiler (`tsc`), and optionally a runner that executes `.ts` files directly.

## Why It Matters
Browsers and Node run JavaScript, not TypeScript. TypeScript is a type checker plus a compiler that erases types. Knowing which tool does what (checking vs. compiling vs. running) avoids most beginner confusion.

## Prerequisites
Basic JavaScript.

## What each tool does

| Tool | Role |
|---|---|
| Node.js | Runs JavaScript on your machine |
| npm / pnpm / yarn | Installs packages, runs scripts |
| `tsc` | Type-checks and compiles `.ts` to `.js` |
| `tsx` / `ts-node` | Runs `.ts` files directly during development |

## Install

### 1. Node.js
Install the current LTS version from nodejs.org, or use a version manager (`nvm`, `fnm`, `volta`) so you can switch versions per project.

```bash
node --version
npm --version
```

### 2. TypeScript (per project, not global)
Installing per project pins the version for everyone on the team.

```bash
npm install --save-dev typescript
npx tsc --version
```

A global install (`npm install -g typescript`) works for quick experiments but is not recommended for real projects.

### 3. Also install Node's types
```bash
npm install --save-dev @types/node
```
This gives you types for `fs`, `path`, `process` and other Node APIs.

## Running TypeScript

### Option A: compile, then run (what production uses)
```bash
npx tsc
node dist/index.js
```

### Option B: `tsx` (fast, recommended for development)
```bash
npm install --save-dev tsx
npx tsx src/index.ts
npx tsx watch src/index.ts   # re-run on save
```
`tsx` strips types and runs the code. It does **not** type-check.

### Option C: `ts-node`
```bash
npm install --save-dev ts-node
npx ts-node src/index.ts
```
`ts-node` can type-check while running, but is slower and has more ESM friction than `tsx`.

### Option D: Node's built-in type stripping
Recent Node versions can run `.ts` files directly by stripping types. Support and flags depend on your Node version, so check the Node docs for the version you use. Like `tsx`, this does not type-check, and it only supports syntax that can be erased (no enums or namespaces without extra flags).

## Important Concepts
- **Running is not checking.** `tsx`, Node type stripping, esbuild and swc all remove types without verifying them. Always run `tsc --noEmit` (locally and in CI) to actually type-check.
- **TypeScript version matters.** Syntax and behaviour change between releases; pin it in `devDependencies`.

## Common Mistakes
- Installing TypeScript globally and getting different results than a teammate.
- Assuming `tsx file.ts` reports type errors.
- Forgetting `@types/node`, then seeing "Cannot find name 'process'" or "Cannot find module 'fs'".

## Best Practices
- Install `typescript` as a dev dependency.
- Add scripts so nobody has to remember commands:

```json
{
  "scripts": {
    "dev": "tsx watch src/index.ts",
    "build": "tsc",
    "typecheck": "tsc --noEmit",
    "start": "node dist/index.js"
  }
}
```

## Quick Reference

| Goal | Command |
|---|---|
| Type-check only | `npx tsc --noEmit` |
| Compile | `npx tsc` |
| Compile on change | `npx tsc --watch` |
| Run during dev | `npx tsx src/index.ts` |

## Related Topics
- [01-first-project.md](01-first-project.md)
- [13 build options](../13-compiler-and-tsconfig/00-compiler-options.md)
- [21 build and bundling](../21-production-tooling/01-build-and-bundling.md)
