# Production Config

A TypeScript setup that works on your laptop (`tsx watch`) is not the same as one you can ship. This file covers hardening the compiler settings, building clean output, wiring up linting and CI, and running compiled code in production. It connects to the deployment topics in `16-production/`.

## The production mindset

| Concern | Development | Production |
|---------|-------------|------------|
| Execution | `tsx` runs `.ts` directly | `node` runs compiled `.js` from `dist/` |
| Type checking | In the editor | Enforced in CI (`tsc --noEmit`) |
| Dependencies | Everything | Runtime `dependencies` only |
| Source maps | Helpful | Enabled, so stack traces point at `.ts` lines |

**Never run `tsx` or `ts-node` in production.** They add startup cost and memory overhead, and compile-on-the-fly is one more thing that can fail at runtime. Compile once at build time, run plain JavaScript.

---

## A strict `tsconfig.json`

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2022"],
    "module": "NodeNext",
    "moduleResolution": "NodeNext",

    "rootDir": "src",
    "outDir": "dist",

    "strict": true,
    "noUncheckedIndexedAccess": true,
    "noImplicitOverride": true,
    "noImplicitReturns": true,
    "noFallthroughCasesInSwitch": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "exactOptionalPropertyTypes": true,

    "esModuleInterop": true,
    "forceConsistentCasingInFileNames": true,
    "skipLibCheck": true,
    "resolveJsonModule": true,
    "isolatedModules": true,

    "sourceMap": true,
    "declaration": false,
    "removeComments": true
  },
  "include": ["src"],
  "exclude": ["node_modules", "dist", "**/*.test.ts"]
}
```

What the extra strictness buys you beyond `strict: true`:

| Option | Catches |
|--------|---------|
| `noUncheckedIndexedAccess` | `arr[0]` and `obj[key]` become `T \| undefined` — forces you to handle empty arrays and missing keys |
| `noImplicitOverride` | Requires the `override` keyword, so renaming a base-class method doesn't silently orphan the subclass one |
| `noImplicitReturns` | A function that returns a value on some paths but not others |
| `noFallthroughCasesInSwitch` | Forgotten `break` statements |
| `noUnusedLocals` / `noUnusedParameters` | Dead code (prefix intentionally unused params with `_`) |
| `exactOptionalPropertyTypes` | Distinguishes "property missing" from "property set to `undefined`" |
| `forceConsistentCasingInFileNames` | Imports that work on macOS/Windows but break on Linux servers |

`noUncheckedIndexedAccess` and `exactOptionalPropertyTypes` are the strictest and most likely to produce a wave of errors on an existing codebase. For a **new** project, enable them from day one; for a migration, enable them later, one at a time.

### Separate configs for build and editor

Tests usually need to be type-checked in the editor but excluded from the shipped build. A common split:

```json
// tsconfig.json — used by editor, tests, and `tsc --noEmit`
{
  "extends": "./tsconfig.build.json",
  "compilerOptions": { "noEmit": true },
  "include": ["src", "tests"],
  "exclude": ["node_modules", "dist"]
}
```

```json
// tsconfig.build.json — used for the production build
{
  "compilerOptions": { /* ...strict options from above... */ },
  "include": ["src"],
  "exclude": ["node_modules", "dist", "**/*.test.ts"]
}
```

```json
{
  "scripts": {
    "build": "tsc -p tsconfig.build.json",
    "typecheck": "tsc --noEmit"
  }
}
```

---

## The build pipeline

```json
{
  "scripts": {
    "clean": "rm -rf dist",
    "build": "npm run clean && tsc -p tsconfig.build.json",
    "start": "node --enable-source-maps dist/index.js",
    "dev": "tsx watch src/index.ts",
    "typecheck": "tsc --noEmit",
    "lint": "eslint .",
    "test": "vitest run"
  }
}
```

- **`clean` first** — stale files from deleted sources otherwise linger in `dist/` and can still be imported
- **`--enable-source-maps`** — makes stack traces point at original `.ts` lines instead of compiled `.js` (requires `sourceMap: true`)
- On Windows, replace `rm -rf` with a cross-platform tool such as `rimraf`

---

## Path aliases — and their runtime catch

Aliases avoid `../../../utils/logger` chains:

```json
{
  "compilerOptions": {
    "baseUrl": ".",
    "paths": {
      "@/*": ["src/*"]
    }
  }
}
```

```ts
import { logger } from "@/lib/logger.js";
```

> **The catch:** `tsc` type-checks aliases but **does not rewrite them** in the emitted JavaScript. Node will then fail with `ERR_MODULE_NOT_FOUND` because it has no idea what `@/` means.

Ways to deal with it:

1. **Skip aliases** — relative imports always work. Often the pragmatic choice for backend projects.
2. **Use Node's native `imports` field** in `package.json` (`"#lib/*": "./dist/lib/*"`) — resolved by Node itself at runtime.
3. **Bundle with `tsup`/`esbuild`**, which resolve aliases during bundling.
4. **Post-process** with a tool like `tsc-alias`.

Whichever you pick, verify by actually running `node dist/index.js` — type-checking passing proves nothing about runtime resolution.

---

## Bundling vs plain `tsc`

| | `tsc` | `tsup` / `esbuild` |
|---|---|---|
| Speed | Slower | Very fast |
| Type-checking | Built in | **None** — pair with `tsc --noEmit` |
| Output | One `.js` per `.ts` | Single bundled file (optional) |
| Aliases | Not rewritten | Resolved |

For most Express APIs, plain `tsc` is simple and sufficient. Reach for a bundler when build speed or deployment size matters. Remember the rule either way: **a bundler compiles, it doesn't check** — keep `tsc --noEmit` in CI.

---

## Linting with typescript-eslint

TypeScript catches type errors; ESLint catches *patterns* that are legal but risky.

```bash
npm install --save-dev eslint typescript-eslint
```

```js
// eslint.config.js
import eslint from "@eslint/js";
import tseslint from "typescript-eslint";

