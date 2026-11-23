# TypeScript Ground Truth Facts & Baseline Specifications (TS_FACTS.md)

> Verified runtime environments, compiler milestones, standard library evolutions, and tooling versions for the TypeScript curriculum track.

---

## 1. Compiler Milestones & Release Versions

- **TypeScript 5.0 (March 2023)**:
  - Standard Stage 3 Decorators (replacing legacy `experimentalDecorators`).
  - `const` Type Parameters (`<const T>`).
  - Supporting multiple configuration files in `extends` (`"extends": ["...", "..."]`).
  - `--moduleResolution bundler` for modern frontend/fullstack bundlers (Vite, Next.js).
  - `--verbatimModuleSyntax` (supersedes `importsNotUsedAsValues` and `preserveValueImports`).
  - Source: [TypeScript 5.0 Announcement](https://devblogs.microsoft.com/typescript/announcing-typescript-5-0/)

- **TypeScript 5.1 (June 2023)**:
  - Easier implicit returns for `undefined`-returning functions.
  - Unrelated types for getters and setters.
  - Decoupled JSX element and JSX element type checking.
  - Source: [TypeScript 5.1 Announcement](https://devblogs.microsoft.com/typescript/announcing-typescript-5-1/)

- **TypeScript 5.2 (August 2023)**:
  - Explicit Resource Management (`using` and `await using` declarations).
  - Decorator metadata (`Symbol.metadata`).
  - Source: [TypeScript 5.2 Announcement](https://devblogs.microsoft.com/typescript/announcing-typescript-5-2/)

- **TypeScript 5.3 (November 2023)**:
  - Import Attributes syntax (`import data from "./data.json" with { type: "json" }`).
  - Stable `resolution-mode` in import attributes.
  - Narrowing on `Boolean(x)` and `instanceof` with `Symbol.hasInstance`.
  - Source: [TypeScript 5.3 Announcement](https://devblogs.microsoft.com/typescript/announcing-typescript-5-3/)

- **TypeScript 5.4 (March 2024)**:
  - `NoInfer<T>` intrinsic utility type preventing contextual inference.
  - Preserved narrowing in closures following last assignment.
  - `Object.groupBy` and `Map.groupBy` standard lib types.
  - Source: [TypeScript 5.4 Announcement](https://devblogs.microsoft.com/typescript/announcing-typescript-5-4/)

- **TypeScript 5.5 (June 2024)**:
  - Inferred type predicates (functions returning boolean now infer `x is Type` automatically without explicit return annotations).
  - Control flow narrowing for constant indexed accesses (`obj[key]`).
  - JSDoc `@import` comments.
  - `--isolatedDeclarations` for fast, parallel multi-threaded declaration emit.
  - Source: [TypeScript 5.5 Announcement](https://devblogs.microsoft.com/typescript/announcing-typescript-5-5/)

- **TypeScript 5.6 (September 2024)**:
  - Disallowed nullish checks on truthy/falsy expressions (syntax errors on invalid `if (/regex/)` or `typeof x === null`).
  - Iterator helpers in `lib.es2024.iterator.d.ts`.
  - Disallowed comparisons against un-awaited Promises (`if (promise === true)` error).
  - Source: [TypeScript 5.6 Announcement](https://devblogs.microsoft.com/typescript/announcing-typescript-5-6/)

- **TypeScript 5.7 (November 2024)**:
  - Checks for never-initialized variables (`strictNullChecks` flags variables never written to).
  - Path-rewriting support for relative `.ts` imports (`--rewriteRelativeImportExtensions`).
  - Source: [TypeScript 5.7 Announcement](https://devblogs.microsoft.com/typescript/announcing-typescript-5-7/)

- **TypeScript 5.8 (February 2025)**:
  - Granular return expression branch checks.
  - Support for `require()` of ECMAScript modules under `--module nodenext`.
  - `--erasableSyntaxOnly` flag forbidding non-standard erasable syntax (enums, namespaces, parameter properties).
  - `--libReplacement` option.
  - Source: [TypeScript 5.8 Announcement](https://devblogs.microsoft.com/typescript/announcing-typescript-5-8/)

- **TypeScript 5.9 (August 2025)**:
  - Minimal clean `tsc --init` output without endless commented-out lines.
  - Deferred imports (`import defer`).
  - Targeted `--module node20`.
  - Source: [TypeScript 5.9 Announcement](https://devblogs.microsoft.com/typescript/announcing-typescript-5-9/)

- **TypeScript 6.0 (Early 2026)**:
  - Final JavaScript-bootstrapped compiler release bridging the ecosystem towards native compiler GA.
  - Source: [TypeScript 6.0 Announcement](https://devblogs.microsoft.com/typescript/announcing-typescript-6-0/)

- **TypeScript 7.0 (July 2026 - Native Compiler / Project Corsa)**:
  - Native compiler rewritten in **Go** (formerly `tsgo` binary / `@typescript/native-preview`).
  - 8x to 12x compilation speedup, massive memory reduction.
  - True multi-threaded shared-memory parallelism (`--checkers`, `--builders`).
  - Source: [TypeScript 7.0 Announcement](https://devblogs.microsoft.com/typescript/announcing-typescript-7-0/)

---

## 2. Node.js Native TypeScript Support & Type Stripping

- **Node.js Baseline**: Node.js v22 LTS (v22.6.0+ introduced `--experimental-strip-types`, active by default in modern releases) & Node.js v24.
- **Type Stripping Mechanics**:
  - Replaces erasable TypeScript syntax (types, interfaces, type annotations) with blank whitespace to preserve exact line/column positions in source maps and stack traces.
  - Does **NOT** type-check code at runtime; type-checking must run separately via `tsc --noEmit`.
  - **Unsupported syntax** (causes runtime SyntaxError under type stripping):
    - `enum` declarations
    - `namespace` blocks containing runtime JavaScript
    - Parameter properties in class constructors (`constructor(public name: string)`)
    - Legacy `import =` / `export =`
  - Enforced by TypeScript via `--erasableSyntaxOnly` compiler flag (TS 5.8+).
  - Source: [Node.js Native Type Stripping Documentation](https://nodejs.org/docs/latest/api/typescript.html)

---

## 3. Tooling & Ecosystem Baselines

- **typescript-eslint**:
  - Version: `v8.x` Flat Config (`eslint.config.js`).
  - `projectService: true` replaces legacy `parserOptions.project` for type-aware linting.
  - Source: [typescript-eslint v8 Documentation](https://typescript-eslint.io/)
- **Vitest**:
  - Version: `v2.x` / `v3.x`.
  - Native type-checking via `vitest --typecheck` and `expectTypeOf()`.
  - Source: [Vitest Typecheck](https://vitest.dev/guide/testing-types.html)
- **Zod**:
  - Version: `v3.x` / `v4.x` with `z.infer<typeof Schema>`.
  - Compatible with standard TypeScript type narrowing and branded types.
- **TanStack Query**:
  - Version: `v5.x` with `@tanstack/react-query` and flat ESLint plugin `@tanstack/eslint-plugin-query`.
- **Runtime Validation Libraries**:
  - Zod, Valibot (modular/tree-shakeable), ArkType (fast 1:1 TS syntax runtime validator), TypeBox (JSON Schema).
