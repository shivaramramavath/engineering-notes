# Module 00: Environment Setup, Toolchains & Tooling (00-setup)

> Comprehensive architectural foundation covering the modern TypeScript toolchain, project initialization, compiler options, editor LSP integration, release lifecycles, runtimes (Node.js, Bun, Deno), and reproduction strategies.

---

## Module Overview

This module establishes the foundational infrastructure required for professional TypeScript engineering. It clarifies the critical distinction between type-checking (`tsc --noEmit`) and transpilation/type-stripping, configures modern project layouts using ECMAScript modules, and prepares engineers to diagnose compiler issues effectively.

---

## Notes Directory

| Code | Note Title | Focus & Core Concepts | Target Level | File Link |
| :--- | :--- | :--- | :--- | :--- |
| `00` | Toolchain & Runtime Ecosystem | `tsc` vs transpilers, Node.js native type stripping, Bun/Deno, AST flow | Beginner / Intermediate | [00-toolchain.md](file:///e:/Learning/engineering-notes/01-languages/typescript/00-setup/00-toolchain.md) |
| `01` | Creating & Structuring Projects | `package.json`, `tsconfig.json`, `NodeNext` resolution, `.d.ts` emit | Beginner / Intermediate | [01-first-project.md](file:///e:/Learning/engineering-notes/01-languages/typescript/00-setup/01-first-project.md) |
| `02` | Editor Integration & Playground | Language Server Protocol (LSP), `tsserver`, VS Code inlay hints, AST Explorer | Beginner / Intermediate | [02-editor-and-playground.md](file:///e:/Learning/engineering-notes/01-languages/typescript/00-setup/02-editor-and-playground.md) |
| `03` | Versions & Upgrade Strategies | TS 5.x releases, SemVer reality, `NoInfer`, TS 7.0 Native Go compiler | Intermediate / Advanced | [03-typescript-versions-and-upgrade-strategy.md](file:///e:/Learning/engineering-notes/01-languages/typescript/00-setup/03-typescript-versions-and-upgrade-strategy.md) |
| `04` | Running TypeScript | Direct execution, `tsx`, `node --experimental-strip-types`, runtime tradeoffs | Beginner / Intermediate | [04-running-typescript.md](file:///e:/Learning/engineering-notes/01-languages/typescript/00-setup/04-running-typescript.md) |
| `05` | Playground & Reproduction Repos | Minimal Reproducible Examples (MREs), URL state encoding, bisecting | Intermediate | [05-playground-and-reproduction-repos.md](file:///e:/Learning/engineering-notes/01-languages/typescript/00-setup/05-playground-and-reproduction-repos.md) |

---

## Learning Outcomes

After completing this module, you will be able to:
1. Distinguish between type checking and code transpilation in modern build systems.
2. Configure a strict, production-ready `tsconfig.json` utilizing `NodeNext` module resolution and erasable syntax.
3. Execute TypeScript natively in Node.js 22 LTS without third-party bundlers or build steps.
4. Set up editor inlay hints and language server parameters for maximum productivity.
5. Plan and execute zero-downtime TypeScript compiler version upgrades across enterprise codebases.
6. Create isolated, single-file Minimal Reproducible Examples to report bugs and share type proofs.
