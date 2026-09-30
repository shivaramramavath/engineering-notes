# npm & `package.json`

npm (Node Package Manager) is what installs and manages dependencies; `package.json` is the file that describes your project to npm — and to every other tool built around it.

## Creating a `package.json`

```bash
npm init          # interactive — asks name, version, description, etc.
npm init -y         # skip the questions, use sensible defaults
```

```json
{
  "name": "my-project",
  "version": "1.0.0",
  "description": "",
  "main": "index.js",
  "scripts": {
    "test": "echo \"Error: no test specified\" && exit 1"
  },
  "keywords": [],
  "author": "",
  "license": "ISC"
}
```

---

## Anatomy of `package.json`

| Field             | Meaning                                                                                                     |
| ----------------- | ----------------------------------------------------------------------------------------------------------- |
| `name`            | Package name — must be lowercase, no spaces, used if you ever publish it                                    |
| `version`         | Follows semantic versioning (`major.minor.patch`) — see `04-npm-ecosystem/01-semantic-versioning.md`        |
| `main`            | Entry point file when something `require()`s/`import`s your package                                         |
| `type`            | `"module"` for ES Modules, or omitted/`"commonjs"` for CommonJS — see `01-fundamentals/03-module-system.md` |
| `scripts`         | Named shell commands runnable via `npm run <name>`                                                          |
| `dependencies`    | Packages needed to _run_ the app                                                                            |
| `devDependencies` | Packages only needed _during development_ (linters, test runners, build tools)                              |
| `engines`         | Optionally pins a required Node/npm version range                                                           |

```json
{
  "name": "my-api",
  "version": "1.0.0",
  "type": "module",
  "engines": {
    "node": ">=20.0.0"
  }
}
```

---

## Installing packages

```bash
npm install express              # adds to "dependencies"
npm install -D eslint prettier     # adds to "devDependencies" (-D / --save-dev)
npm install express@4.18.2          # a specific version
npm install                           # installs everything already listed in package.json
```

### `dependencies` vs `devDependencies`

```json
{
  "dependencies": {
    "express": "^4.18.2"
  },
  "devDependencies": {
    "eslint": "^9.0.0",
    "nodemon": "^3.0.0"
  }
}
```

- **`dependencies`** — required for the app to actually run in production (Express, a database driver, `dotenv`)
- **`devDependencies`** — only needed while developing (ESLint, Prettier, a test runner, TypeScript's compiler) — a production deployment can skip installing these entirely (`npm install --omit=dev`, as covered in the Docker guide's image-optimization file)

---

## Version ranges: what `^` and `~` mean

```json
{
  "dependencies": {
    "express": "^4.18.2",
    "lodash": "~4.17.21",
    "some-lib": "4.0.0"
  }
}
```

| Prefix              | Allows                                                                    |
| ------------------- | ------------------------------------------------------------------------- |
| `^4.18.2`           | Any `4.x.x` version ≥ `4.18.2` — the default, and generally what you want |
| `~4.17.21`          | Any `4.17.x` version ≥ `4.17.21` — patch updates only                     |
| `4.0.0` (no prefix) | Exactly that version, nothing else                                        |

Full explanation of semantic versioning in `04-npm-ecosystem/01-semantic-versioning.md`.

---

## `package-lock.json` — exact, reproducible installs

```bash
npm install express
```

This updates **both** `package.json` (the version range, e.g. `^4.18.2`) and `package-lock.json` (the _exact_ resolved version, plus the exact versions of every nested dependency).

### Why both files matter

- `package.json` says "any compatible 4.x version of Express is fine"
- `package-lock.json` says "this exact tree of versions is what was actually installed and tested"

```bash
npm install       # respects package-lock.json if present — reproducible
npm ci               # stricter: installs EXACTLY what's in the lockfile, fails if
                        # package.json and the lockfile disagree — the standard choice for CI/production
```

**Always commit `package-lock.json` to version control.** Without it, two developers (or a developer and a CI server) running `npm install` at different times could end up with subtly different dependency versions — a common source of "works on my machine" bugs that a lockfile eliminates entirely.

---

## npm scripts

```json
{
  "scripts": {
    "start": "node server.js",
    "dev": "nodemon server.js",
    "test": "node --test",
    "lint": "eslint ."
  }
}
```

```bash
npm run dev
npm start        # "start" and "test" are special — they can be run without "run"
npm test
```

Every other script needs the explicit `run`: `npm run lint`, not `npm lint`.

### Chaining scripts

```json
{
  "scripts": {
    "build": "tsc",
    "prebuild": "npm run lint",
    "postbuild": "echo Build complete"
  }
}
```

npm automatically runs a `pre<name>` script before, and a `post<name>` script after, a script of the matching base name — `npm run build` here runs `lint` first, then `tsc`, then the completion message, without you needing to chain them manually.

---

## Removing and updating packages

```bash
npm uninstall express        # remove a package
npm update                     # update packages within their allowed version ranges
npm outdated                     # see what's outdated, and by how much
```

```bash
npx npm-check-updates -u        # (a popular third-party tool) bump package.json itself
                                    # to the latest versions, ignoring the ^/~ constraints
npm install                        # then actually install them
```

---

## `node_modules` — never commit this

```gitignore
node_modules
```

`node_modules` can be regenerated at any time from `package.json` + `package-lock.json` via `npm install` — committing it bloats a repository enormously and is unnecessary given the lockfile already captures everything needed to reproduce it exactly.

## Quick summary

- `package.json` describes the project: metadata, scripts, and dependency version _ranges_
- `dependencies` are needed to run the app; `devDependencies` only during development
- `^`/`~` control how much a dependency is allowed to drift on update; no prefix pins an exact version
- `package-lock.json` records the _exact_ installed tree — always commit it, and use `npm ci` in CI/production for reproducible installs
- Never commit `node_modules` — it's fully regenerable from the two files above

## Section complete

With Node installed and `package.json`/npm basics in place, **`01-fundamentals`** covers how Node actually executes your code — the runtime, global objects, and the module system.
