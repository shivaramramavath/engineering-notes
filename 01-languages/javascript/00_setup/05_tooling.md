# Tooling

Good tooling catches bugs before they run and keeps code consistent without effort.

```
Editor  ─►  Formatter (Prettier)  ─►  Linter (ESLint)  ─►  Tests  ─►  Bundler/Dev server (Vite)  ─►  Git
```

## Git basics

```bash
git init
git add .
git commit -m "feat: add setup chapter"
git switch -c feature/setup
git merge feature/setup
git log --oneline
```

| Concept                         | Meaning                       |
| ------------------------------- | ----------------------------- |
| Working tree / staging / commit | edit, select, save a snapshot |
| Branch                          | independent line of work      |
| Remote                          | shared copy (GitHub, GitLab)  |
| `.gitignore`                    | files Git should skip         |

Write small commits with clear messages (Conventional Commits: `feat:`, `fix:`, `docs:`, `refactor:`).

## ESLint: finds problems

```bash
npm install -D eslint @eslint/js
npx eslint .
```

```js
// eslint.config.js
import js from "@eslint/js";

export default [
  js.configs.recommended,
  {
    rules: {
      "no-unused-vars": "warn",
      eqeqeq: "error",
      "no-debugger": "error",
    },
  },
];
```

Catches unused variables, unreachable code, accidental globals and risky patterns.

## Prettier: formats code

```bash
npm install -D prettier
npx prettier --write .
```

```json
{ "semi": true, "singleQuote": false, "printWidth": 90 }
```

| Tool         | Job                                  |
| ------------ | ------------------------------------ |
| **Prettier** | style (spacing, quotes, line breaks) |
| **ESLint**   | correctness and best practices       |

Let Prettier own formatting and ESLint own logic rules.

## Vite: dev server and bundler

```bash
npm create vite@latest my-app
cd my-app
npm install
npm run dev
```

Gives instant startup, hot module replacement, and an optimized production build (`npm run build`).

## Bundlers in one table

| Tool              | Notes                                       |
| ----------------- | ------------------------------------------- |
| **Vite**          | dev server plus Rollup build, great default |
| **esbuild / SWC** | very fast compilers                         |
| **webpack**       | mature, highly configurable                 |
| **Rollup**        | best for libraries                          |

Bundling, tree-shaking and code splitting are covered in `13_modules/04_bundlers-and-tree-shaking.md`.

## TypeScript (optional next step)

Adds static types on top of JavaScript and compiles to plain JS. Even without it, you can get type hints in JS files with JSDoc and `// @ts-check`.

```js
// @ts-check
/** @param {number} a @param {number} b */
export const add = (a, b) => a + b;
```

## Editor setup (VS Code)

| Extension / setting           | Benefit                               |
| ----------------------------- | ------------------------------------- |
| ESLint                        | inline problems                       |
| Prettier                      | format on save                        |
| `"editor.formatOnSave": true` | automatic formatting                  |
| Built-in debugger             | breakpoints in Node                   |
| EditorConfig                  | consistent indentation across editors |

## Example project scripts

```json
{
  "scripts": {
    "dev": "vite",
    "build": "vite build",
    "lint": "eslint .",
    "format": "prettier --write .",
    "test": "vitest"
  }
}
```

## Automation

- **Husky + lint-staged**: run lint and format on staged files before each commit
- **CI** (GitHub Actions): run `npm ci`, `lint`, `test`, `build` on every push

## Tooling pitfalls

| Pitfall                      | Why it hurts              | Better                           |
| ---------------------------- | ------------------------- | -------------------------------- |
| Style debates in code review | Wasted time               | Prettier plus shared config      |
| ESLint and Prettier fighting | Conflicting rules         | `eslint-config-prettier`         |
| Tools only on your machine   | Inconsistent team results | Commit configs, run in CI        |
| Over-configuring on day one  | Slows learning            | Start with recommended presets   |
| Committing secrets (`.env`)  | Security leak             | `.gitignore` and secret managers |

## Key takeaways

- Git tracks history, ESLint finds bugs, Prettier fixes style
- Vite is a solid default for browser projects
- Put lint, format and test in `scripts` and run them in CI
- Start with recommended presets, then tighten rules over time

**Next:** [Fundamentals](../01_fundamentals/00_README.md)
