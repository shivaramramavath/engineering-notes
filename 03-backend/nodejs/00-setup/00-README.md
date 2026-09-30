# 00 — Setup

Everything you need before writing any real Node.js code: installing Node itself, and understanding npm and `package.json` well enough to start a project.

## In this section

| File                         | Covers                                                                                                |
| ---------------------------- | ----------------------------------------------------------------------------------------------------- |
| `01-getting-started.md`      | Installing Node (and why via a version manager), verifying the install, running your first script     |
| `02-npm-and-package-json.md` | npm basics, `package.json`, and `package-lock.json` — the three things every Node project starts with |

## Why this comes first

Every later section assumes Node and npm are installed and that `package.json` makes sense to you — it's the file every single command (`npm install`, `npm run`, `npm test`) reads from. Getting comfortable with it now avoids a lot of confusion later when sections start referencing scripts, dependencies, and dev dependencies without re-explaining them each time.

## What you should be able to do after this section

- Install Node via a version manager (not a single global install) and switch versions per project
- Run `node --version` / `npm --version` and get sensible output
- Initialize a new project with `npm init` and understand every field `package.json` generates
- Explain the difference between `package.json` and `package-lock.json`, and why both get committed to Git

## Next

**`01-fundamentals`** covers how Node actually runs your JavaScript — the runtime, global objects, and the module system.
