# Node.js and npm

React apps are built with JavaScript tooling that runs on Node.js: the dev server, the bundler, the linter, and the test runner are all Node programs. npm is the package manager that installs them (and React itself). You don't write Node code here — you just need a working install and a clear picture of what `package.json` does.

## Prerequisites

None.

---

## Installing Node.js

Install the **LTS** (long-term support) release. Two common approaches:

- **Official installer** from nodejs.org — simplest for beginners.
- **A version manager** such as `nvm` or `fnm` — better long term, since different projects often need different Node versions and you can switch with one command.

Verify the install:

```bash
node --version
npm --version
```

Modern build tools set a minimum Node version. If a tool refuses to start, check its docs for the required version and upgrade Node before anything else.

---

## What npm does

npm installs packages from a public registry into your project's `node_modules/` folder and records them in `package.json`.

```bash
npm install react react-dom     # add runtime dependencies
npm install -D typescript       # add a dev-only dependency
npm uninstall lodash            # remove a package
npm install                     # install everything listed in package.json
```

Alternatives like `pnpm` and `yarn` do the same job with different trade-offs (speed, disk usage). Pick one per project and don't mix them.

---

## `package.json`

```json
{
  "name": "my-app",
  "private": true,
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "tsc -b && vite build",
    "preview": "vite preview",
    "lint": "eslint ."
  },
  "dependencies": {
    "react": "^19.0.0",
    "react-dom": "^19.0.0"
  },
  "devDependencies": {
    "typescript": "~5.6.0",
    "vite": "^6.0.0"
  }
}
```

| Field | Purpose |
|-------|---------|
| `scripts` | Named commands. Run with `npm run <name>` (`npm start` and `npm test` also work without `run`) |
| `dependencies` | Packages needed by the app at runtime |
| `devDependencies` | Tooling needed only to build, lint, or test |
| `"type": "module"` | Treat `.js` files as ES modules (`import`/`export`) |
| `"private": true` | Prevents accidentally publishing the app to npm |

**Dependencies vs devDependencies:** for a front-end app that gets bundled, the distinction is mostly organizational, but keep it correct — it matters when you publish a library.

---

## Versions and semver

Versions look like `MAJOR.MINOR.PATCH`:

- `^19.0.0` allows minor and patch updates (`19.x.x`)
- `~5.6.0` allows patch updates only (`5.6.x`)
- `19.0.0` pins an exact version

Major versions can contain breaking changes, so upgrade them deliberately.

---

## The lockfile

`package-lock.json` (or `pnpm-lock.yaml`, `yarn.lock`) records the **exact** versions installed. **Commit it.** It guarantees teammates and CI install the same versions you tested with.

Use `npm ci` in CI instead of `npm install` — it installs strictly from the lockfile and fails if it's out of sync.

---

## `npx` and one-off commands

`npx` runs a package's command without installing it globally:

```bash
npx create-vite@latest
npx eslint .
```

---

## `.gitignore`

Never commit `node_modules/` or build output:

```
node_modules
dist
.env.local
```

---

## Common mistakes

- **Committing `node_modules/`** — huge, and reproducible from the lockfile.
- **Deleting the lockfile to "fix" an error** — you lose version guarantees; fix the real conflict instead.
- **Mixing package managers** — leaves multiple lockfiles and inconsistent installs.
- **Installing packages globally for project tools** — use project-local installs so versions are shared with the team.
- **Running an old Node version** — causes confusing build errors; check the tool's minimum version first.

## Quick summary

- Install the Node LTS release; a version manager helps across projects
- npm installs packages into `node_modules/` and tracks them in `package.json`
- `scripts` are named commands you run with `npm run`
- Commit the lockfile; use `npm ci` in CI
- Pick one package manager per project

## Next

**[`02-vite.md`](./02-vite.md)** uses these tools to create and run your first React project.
