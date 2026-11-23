# 09 Declaration Files

> Authoring, augmenting, publishing, and managing TypeScript declaration files (`.d.ts`): ambient contexts, global and module augmentation, declaration merging, `@types` ecosystem, `typesVersions`, and DefinitelyTyped workflows.

## Module Overview

Declaration files form the bridge between untyped JavaScript runtime ecosystems and static compile-time type safety. This module provides a comprehensive reference for authoring, packaging, and maintaining high-quality type declarations.

---

## Documents in this Module

| File | Level | Focus | Key Topics |
| :--- | :--- | :--- | :--- |
| [00-declaration-files.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/00-declaration-files.md) | Intermediate | Fundamentals | `.d.ts` syntax, `declaration: true`, `declarationMap`, `emitDeclarationOnly` |
| [01-ambient-declarations.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/01-ambient-declarations.md) | Intermediate | Ambient Scope | `declare` keyword, globals, window properties, `NodeJS.ProcessEnv` |
| [02-global-and-module-augmentation.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/02-global-and-module-augmentation.md) | Advanced | Non-Invasive Extension | `declare module`, `declare global`, Express/Fastify/Vitest extensions |
| [03-declaration-merging.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/03-declaration-merging.md) | Advanced | Merging Semantics | Interface merging, function + namespace merging, enum companions |
| [04-third-party-types.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/04-third-party-types.md) | Intermediate | Package Consumption | `@types/` packages, `skipLibCheck`, `typeRoots`, scoping with `types` |
| [05-authoring-declarations-for-javascript-libraries.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/05-authoring-declarations-for-javascript-libraries.md) | Advanced | Reverse Engineering | CJS/ESM/UMD authoring, `export =`, `export as namespace`, type tests |
| [06-types-versions-and-downleveling.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/06-types-versions-and-downleveling.md) | Advanced | Multi-Version Support | `typesVersions` in `package.json`, `downlevel-dts`, backwards compatibility |
| [07-definitely-typed-workflow-and-best-practices.md](file:///E:/Learning/engineering-notes/01-languages/typescript/09-declaration-files/07-definitely-typed-workflow-and-best-practices.md) | Advanced | Contribution & CI | DefinitelyTyped monorepo, `dtslint`, PR approval bot, automated publishing |

---

## Prerequisites
Before tackling this module, ensure you are familiar with:
- [00-setup/](file:///E:/Learning/engineering-notes/01-languages/typescript/00-setup/) (toolchain, tsconfig basics)
- [08-modules/](file:///E:/Learning/engineering-notes/01-languages/typescript/08-modules/) (imports, exports, resolution modes)

---

## Suggested Study Path
1. Learn declaration file fundamentals in **00-declaration-files**.
2. Master ambient runtime identifiers in **01-ambient-declarations**.
3. Augment third-party frameworks non-invasively in **02-global-and-module-augmentation**.
4. Understand how symbols merge across spaces in **03-declaration-merging**.
5. Manage `@types` dependencies in **04-third-party-types**.
6. Author declaration files for untyped libraries in **05-authoring-declarations-for-javascript-libraries**.
7. Support multiple compiler versions with **06-types-versions-and-downleveling**.
8. Contribute to the open-source community in **07-definitely-typed-workflow-and-best-practices**.
