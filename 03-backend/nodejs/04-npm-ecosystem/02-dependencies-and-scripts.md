# Dependencies & Scripts

Managing what your project depends on, and automating the commands you run against it. Both are day-to-day npm skills beyond the basics covered in `00-setup/02-npm-and-package-json.md`.

## `dependencies` vs `devDependencies`, revisited

```bash
npm install express         # → dependencies
npm install -D eslint         # → devDependencies
```

The test: **would a production deployment need this to actually run the app?**

|                              | Goes in           | Example                                                      |
| ---------------------------- | ----------------- | ------------------------------------------------------------ |
| Needed at runtime            | `dependencies`    | `express`, `pg`, `dotenv`, `jsonwebtoken`                    |
| Only needed while developing | `devDependencies` | `eslint`, `prettier`, `nodemon`, `typescript`, a test runner |

```bash
npm install --omit=dev      # production install — skips devDependencies entirely
```

Getting this split right matters for image size and install speed in production (see the Docker guide's image-optimization file) — a `devDependencies` entry mistakenly placed in `dependencies` bloats every production install with tools that will never run there.

---

## `peerDependencies` — a quick mention

```json
{
  "peerDependencies": {
    "react": "^18.0.0"
  }
}
```

Used by packages meant to plug into a host project that already has its own copy of something (a React component library expecting the _consuming app_ to provide React, rather than bundling its own copy). You'll encounter this mostly as a package author, rarely as an app developer — worth recognizing the term when npm warns about an unmet peer dependency.

---

## Auditing for vulnerabilities

```bash
npm audit
```

```
found 3 vulnerabilities (1 moderate, 2 high)
```

```bash
npm audit fix          # attempts to automatically upgrade to non-vulnerable versions
npm audit fix --force    # also allows major version bumps to fix them (riskier — review changes)
```

Worth running periodically, and often wired into CI to catch newly-disclosed vulnerabilities in existing dependencies over time, not just at install time.

---

## npm scripts in depth

```json
{
  "scripts": {
    "dev": "nodemon src/server.js",
    "build": "tsc",
    "start": "node dist/server.js",
    "lint": "eslint .",
    "lint:fix": "eslint . --fix",
    "test": "node --test",
    "format": "prettier . --write"
  }
}
```

```bash
npm run dev
npm start          # "start" and "test" don't need "run"
npm test
```

### `pre`/`post` hooks

```json
{
  "scripts": {
    "build": "tsc",
    "prebuild": "npm run lint",
    "postbuild": "echo Build complete"
  }
}
```

```bash
npm run build
```

npm automatically runs `prebuild` before, and `postbuild` after, any script named `build` — this works for any script name, not just `build`/`start`/`test`.

### Passing arguments through to a script

```bash
npm run test -- --watch
```

Everything after `--` is passed through to the underlying command — here, `node --test --watch` (if that's what `"test"` maps to) rather than being interpreted by npm itself.

### Chaining scripts explicitly

```json
{
  "scripts": {
    "lint": "eslint .",
    "typecheck": "tsc --noEmit",
    "check": "npm run lint && npm run typecheck"
  }
}
```

`&&` runs the next command only if the previous one succeeded (exit code 0) — the standard way to compose several scripts into one, beyond what automatic `pre`/`post` hooks cover.

### Running scripts in parallel

```bash
npm install -D concurrently
```

```json
{
  "scripts": {
    "dev": "concurrently \"npm run dev:server\" \"npm run dev:client\""
  }
}
```

Plain npm scripts run sequentially; a small utility like `concurrently` is the common way to run multiple long-lived processes (a backend and a frontend dev server, for example) side by side from one command.

### Referencing environment variables in a script

```json
{
  "scripts": {
    "start": "NODE_ENV=production node server.js"
  }
}
```

This syntax works on macOS/Linux but **not** natively on Windows PowerShell/cmd. `cross-env` (`npm install -D cross-env`) is the standard fix for a script that needs to set an environment variable and run identically on every OS:

```json
{
  "scripts": {
    "start": "cross-env NODE_ENV=production node server.js"
  }
}
```

---

## `npx` — run a package without installing it globally

```bash
npx create-react-app my-app
npx cowsay "Hello!"
```

`npx` downloads (and caches) a package temporarily just to run its CLI once, without permanently installing it globally on your machine — the standard way to run one-off generators, migration tools, or CLI utilities you don't need installed all the time.

### Running a locally-installed CLI tool

```bash
npx eslint .
```

If a package is already installed as a project dependency (e.g. `eslint` in `devDependencies`), `npx` runs the local copy from `node_modules/.bin/` rather than downloading anything — this is actually the more common real-world use of `npx`: running your project's own installed tools without needing to reference the full `node_modules/.bin/` path or add it to your system `PATH`.

---

## Common mistakes

- **Putting a dev-only tool in `dependencies`** — bloats production installs unnecessarily; use `-D`/`devDependencies`.
- **Forgetting `--omit=dev` for production installs** — installs every dev tool (linters, test runners) into a production environment that will never use them.
- **Writing OS-specific scripts (`NODE_ENV=x command`)** without `cross-env` — works for the author, breaks for a Windows teammate or CI runner.
- **Not running `npm audit` periodically** — vulnerabilities get disclosed in already-installed dependencies over time, not just at install time.

## Quick summary

- The dependencies/devDependencies split should track "does production need this to run" — get it right for leaner production installs
- `npm audit`/`audit fix` surface and often auto-resolve known vulnerabilities in your dependency tree
- `pre`/`post` script hooks and `&&` chaining let you compose npm scripts without external tooling; `concurrently` handles running several long-lived scripts side by side
- `cross-env` keeps a script that sets environment variables working identically across operating systems
- `npx` runs a package's CLI without a permanent global install — and is also how you invoke your project's own locally-installed tools

## Next

**`03-publishing-a-package.md`** covers putting your own package on the npm registry.
