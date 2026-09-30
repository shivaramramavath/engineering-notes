# 04 — npm Ecosystem

`00-setup/02-npm-and-package-json.md` covered the basics of `package.json` and installing packages. This section goes deeper into using npm well day to day — how version numbers actually work, managing dependencies and scripts effectively, and publishing your own package.

## In this section

| File                             | Covers                                                                          |
| -------------------------------- | ------------------------------------------------------------------------------- |
| `01-semantic-versioning.md`      | What `major.minor.patch` actually means, and how `^`/`~` ranges use it          |
| `02-dependencies-and-scripts.md` | Managing `dependencies`/`devDependencies` well, npm scripts in depth, and `npx` |
| `03-publishing-a-package.md`     | Publishing your own package to the npm registry                                 |

## Why this matters beyond "installing packages"

Every dependency your project uses has its own version history, and every update carries some risk. Understanding semantic versioning is what lets you read a `package.json` and know whether an update is likely safe or likely to break something, rather than treating version numbers as arbitrary. Scripts and `npx` are the daily-driver tools for actually running your project's tasks consistently across every machine that clones it.

## What you should be able to do after this section

- Read a version like `^4.18.2` and know exactly what range of versions it permits, and why
- Decide correctly whether a new dependency belongs in `dependencies` or `devDependencies`
- Write and chain npm scripts (including `pre`/`post` hooks) for your project's common tasks
- Use `npx` to run a package's CLI without installing it globally
- Publish a package to npm, including versioning it correctly for updates

## Next

**`05-http-web`** moves from tooling into the actual protocol every Node backend deals with — HTTP methods, status codes, headers, and more.
