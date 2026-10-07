# tsconfig Recipes

Ready-to-adapt `tsconfig.json` files for the situations you meet most often. Each one is a **starting point**, not a rule: frameworks and tools change their recommended settings between versions, so compare against your tool's current documentation or generated template before copying.

**Prerequisites:**
- [Compiler options](./00-compiler-options.md)
- [Strict mode](./01-strict-mode.md)
- [Target, module, and lib](./02-target-module-and-lib.md)
- [Module resolution and paths](./03-module-resolution-and-paths.md)

---

## How to use this note

All examples are JSONC (comments allowed). For each recipe, three decisions matter most:

1. **Who produces the JavaScript?** `tsc` (needs `outDir`, `module`, `target`) or a bundler/runtime (usually `noEmit: true`).
2. **Who runs it?** Node (`nodenext`) or a bundler (`bundler`).
3. **How strict?** Start from `strict: true`, add `noUncheckedIndexedAccess`.

Community bases (`@tsconfig/node20`, `@tsconfig/strictest`, and others in the `@tsconfig` scope) encode vetted combinations and can replace many lines with an `extends`.

## 1. Node.js app or service (ESM, built with `tsc`)

```jsonc
// package.json must have: "type": "module"
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2022"],
    "module": "NodeNext",
    "moduleResolution": "NodeNext",

    "strict": true,
    "noUncheckedIndexedAccess": true,
    "noImplicitOverride": true,
    "noFallthroughCasesInSwitch": true,
    "verbatimModuleSyntax": true,

    "outDir": "dist",
    "rootDir": "src",
    "sourceMap": true,
    "skipLibCheck": true
  },
  "include": ["src"]
}
```

Notes: relative imports need `.js` extensions. Install `@types/node`. Run with `node --enable-source-maps dist/index.js`. Pick `target` and `lib` to match your Node version, or extend `@tsconfig/node*`.

## 2. Node.js with CommonJS output

```jsonc
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2022"],
    "module": "CommonJS",
    "moduleResolution": "Node10",   // or "Bundler" if you prefer exports-map support

    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,

    "outDir": "dist",
    "rootDir": "src",
    "sourceMap": true
  },
  "include": ["src"]
}
```

Do **not** enable `verbatimModuleSyntax` with CommonJS output: it rejects ESM `import`/`export` syntax in a file the compiler will emit as CommonJS. Use `isolatedModules` and `import type` instead.

## 3. Vite (or other bundled) front end

```jsonc
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2022", "DOM", "DOM.Iterable"],
    "module": "ESNext",
    "moduleResolution": "bundler",
    "jsx": "react-jsx",            // for React; omit for non-JSX projects

    "noEmit": true,                // the bundler produces the output
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "noFallthroughCasesInSwitch": true,
    "verbatimModuleSyntax": true,
    "isolatedModules": true,
    "resolveJsonModule": true,
    "allowImportingTsExtensions": true,   // allowed because noEmit is on
    "skipLibCheck": true,

    "paths": { "@/*": ["./src/*"] }       // also add the alias to vite.config
  },
  "include": ["src"]
}
```

Add `/// <reference types="vite/client" />` in a `.d.ts` for asset and `import.meta.env` types ([ambient declarations](../09-declaration-files/01-ambient-declarations.md)). Vite's own templates split config into app and node tsconfigs, which is a project-references-style setup.

## 4. Next.js

