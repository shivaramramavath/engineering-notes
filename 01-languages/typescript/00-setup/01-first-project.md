# First Project

## Definition
A minimal TypeScript project: a `package.json`, a `tsconfig.json`, a `src/` folder, and scripts to type-check, build and run.

## Why It Matters
Every later chapter assumes you can create a file, see type errors, and run the result. This sets that loop up once.

## Prerequisites
[00-toolchain.md](00-toolchain.md)

## Steps

### 1. Create the project
```bash
mkdir ts-playground && cd ts-playground
npm init -y
npm install --save-dev typescript tsx @types/node
```

### 2. Create `tsconfig.json`
You can generate a commented one with `npx tsc --init`, but this minimal version is easier to read:

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "outDir": "dist",
    "rootDir": "src"
  },
  "include": ["src"]
}
```

What matters at this stage:

| Option | Meaning |
|---|---|
| `target` | Which JavaScript version the output uses |
| `module` / `moduleResolution` | How imports are emitted and resolved (use `NodeNext` for Node projects) |
| `strict` | Turns on the full set of strict checks. Always enable it |
| `outDir` / `rootDir` | Where compiled files go / where sources live |
| `include` | Which files belong to the project |

Everything else is explained in [13-compiler-and-tsconfig](../13-compiler-and-tsconfig/README.md).

### 3. Write code
`src/index.ts`:
```ts
function greet(name: string): string {
  return `Hello, ${name}!`;
}

console.log(greet("TypeScript"));
console.log(greet(42)); // error
```

### 4. See the error
```bash
npx tsc --noEmit
```
```
src/index.ts(6,19): error TS2345: Argument of type 'number' is not assignable to parameter of type 'string'.
```

Fix the last line, then:

```bash
npx tsx src/index.ts      # run directly
npx tsc && node dist/index.js   # compile, then run
```

### 5. Add scripts to `package.json`
```json
{
  "type": "module",
  "scripts": {
    "dev": "tsx watch src/index.ts",
    "build": "tsc",
    "typecheck": "tsc --noEmit",
    "start": "node dist/index.js"
  }
}
```
With `"module": "NodeNext"`, setting `"type": "module"` makes `.ts` files ESM. In ESM, relative imports need a `.js` extension, even in `.ts` source files (`import { x } from "./utils.js"`). See [08-modules](../08-modules/README.md).

## Project layout
```text
ts-playground/
├── package.json
├── tsconfig.json
├── src/
│   └── index.ts
└── dist/          # generated, git-ignore it
```

Add a `.gitignore`:
```text
node_modules
dist
```

## Common Mistakes
- Putting `tsconfig.json` in the wrong folder; `tsc` looks upward from the current directory.
- Forgetting `include`, so `tsc` compiles files you did not intend.
- Leaving `strict` off to "make errors go away". This hides the problems TypeScript exists to catch.
- Missing `.js` extensions in relative imports under `NodeNext`.

## Best Practices
- Keep `strict: true` from day one.
- Separate `typecheck` from `build` in scripts.
- Commit `tsconfig.json`; never commit `dist/` or `node_modules/`.

## Related Topics
- [00-toolchain.md](00-toolchain.md)
- [13 compiler options](../13-compiler-and-tsconfig/00-compiler-options.md)
- [13 strict mode](../13-compiler-and-tsconfig/01-strict-mode.md)
- [13 tsconfig recipes](../13-compiler-and-tsconfig/05-tsconfig-recipes.md)
