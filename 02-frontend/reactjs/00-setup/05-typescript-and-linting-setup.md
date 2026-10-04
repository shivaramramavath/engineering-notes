# TypeScript and Linting Setup

Three tools catch most mistakes before you run the code: **TypeScript** (type errors), **ESLint** (suspicious code and rule violations), and **Prettier** (consistent formatting). The Vite template configures TypeScript and ESLint for you; this file explains what those settings mean and how to add Prettier and editor integration.

## Prerequisites

[`02-vite.md`](./02-vite.md) — a project created from the `react-ts` template.

---

## TypeScript configuration

The template splits config across three files (see [`03-project-structure.md`](./03-project-structure.md)). The important one is `tsconfig.app.json`:

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2022", "DOM", "DOM.Iterable"],
    "module": "ESNext",
    "moduleResolution": "bundler",
    "jsx": "react-jsx",
    "strict": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "noEmit": true,
    "skipLibCheck": true,
    "baseUrl": ".",
    "paths": {
      "@/*": ["src/*"]
    }
  },
  "include": ["src"]
}
```

| Option | Why it matters |
|--------|----------------|
| `strict: true` | Turns on the full set of strict checks (null safety, implicit `any`, and more). **Keep it on** — it's where most of TypeScript's value comes from |
| `jsx: "react-jsx"` | Uses the modern JSX transform, so you don't `import React` in every file |
| `moduleResolution: "bundler"` | Matches how Vite resolves imports |
| `noEmit: true` | TypeScript only checks; Vite does the compiling |
| `noUnusedLocals` / `noUnusedParameters` | Flags dead code |
| `paths` | Makes `@/components/Button` resolve — must match the alias in `vite.config.ts` |

### Type-checking is separate from running

Vite strips types and **does not type-check** during `npm run dev`. Type errors show in your editor, and the build script runs the compiler:

```bash
npx tsc -b      # type-check the whole project
```

Run it in CI so type errors can't be merged.

---

## ESLint

ESLint finds bugs and bad patterns. The template ships a flat config (`eslint.config.js`). A representative version:

```js
import js from "@eslint/js";
import globals from "globals";
import tseslint from "typescript-eslint";
import reactHooks from "eslint-plugin-react-hooks";

export default tseslint.config(
  { ignores: ["dist"] },
  {
    extends: [js.configs.recommended, ...tseslint.configs.recommended],
    files: ["**/*.{ts,tsx}"],
    languageOptions: {
      ecmaVersion: 2022,
      globals: globals.browser,
    },
    plugins: {
      "react-hooks": reactHooks,
    },
    rules: {
      "react-hooks/rules-of-hooks": "error",
      "react-hooks/exhaustive-deps": "warn",
    },
  }
);
```

The two React rules are the most valuable lines here:

- **`rules-of-hooks`** — catches hooks called conditionally or in loops (see `../03-hooks/00-hook-rules.md`).
- **`exhaustive-deps`** — warns when an effect or memo is missing a dependency (see `../03-hooks/02-useEffect.md`).

Treat `exhaustive-deps` warnings as real bugs to fix, not noise to disable.

---

## Prettier

Prettier formats code automatically so style never comes up in review.

```bash
npm install -D prettier eslint-config-prettier
```

```json
// .prettierrc
{
  "semi": true,
  "singleQuote": false,
  "trailingComma": "all",
  "printWidth": 80
}
```

Add `eslint-config-prettier` as the **last** entry in your ESLint config so ESLint stops enforcing formatting rules that would conflict with Prettier. Division of labor: **ESLint finds problems, Prettier handles layout.**

Add scripts:

```json
{
  "scripts": {
    "lint": "eslint .",
    "format": "prettier --write .",
    "typecheck": "tsc -b"
  }
}
```

---

## Editor setup (VS Code)

Install the **ESLint** and **Prettier** extensions, then add workspace settings:

```json
// .vscode/settings.json
{
  "editor.formatOnSave": true,
  "editor.defaultFormatter": "esbenp.prettier-vscode",
  "editor.codeActionsOnSave": {
    "source.fixAll.eslint": "explicit"
  },
  "typescript.tsdk": "node_modules/typescript/lib"
}
```

Commit `.vscode/settings.json` so the whole team gets the same behavior. The last line makes the editor use the project's TypeScript version rather than its bundled one.

---

## Verify it works

1. Declare `const unused = 1;` — the editor should flag it.
2. Write `const n: number = "text";` — a type error should appear.
3. Call `useState` inside an `if` — ESLint should report a hooks error.
4. Save a badly indented file — Prettier should reformat it.

If any step fails, restart the editor's TypeScript/ESLint servers.

---

## Common mistakes

- **Turning off `strict`** to silence errors — you lose the safety you adopted TypeScript for.
- **Adding the `@` alias in only one place** — it must exist in both `vite.config.ts` and `tsconfig.app.json`.
- **Assuming Vite type-checks** — it doesn't; run `tsc -b` in CI and before building.
- **Disabling `exhaustive-deps`** instead of fixing the effect — hides real stale-value bugs.
- **Letting ESLint and Prettier fight** — add `eslint-config-prettier` and don't enable formatting rules in ESLint.

## Quick summary

- Keep `strict: true`; use `jsx: "react-jsx"`
- Vite doesn't type-check, so run `tsc -b` in CI
- ESLint's `rules-of-hooks` and `exhaustive-deps` catch the most common React bugs
- Prettier formats; ESLint finds problems; `eslint-config-prettier` prevents conflicts
- Share editor settings through `.vscode/settings.json`

## Next

You're set up. Continue to **[`../01-fundamentals/README.md`](../01-fundamentals/README.md)** to start building components, beginning with `00-thinking-in-react.md`.