Next generates and manages its own tsconfig. Typically it looks like this (verify against your version's generated file):

```jsonc
{
  "compilerOptions": {
    "target": "ES2017",
    "lib": ["dom", "dom.iterable", "esnext"],
    "allowJs": true,
    "skipLibCheck": true,
    "strict": true,
    "noEmit": true,
    "esModuleInterop": true,
    "module": "esnext",
    "moduleResolution": "bundler",
    "resolveJsonModule": true,
    "isolatedModules": true,
    "jsx": "preserve",
    "incremental": true,
    "plugins": [{ "name": "next" }],
    "paths": { "@/*": ["./*"] }
  },
  "include": ["next-env.d.ts", "**/*.ts", "**/*.tsx", ".next/types/**/*.ts"],
  "exclude": ["node_modules"]
}
```

Let Next adjust the file. Tighten it by adding `noUncheckedIndexedAccess`. See [Next.js](../19-react-and-frontend/09-nextjs.md).

## 5. NestJS

Nest's scaffold targets CommonJS with decorators. A typical shape:

```jsonc
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "CommonJS",
    "moduleResolution": "Node10",

    "experimentalDecorators": true,
    "emitDecoratorMetadata": true,
    "declaration": true,
    "removeComments": true,
    "allowSyntheticDefaultImports": true,
    "esModuleInterop": true,
    "sourceMap": true,
    "outDir": "./dist",
    "incremental": true,
    "skipLibCheck": true,

    "strict": true
  },
  "include": ["src"]
}
```

Notes: the generated Nest project historically ships with some strict flags **off** (for example `strictNullChecks` and `noImplicitAny`). Turning `strict` on is worthwhile but may require adjusting DTOs and entity classes (for example `!` on injected or decorated properties, or `strictPropertyInitialization: false` if you prefer). `emitDecoratorMetadata` is needed for dependency injection to see constructor parameter types. See [NestJS](../20-nodejs-backend/08-nestjs.md) and [decorators](../05-classes/07-decorators.md).

## 6. Publishing a library (ESM, with declarations)

```jsonc
{
  "compilerOptions": {
    "target": "ES2020",
    "lib": ["ES2020"],
    "module": "NodeNext",
    "moduleResolution": "NodeNext",

    "strict": true,
    "verbatimModuleSyntax": true,
    "skipLibCheck": true,

    "declaration": true,
    "declarationMap": true,
    "sourceMap": true,
    "outDir": "dist",
    "rootDir": "src"
  },
  "include": ["src"],
  "exclude": ["src/**/*.test.ts"]
}
```

Pick a conservative `target` so consumers on older runtimes are not broken. Describe the package with `exports` and `types` in `package.json`, and verify with `@arethetypeswrong/cli` ([publishing packages](../21-production-tooling/03-publishing-packages.md)). Shipping both ESM and CommonJS usually means two builds or a bundler such as tsup, with matching `.d.ts` and `.d.cts` files.

## 7. Shared base plus project-specific configs

```jsonc
// tsconfig.base.json
{
  "compilerOptions": {
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "skipLibCheck": true,
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "verbatimModuleSyntax": true
  }
}
```

```jsonc
// packages/api/tsconfig.json
{
  "extends": "../../tsconfig.base.json",
  "compilerOptions": {
    "composite": true,
    "outDir": "dist",
    "rootDir": "src"
  },
  "include": ["src"],
  "references": [{ "path": "../core" }]
}
```

```jsonc
// tsconfig.json (root solution file)
{
  "files": [],
  "references": [{ "path": "packages/core" }, { "path": "packages/api" }]
}
```

With TS 5.5+, `${configDir}` in a shared base lets paths like `"outDir": "${configDir}/dist"` resolve against the config that **extends** it. Details in [project references](./04-project-references-and-incremental-builds.md) and [monorepos](../21-production-tooling/02-monorepos.md).

## 8. Separate config for tests

```jsonc
// tsconfig.test.json
{
  "extends": "./tsconfig.json",
  "compilerOptions": {
    "types": ["vitest/globals", "node"],
    "noEmit": true
  },
  "include": ["src", "test"]
}
```

This keeps test globals (`describe`, `expect`) out of production code. Point your editor and test runner at it where needed. See [test runners](../18-testing-and-debugging/03-test-runners.md).

## 9. Type-check-only for scripts and tooling

```jsonc
{
  "extends": "./tsconfig.base.json",
  "compilerOptions": { "noEmit": true, "allowJs": true, "checkJs": true },
  "include": ["scripts", "*.config.ts"]
}
```

Useful for build scripts and configs that run through `tsx`, `ts-node`, or a runtime with native TypeScript support.

## Choosing quickly

| You are building | `module` / `moduleResolution` | Output by | `noEmit` |
|---|---|---|---|
| Node service or CLI (ESM) | `NodeNext` / `NodeNext` | `tsc` | no |
| Node service (CommonJS) | `CommonJS` / `Node10` | `tsc` | no |
| Browser app with a bundler | `ESNext` / `Bundler` | bundler | yes |
| Library for npm | `NodeNext` / `NodeNext` | `tsc` or bundler | no |
| Monorepo | per package, with `composite` and references | `tsc -b` | no |

## Common mistakes

- **Copying a config without understanding `module`/`moduleResolution`,** then fighting import errors.
- **`bundler` resolution for Node code,** or `nodenext` for a bundled app that uses extensionless imports.
- **`verbatimModuleSyntax` with CommonJS output.**
- **One tsconfig for source, tests, and scripts,** so globals leak into production code.
- **Leaving `skipLibCheck` off in a project with conflicting dependency types,** or on in a library whose own `.d.ts` you want verified.
- **Using `paths` without matching bundler or runtime config** ([module resolution](./03-module-resolution-and-paths.md)).
- **Not pinning the TypeScript version,** so defaults and `strict` contents shift under you.

## Debugging

- `tsc --showConfig` prints the final merged config for any file you run it against.
- Compare against your framework's generated template (`create-vite`, `create-next-app`, `nest new`) when something differs from these recipes.
- If an option has no effect, confirm that the tool you are watching (editor, bundler, test runner) actually reads this tsconfig, and which one.
- Run `tsc --noEmit` in CI with the same config the editor uses so the two cannot drift.

## Quick summary

- Decide first who emits JavaScript and who runs it. That fixes `module`, `moduleResolution`, and `noEmit`.
- Always start from `strict: true`. Add `noUncheckedIndexedAccess` and `noImplicitOverride` where you can.
- Node: `NodeNext` with `.js` extensions. Bundled front end: `ESNext` + `bundler` + `noEmit`. CommonJS output: no `verbatimModuleSyntax`.
- Use a shared base with `extends`, separate configs for tests, and `composite` references for monorepos.
- Treat these as starting points and re-check against your tools' current docs.

**Next:** [14 Type System Internals](../14-type-system-internals/README.md)
