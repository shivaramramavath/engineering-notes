# Linting and Formatting

**Linting** finds likely bugs and rule violations (unused variables, missing hook dependencies, Next.js-specific mistakes). **Formatting** makes code look the same everywhere (quotes, indentation, line width). They solve different problems: use ESLint for the first and Prettier for the second, and keep them from fighting each other.

> Written for Next.js 16. Next 16 removed the `next lint` command and no longer runs linting during `next build`. You run ESLint yourself.

## ESLint

`create-next-app` installs `eslint` and `eslint-config-next` and creates a flat config file:

```js
// eslint.config.mjs
import { defineConfig, globalIgnores } from "eslint/config";
import nextVitals from "eslint-config-next/core-web-vitals";
import nextTs from "eslint-config-next/typescript";

export default defineConfig([
  ...nextVitals,
  ...nextTs,
  globalIgnores([".next/**", "out/**", "build/**", "next-env.d.ts"]),
]);
```

Your generated file may look slightly different depending on the version. The structure is what matters:

- `core-web-vitals` is Next's recommended rule set, with stricter rules for performance-affecting issues.
- `typescript` adds TypeScript-aware rules.
- `globalIgnores` keeps ESLint out of generated and build output.

Run it:

```json
{
  "scripts": {
    "lint": "eslint",
    "lint:fix": "eslint --fix"
  }
}
```

```bash
npm run lint
```

### What the Next.js rules catch

- `<a>` used for internal navigation instead of `next/link`
- `<img>` instead of `next/image` (a performance warning)
- Missing or conflicting `key`s and hook rule violations (via the React rules)
- Misuse of `next/script`, `next/font`, and `next/head` in the App Router

### Adjusting rules

```js
export default defineConfig([
  ...nextVitals,
  ...nextTs,
  {
    rules: {
      "@typescript-eslint/no-unused-vars": ["error", { argsIgnorePattern: "^_" }],
    },
  },
  globalIgnores([".next/**"]),
]);
```

Disable a rule for one line instead of switching it off globally:

```tsx
// eslint-disable-next-line @next/next/no-img-element
<img src={src} alt="" />
```

## Prettier

ESLint does not format; add Prettier if you want automatic formatting.

```bash
npm install -D prettier eslint-config-prettier
```

```json
// .prettierrc
{
  "semi": true,
  "singleQuote": false,
  "trailingComma": "all",
  "printWidth": 100
}
```

```text
# .prettierignore
.next
node_modules
package-lock.json
```

Add scripts:

```json
{
  "scripts": {
    "format": "prettier --write .",
    "format:check": "prettier --check ."
  }
}
```

### Making them coexist

`eslint-config-prettier` turns off ESLint rules that conflict with Prettier. Add it **last** so it wins:

```js
import prettier from "eslint-config-prettier/flat";

export default defineConfig([
  ...nextVitals,
  ...nextTs,
  prettier,
  globalIgnores([".next/**"]),
]);
```

Do not use ESLint to enforce formatting rules too. Let Prettier own formatting and ESLint own correctness.

### Tailwind class sorting

If you use Tailwind, `prettier-plugin-tailwindcss` sorts class names automatically:

```bash
npm install -D prettier-plugin-tailwindcss
```

```json
{ "plugins": ["prettier-plugin-tailwindcss"] }
```

## Where to run them

| When | What | Why |
|---|---|---|
| Editor on save | Prettier (and ESLint fixes) | Instant feedback |
| Pre-commit (optional) | `lint-staged` with Prettier + ESLint on changed files | Keeps bad code out of git |
| CI | `lint`, `format:check`, `typecheck`, then `build` | Authoritative gate |

Because `next build` does not lint in Next 16, **CI must run `npm run lint` explicitly** or lint errors will never block a merge.

## Common mistakes

| Mistake | Effect | Fix |
|---|---|---|
| Expecting `next build` to lint | Lint errors ship | Add `lint` to CI |
| Using `next lint` on Next 16 | Command removed | Use `eslint` directly |
| Prettier and ESLint rules conflict | Code flips format on every save | Add `eslint-config-prettier` last |
| Linting `.next/` | Slow, thousands of errors | Keep it in `globalIgnores` |
| Old `.eslintrc.json` copied from tutorials | Legacy format, may be ignored by current ESLint | Migrate to `eslint.config.mjs` |

## Quick Summary

- ESLint = correctness. Prettier = formatting. Do not mix their jobs.
- Next 16: run `eslint` directly; `next lint` is gone and `next build` no longer lints.
- Add `eslint-config-prettier` last in the config.
- Make CI run lint, format check, typecheck, then build.

## Next

- [Environment Variables](./03-environment-variables.md)
- [Development Workflow](./05-development-workflow.md)
