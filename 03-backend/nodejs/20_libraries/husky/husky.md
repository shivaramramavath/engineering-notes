# Husky — Reference Guide

## What is Husky?

Husky makes it easy to use **Git hooks** in Node.js/JavaScript/TypeScript projects. Git hooks are scripts Git runs automatically at certain points (like committing or pushing), and Husky lets you wire your own commands — lint, typecheck, format, test — into those points, stopping the operation if a check fails.

```
git commit → pre-commit hook → lint / typecheck / format → commit created (or blocked)
```

Husky doesn't replace ESLint, Prettier, TypeScript, or your test runner — it just **connects Git events to the commands you already have**.

## Why use it?

Without Husky, developers have to remember to run `lint`, `typecheck`, and `format:check` manually before every commit. Husky automates that:

- Prevents known issues from entering commits
- Standardizes checks across the team
- Integrates with ESLint, Prettier, TypeScript, tests
- Can enforce commit-message conventions

## Common Git hooks

| Hook                 | Runs                                   |
| -------------------- | -------------------------------------- |
| `pre-commit`         | Before a commit is created             |
| `commit-msg`         | To validate the commit message         |
| `pre-push`           | Before pushing                         |
| `prepare-commit-msg` | Before the commit message editor opens |
| `post-commit`        | After a commit                         |
| `post-checkout`      | After checkout                         |
| `post-merge`         | After a merge                          |

Most Node.js projects only need `pre-commit`, `commit-msg`, and `pre-push`.

## Install & initialize

```bash
npm install -D husky
npx husky init
```

`husky init` creates `.husky/pre-commit` and adds a `prepare` script to `package.json`:

```json
{
  "scripts": {
    "prepare": "husky"
  }
}
```

`prepare` is an npm lifecycle script — it runs automatically on `npm install`, so when a teammate clones the repo and runs `npm install`, Husky's hooks are set up for them too.

## Verify the setup

```bash
npm list husky              # confirm husky is installed
git config core.hooksPath   # should point to .husky/_
cat .husky/pre-commit       # (Get-Content .husky/pre-commit on PowerShell)
```

## Configure `pre-commit`

Edit `.husky/pre-commit`:

```sh
npm run lint
npm run typecheck
npm run format:check
```

Commands run sequentially; if one fails (non-zero exit code), the rest don't run and the commit is aborted.

```json
{
  "scripts": {
    "typecheck": "tsc --noEmit",
    "lint": "eslint .",
    "lint:fix": "eslint . --fix",
    "format": "prettier . --write",
    "format:check": "prettier . --check",
    "prepare": "husky"
  }
}
```

## `pre-push`

Use for checks too slow for every commit — tests, full type-checking, builds:

```sh
# .husky/pre-push
npm run typecheck
npm run lint
npm test
```

|              | Good for                                           |
| ------------ | -------------------------------------------------- |
| `pre-commit` | Fast checks: ESLint, Prettier, `lint-staged`       |
| `pre-push`   | Slower checks: full TypeScript check, tests, build |

## `commit-msg` (conventional commits)

`.husky/commit-msg` can validate commit message format, typically with a tool like **Commitlint**:

```
feat: add login
fix: resolve authentication bug
docs: update README
refactor: simplify auth middleware
chore: update dependencies
```

Pattern: `type: description`.

## Speed it up with `lint-staged`

Running ESLint over the whole repo on every commit gets slow as a project grows. `lint-staged` runs commands only on staged files.

```bash
npm install -D lint-staged
```

```json
{
  "lint-staged": {
    "*.{js,jsx,ts,tsx}": ["eslint --fix", "prettier --write"],
    "*.{json,md,css}": ["prettier --write"]
  }
}
```

```sh
# .husky/pre-commit
npx lint-staged
```

## Husky vs CI/CD

Husky gives fast **local** feedback; it isn't a substitute for CI. Hooks can be skipped with `--no-verify`, so CI remains the authoritative check:

```bash
git commit --no-verify   # skips commit hooks
git push --no-verify     # skips push hooks
```

Avoid bypassing routinely — if a hook blocks legitimate work often, fix the hook rather than skipping it.

## Testing it works

```bash
# introduce an intentional lint error, then:
git add .
git commit -m "test: husky"
# → commit should fail with the lint error

# fix the issue, then:
git add .
git commit -m "test: husky"
# → commit should succeed
```

## Troubleshooting

| Symptom                    | Check                                                                                                                                          |
| -------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| Hook doesn't run           | `git config core.hooksPath` should be `.husky/_`                                                                                               |
| Unsure hook exists         | `ls .husky` (or `Get-ChildItem .husky`)                                                                                                        |
| Husky installed?           | `npm list husky`                                                                                                                               |
| Is this even a git repo?   | `git status` (run `git init` if not)                                                                                                           |
| Hook fails but unclear why | Run the underlying command directly first — `npm run lint`, `npm run typecheck`, `npm run format:check` — before assuming Husky is the problem |

**Windows line endings:** if you see `LF will be replaced by CRLF`, add a `.gitattributes` to standardize line endings, especially for shell-based hooks:

```gitattributes
* text=auto
*.js text eol=lf
*.ts text eol=lf
.husky/* text eol=lf
```

## Example project layout

```
project/
├── src/
├── .husky/
│   ├── pre-commit
│   └── pre-push
├── eslint.config.js
├── .prettierrc
├── tsconfig.json
└── package.json
```

## Quick reference

```bash
npm install -D husky         # install
npx husky init                # initialize
git config core.hooksPath     # verify (.husky/_)
git commit --no-verify        # bypass hooks (use sparingly)
```

```sh
# .husky/pre-commit
npm run lint
npm run typecheck
npm run format:check

# .husky/pre-push
npm run typecheck
npm test
npm run build
```

## Mental model

```
Git event (commit / push)
        ↓
      Husky
        ↓
  your commands (lint, typecheck, test, build)
        ↓
  pass → continue    fail → stop
```
