# npm and Package Managers

A package manager installs, updates and removes third-party code, and records exactly which versions your project uses.

```
package.json   ─────►  what you ask for   ("^1.2.0")
lockfile       ─────►  what you got       ("1.2.7", exact tree)
node_modules/  ─────►  installed code
```

## Managers

| Manager  | Notes                                                        |
| -------- | ------------------------------------------------------------ |
| **npm**  | Ships with Node, the default                                 |
| **pnpm** | Fast, saves disk with a shared store, strict dependency tree |
| **yarn** | Alternative CLI, workspaces, Plug'n'Play mode                |
| **bun**  | Built into the Bun runtime                                   |

Use one manager per project.

## Starting a project

```bash
npm init -y
```

## `package.json` fields

| Field              | Purpose                                                   |
| ------------------ | --------------------------------------------------------- |
| `name`, `version`  | package identity                                          |
| `type`             | `"module"` (ESM) or `"commonjs"`                          |
| `main` / `exports` | entry points when others import your package              |
| `scripts`          | named commands (`npm run <name>`)                         |
| `dependencies`     | needed at runtime                                         |
| `devDependencies`  | needed only for development (tests, linters, build tools) |
| `peerDependencies` | versions the host project must provide                    |
| `engines`          | supported Node versions                                   |
| `private`          | `true` prevents accidental publishing                     |

```json
{
  "name": "my-app",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "test": "vitest",
    "lint": "eslint ."
  },
  "engines": { "node": ">=20" }
}
```

## Common commands

| Task                          | Command                                            |
| ----------------------------- | -------------------------------------------------- |
| Install everything            | `npm install` (or `npm ci` in CI)                  |
| Add a dependency              | `npm install lodash-es`                            |
| Add a dev dependency          | `npm install -D vitest`                            |
| Remove                        | `npm uninstall lodash-es`                          |
| Run a script                  | `npm run dev` (`npm test`, `npm start` skip `run`) |
| Run a tool without installing | `npx prettier --check .`                           |
| See outdated packages         | `npm outdated`                                     |
| Update within ranges          | `npm update`                                       |
| Security scan                 | `npm audit`                                        |
| See why a package exists      | `npm ls <name>`                                    |

## Semantic versioning

Versions look like `MAJOR.MINOR.PATCH`.

| Part  | Bump when                        |
| ----- | -------------------------------- |
| MAJOR | breaking change                  |
| MINOR | new feature, backward compatible |
| PATCH | bug fix                          |

| Range    | Allows                     |
| -------- | -------------------------- |
| `1.2.3`  | exactly that               |
| `^1.2.3` | `>=1.2.3 <2.0.0` (default) |
| `~1.2.3` | `>=1.2.3 <1.3.0`           |
| `*`      | anything (avoid)           |

Caret on `0.x` is stricter: `^0.2.3` allows only `<0.3.0`.

## Lockfiles

`package-lock.json`, `pnpm-lock.yaml` and `yarn.lock` pin the whole dependency tree.

- **Commit** the lockfile for applications
- Use `npm ci` in CI for clean, reproducible installs
- Never edit lockfiles by hand

## `.gitignore` essentials

```
node_modules/
dist/
.env
```

## Publishing basics

```bash
npm login
npm publish --access public
```

Add `files` or an `.npmignore` so you publish only what is needed.

## Package pitfalls

| Pitfall                                      | Why it hurts                       | Better                          |
| -------------------------------------------- | ---------------------------------- | ------------------------------- |
| Committing `node_modules`                    | Huge repo, platform-specific files | `.gitignore` it                 |
| Mixing managers                              | Conflicting lockfiles              | One manager per repo            |
| `*` or `latest` versions                     | Surprise breakage                  | Caret ranges plus a lockfile    |
| Installing global packages for project tools | Version drift                      | Local devDependencies and `npx` |
| Ignoring `npm audit`                         | Known vulnerabilities              | Review and update regularly     |
| Tiny dependency for one line of code         | Supply-chain risk                  | Write it yourself               |

## Key takeaways

- `package.json` states intent, the lockfile records reality
- `dependencies` for runtime, `devDependencies` for tooling
- `^` allows minor and patch updates; `~` allows patch only
- Use `npm ci` in CI and commit your lockfile

**Next:** [DevTools and Debugging](./04_devtools-and-debugging.md)