export default tseslint.config(
  { ignores: ["dist", "node_modules"] },
  eslint.configs.recommended,
  ...tseslint.configs.recommendedTypeChecked,
  {
    languageOptions: {
      parserOptions: { projectService: true, tsconfigRootDir: import.meta.dirname },
    },
    rules: {
      "@typescript-eslint/no-floating-promises": "error",
      "@typescript-eslint/no-explicit-any": "error",
      "@typescript-eslint/consistent-type-imports": "error",
    },
  }
);
```

Rules that matter most on a backend:

- **`no-floating-promises`** — flags a promise that is neither awaited nor handled. In Node, that's a silent crash waiting to happen (or an unhandled rejection).
- **`no-explicit-any`** — keeps `any` from creeping back in
- **`consistent-type-imports`** — `import type { User }` is erased at compile time, avoiding accidental runtime imports

---

## Typed environment config

Validate environment variables once, at startup, and export a typed object (`16-production/01-environment-management.md`):

```ts
// config/env.ts
import { z } from "zod";

const envSchema = z.object({
  NODE_ENV: z.enum(["development", "test", "production"]).default("development"),
  PORT: z.coerce.number().int().positive().default(3000),
  DATABASE_URL: z.string().url(),
  JWT_SECRET: z.string().min(32),
  LOG_LEVEL: z.enum(["debug", "info", "warn", "error"]).default("info"),
});

const parsed = envSchema.safeParse(process.env);

if (!parsed.success) {
  console.error("Invalid environment configuration:", parsed.error.flatten().fieldErrors);
  process.exit(1);
}

export const env = parsed.data;
export type Env = z.infer<typeof envSchema>;
```

```ts
import { env } from "./config/env.js";
app.listen(env.PORT); // number — not string | undefined
```

The app fails **fast at boot** with a readable message instead of failing mysteriously on the first request that needs the missing variable.

---

## Docker: multi-stage build

Build with the full toolchain, ship only compiled output and production dependencies (`16-production/03-docker-and-compose.md`):

```dockerfile
# ---- Build stage ----
FROM node:22-alpine AS build
WORKDIR /app

COPY package*.json ./
RUN npm ci

COPY tsconfig*.json ./
COPY src ./src
RUN npm run build

# ---- Runtime stage ----
FROM node:22-alpine AS runtime
WORKDIR /app
ENV NODE_ENV=production

COPY package*.json ./
RUN npm ci --omit=dev && npm cache clean --force

COPY --from=build /app/dist ./dist

USER node
EXPOSE 3000
CMD ["node", "--enable-source-maps", "dist/index.js"]
```

Why two stages:

- The final image has **no TypeScript, no `@types/*`, no source files** — smaller and with a smaller attack surface
- `npm ci --omit=dev` installs only runtime dependencies — this is why `typescript` belongs in `devDependencies`
- `USER node` avoids running as root

---

## CI checks

```yaml
# .github/workflows/ci.yml (excerpt)
- run: npm ci
- run: npm run typecheck
- run: npm run lint
- run: npm test
- run: npm run build
```

Order matters for fast feedback: cheap checks first (`typecheck`, `lint`), then tests, then build. Details in `16-production/05-ci-cd.md`.

---

## Gradual migration from JavaScript

You don't have to convert everything at once:

```json
{
  "compilerOptions": {
    "allowJs": true,
    "checkJs": false,
    "strict": false
  }
}
```

1. Add `allowJs` so `.js` and `.ts` files coexist
2. Rename files to `.ts` one module at a time, starting with leaf modules (utilities, types)
3. Tighten flags progressively — first `noImplicitAny`, then `strictNullChecks`, and finally `strict: true`
4. Treat each newly-written file as strict from the start

---

## Common mistakes

- **Running `ts-node`/`tsx` in production** — compile ahead of time and run `node dist/...`
- **Relying on a bundler for type safety** — esbuild/tsup strip types without checking them. Keep `tsc --noEmit` in CI.
- **Path aliases that compile but crash at runtime** — `tsc` doesn't rewrite them. Test the built output.
- **`typescript` in `dependencies`** — bloats production installs; it's a `devDependency`.
- **Skipping `clean` before build** — deleted files linger in `dist/`.
- **No source maps** — production stack traces point at unreadable compiled lines.
- **Using `process.env.X!` throughout the code** — validate once at startup instead.
- **Forgetting `forceConsistentCasingInFileNames`** — works on macOS, breaks on Linux CI.

## Quick summary

- Develop with `tsx`; **ship compiled JavaScript** run by plain `node`
- Go beyond `strict` — `noUncheckedIndexedAccess`, `noImplicitOverride`, `noImplicitReturns`, and friends — especially on new projects
- Split `tsconfig.build.json` (shipped code) from `tsconfig.json` (editor, tests, type-check)
- Enable source maps and run with `--enable-source-maps`
- Path aliases need a runtime strategy; verify against built output
- Lint with `typescript-eslint` (`no-floating-promises` is the standout)
- Validate env vars with Zod at startup and export a typed config
- Multi-stage Docker builds keep the runtime image small; CI runs typecheck → lint → test → build

## Next

**`18-design-patterns/00-README.md`** moves on to classic design patterns — singleton, factory, strategy, observer — with TypeScript-friendly examples.
