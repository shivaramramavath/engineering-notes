# Git Hooks and Commits

Code review and CI catch problems late: after a push, after a failed pipeline, after someone has context-switched. **Git hooks** run scripts at moments in your Git workflow (before a commit, before a push), catching formatting errors, lint failures, and bad commit messages on your own machine in seconds.

This file covers **Husky** (share hooks with the whole team), **lint-staged** (run tools only on changed files), **commitlint** with **Conventional Commits** (consistent messages), and lighter alternatives.

See also: [Linting and Formatting](./04_linting-and-formatting.md), [Tooling](../00_setup/05_tooling.md), [Testing Fundamentals](../21_testing/01_testing-fundamentals.md).

## How Git hooks work

Git looks for executable scripts in `.git/hooks/` and runs them at specific events. If a hook exits with a non-zero status, Git **aborts** the operation.

| Hook                           | Runs                             | Typical use                                                        |
| ------------------------------ | -------------------------------- | ------------------------------------------------------------------ |
| `pre-commit`                   | Before a commit is created       | Lint and format staged files, secret scan                          |
| `prepare-commit-msg`           | Before the message editor opens  | Prefill the message (for example a ticket id from the branch name) |
| `commit-msg`                   | After you write the message      | **Validate the message format** (commitlint)                       |
| `pre-push`                     | Before `git push` sends anything | Run tests or type checks                                           |
| `post-merge` / `post-checkout` | After merge or checkout          | Reinstall dependencies when the lockfile changed                   |

The catch: `.git/` is **not version-controlled**, so hooks placed there are not shared. Every teammate would need to set them up by hand. Husky solves that.

## Husky

**Husky** stores hooks in a committed `.husky/` folder and points Git at it, so everyone who runs `npm install` gets the same hooks automatically.

### Setup

```bash
npm install --save-dev husky
npx husky init
```

`husky init` does three things:

1. Adds `"prepare": "husky"` to `package.json` scripts (npm runs `prepare` after every `npm install`, which activates the hooks for each clone)
2. Creates `.husky/pre-commit` containing a sample command
3. Configures Git to use the `.husky/` directory for hooks

```
.husky/
├── pre-commit          # plain shell commands
└── commit-msg
```

A hook is just a shell script (in Husky 9 you write the commands directly, with no boilerplate header):

```bash
# .husky/pre-commit
npx lint-staged
```

```bash
# .husky/pre-push
npm test
```

Commit the `.husky/` folder so the team shares it.

### Skipping and CI behavior

```bash
git commit --no-verify -m "wip"      # skip pre-commit and commit-msg once (use rarely)
git push --no-verify
HUSKY=0 git commit -m "..."          # disable Husky for one command
```

On CI and in production Docker builds you usually do not want hooks installed:

```json
{
  "scripts": {
    "prepare": "husky || true"
  }
}
```

The `|| true` stops `npm ci --omit=dev` from failing when Husky (a devDependency) is absent. You can also set `HUSKY=0` in the environment, or install with `--ignore-scripts`.

### Hooks are a convenience, not a gate

Anyone can bypass local hooks (`--no-verify`) or clone without installing them. Treat hooks as **fast feedback** and CI as the **real enforcement**: run the same checks in your pipeline.

### Troubleshooting

| Symptom                                       | Likely cause                                                                                                      |
| --------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| Hook never runs                               | Dependencies installed with `--ignore-scripts`, or `prepare` did not run. Run `npx husky`                         |
| `husky: command not found` in CI or Docker    | Production install without devDependencies. Use `"prepare": "husky \|\| true"`                                    |
| Works in terminal, fails in a GUI Git client  | The GUI's `PATH` lacks Node (common with version managers). Make sure Node is available to the GUI                |
| Hook runs on the wrong Git repo in a monorepo | Install Husky at the repo root, or pass the directory: `"prepare": "cd .. && husky app/.husky"`                   |
| Slow commits                                  | Running the whole test suite or linting the whole project. Use lint-staged and move slow work to `pre-push` or CI |

## lint-staged

Running ESLint and Prettier on the **entire project** at every commit is slow. **lint-staged** runs commands only on the files that are **staged** for the commit.

```bash
npm install --save-dev lint-staged
```

```json
{
  "lint-staged": {
    "*.{js,mjs,cjs,ts,tsx}": [
      "eslint --fix --max-warnings=0",
      "prettier --write"
    ],
    "*.{json,md,yml,yaml,css,html}": "prettier --write"
  }
}
```

```bash
# .husky/pre-commit
npx lint-staged
```

What happens on `git commit`:

1. lint-staged finds staged files matching each glob
2. It runs the commands, passing the file names as arguments
3. Fixes made by `--fix` or `--write` are **re-added** to the commit automatically
4. If any command fails, the commit is aborted and the output explains why

Commands inside one glob run **in order** (so `eslint --fix` runs before `prettier --write`), while different globs run in parallel.

### A config file for more control

```js
// lint-staged.config.js
export default {
  "*.{js,ts}": ["eslint --fix --max-warnings=0", "prettier --write"],

  // a function ignores the file list and runs once: use it for whole-project tools
  "*.ts": () => "tsc --noEmit",

  "*.{json,md,yml}": "prettier --write",
};
```

Whole-project commands such as `tsc --noEmit` cannot take a list of files (they would ignore your `tsconfig.json`), so wrap them in a function.

### Tips

- Keep `pre-commit` under a few seconds, or people reach for `--no-verify`
- Use `--max-warnings=0` so warnings do not accumulate unnoticed
- Do not run the full test suite in `pre-commit`; run related tests (`vitest related --run`) or leave tests to `pre-push` and CI
- lint-staged stashes unstaged changes while it works so partially staged files are handled correctly

## Conventional Commits

A consistent commit format makes history readable and enables **automatic changelogs and version bumps**.

```
<type>(<optional scope>): <short summary>

<optional body: what and why>

<optional footer: BREAKING CHANGE: ..., Closes #123>
```

| Type       | Use for                                                 | Version impact (semver) |
| ---------- | ------------------------------------------------------- | ----------------------- |
| `feat`     | A new feature                                           | minor                   |
| `fix`      | A bug fix                                               | patch                   |
| `docs`     | Documentation only                                      | none                    |
| `style`    | Formatting, whitespace (no logic change)                | none                    |
| `refactor` | Code change that neither fixes a bug nor adds a feature | none                    |
| `perf`     | Performance improvement                                 | patch                   |
| `test`     | Adding or fixing tests                                  | none                    |
| `build`    | Build system or dependencies                            | none                    |
| `ci`       | CI configuration                                        | none                    |
| `chore`    | Maintenance that fits nowhere else                      | none                    |
| `revert`   | Reverts an earlier commit                               | varies                  |

Breaking changes use `!` or a footer and trigger a **major** bump:

```
feat(api)!: remove the /v1/users endpoint

BREAKING CHANGE: clients must migrate to /v2/users
```

Examples:

```
feat(auth): add refresh token rotation
fix(cart): prevent negative quantities
docs: explain how to run the integration tests
refactor(users): extract password hashing into its own module
chore(deps): bump pino to 9.x
```

Guidelines: imperative mood ("add", not "added"), subject under about 72 characters, no trailing period, explain **why** in the body when it is not obvious.

## commitlint

**commitlint** checks every commit message against rules and rejects the commit if it does not comply.

```bash
npm install --save-dev @commitlint/cli @commitlint/config-conventional
```

```js
// commitlint.config.js
export default {
  extends: ["@commitlint/config-conventional"],
  rules: {
    "subject-case": [
      2,
      "never",
      ["sentence-case", "start-case", "pascal-case", "upper-case"],
    ],
    "header-max-length": [2, "always", 72],
    "scope-enum": [1, "always", ["api", "auth", "cart", "deps", "docs", "ui"]],
  },
};
```

Rule values are `[level, applicability, value]`: level `0` disables, `1` warns, `2` errors.

Wire it up with a `commit-msg` hook:

```bash
# .husky/commit-msg
npx --no -- commitlint --edit $1
```

```bash
git commit -m "fixed stuff"
# ✖ subject may not be empty
# ✖ type may not be empty

git commit -m "fix(cart): prevent negative quantities"
# ✔ accepted
```

Also validate in CI so pull requests that bypassed local hooks are caught (for example by linting the PR title if you squash-merge):

```bash
npx commitlint --from origin/main --to HEAD --verbose
```

### Helpers for writing messages

| Tool                                                   | What it does                                                       |
| ------------------------------------------------------ | ------------------------------------------------------------------ |
| **commitizen** (`cz`) with `cz-conventional-changelog` | Interactive prompts that build a valid message                     |
| **cz-git**                                             | Commitizen adapter with a friendlier UI and commitlint integration |
| **VS Code extensions**                                 | Conventional Commits helpers in the editor                         |

## Automating releases from commits

Once messages are structured, tools can compute versions and changelogs:

| Tool                        | Approach                                                                                                    |
| --------------------------- | ----------------------------------------------------------------------------------------------------------- |
| **changesets**              | Contributors add small "change" files; the tool bumps versions and writes changelogs (popular in monorepos) |
| **semantic-release**        | Fully automatic releases from commit messages in CI                                                         |
| **release-it**              | Interactive or scripted release flow (version, tag, changelog, publish)                                     |
| **release-please** (Google) | Opens a release pull request from Conventional Commits                                                      |

Pick one, wire it into CI, and publish with provenance where your registry supports it. See [Managing Dependencies](./10_managing-dependencies.md).

## A complete, minimal setup

```bash
npm install --save-dev husky lint-staged prettier eslint @commitlint/cli @commitlint/config-conventional
npx husky init
```

```bash
# .husky/pre-commit
npx lint-staged
```

```bash
# .husky/commit-msg
npx --no -- commitlint --edit $1
```

```bash
# .husky/pre-push (optional)
npm run typecheck && npm test
```

```json
{
  "scripts": { "prepare": "husky" },
  "lint-staged": {
    "*.{js,ts}": ["eslint --fix --max-warnings=0", "prettier --write"],
    "*.{json,md,yml}": "prettier --write"
  }
}
```

And the same checks in CI as the real gate:

```yaml
# .github/workflows/ci.yml
name: ci
on: [push, pull_request]
jobs:
  verify:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with: { fetch-depth: 0 }
      - uses: actions/setup-node@v4
        with: { node-version-file: .nvmrc, cache: npm }
      - run: npm ci
      - run: npm run format:check
      - run: npm run lint
      - run: npm run typecheck
      - run: npm test
      - run: npx commitlint --from=origin/main --to=HEAD
        if: github.event_name == 'pull_request'
```

## Alternatives

| Tool                              | Notes                                                                                                             |
| --------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| **Husky**                         | The default in the JavaScript world; hooks are shell scripts in `.husky/`                                         |
| **simple-git-hooks**              | Tiny: hooks defined in `package.json`, no folder. Good for small projects                                         |
| **Lefthook**                      | Fast Go binary, language-agnostic, parallel execution, config in `lefthook.yml`. Good for polyglot or large repos |
| **pre-commit** (Python framework) | Large hook ecosystem across languages; uses `.pre-commit-config.yaml`                                             |
| **Plain `core.hooksPath`**        | `git config core.hooksPath .githooks` with committed scripts. No dependency, but you must document the setup step |

```yaml
# lefthook.yml
pre-commit:
  parallel: true
  commands:
    lint:
      glob: "*.{js,ts}"
      run: npx eslint --fix {staged_files}
      stage_fixed: true
    format:
      glob: "*.{json,md,yml}"
      run: npx prettier --write {staged_files}
      stage_fixed: true

commit-msg:
  commands:
    commitlint:
      run: npx commitlint --edit {1}
```

```json
{
  "simple-git-hooks": {
    "pre-commit": "npx lint-staged",
    "commit-msg": "npx --no -- commitlint --edit $1"
  },
  "scripts": { "prepare": "simple-git-hooks" }
}
```

## Other useful hooks

```bash
# .husky/post-merge: reinstall when the lockfile changed
if git diff --name-only ORIG_HEAD HEAD | grep -q "package-lock.json"; then
  echo "package-lock.json changed: running npm ci"
  npm ci
fi
```

```bash
# .husky/pre-commit: block accidental secrets (example with gitleaks, if installed)
gitleaks protect --staged --redact || exit 1
```

## Pitfalls

| Pitfall                                             | Why it hurts                           | Better                                                        |
| --------------------------------------------------- | -------------------------------------- | ------------------------------------------------------------- |
| Slow `pre-commit` (full lint, full tests)           | People bypass it with `--no-verify`    | lint-staged on changed files; slow checks in `pre-push` or CI |
| Treating hooks as the only gate                     | They can be skipped or never installed | Run the same checks in CI                                     |
| No `prepare` script                                 | Teammates never get the hooks          | `"prepare": "husky"`                                          |
| `prepare` failing in production installs            | Husky is a devDependency               | `"prepare": "husky \|\| true"` or `HUSKY=0`                   |
| Whole-project commands receiving file lists (`tsc`) | Ignores `tsconfig.json` or errors      | Function form in lint-staged: `() => 'tsc --noEmit'`          |
| Auto-fixing without re-staging                      | Fixed files miss the commit            | Use lint-staged (it re-adds fixes)                            |
| Overly strict commitlint rules                      | Frustrating, encourages bypass         | Start from `config-conventional` and relax carefully          |
| Hooks that need network or services                 | Flaky, slow commits                    | Keep local hooks offline and quick                            |
| Putting hooks inside `.git/hooks`                   | Not shared with the team               | Husky or `core.hooksPath`                                     |
| Different hook behavior per OS                      | "Works on my machine"                  | Keep scripts POSIX-simple; call npm scripts for logic         |
| Using `--no-verify` as routine                      | Defeats the purpose                    | Fix the failing check, or tune the hook                       |
| Husky in a monorepo subfolder                       | Hooks are not found at the repo root   | Install at the root                                           |

## Key takeaways

- Git hooks catch problems locally in seconds; **Husky** shares them with the team through a committed `.husky/` folder and the `prepare` script
- **lint-staged** runs ESLint and Prettier only on staged files, and re-adds the fixes
- Keep `pre-commit` fast; put slower checks in `pre-push` and CI
- **Conventional Commits** plus **commitlint** give a readable history and enable automated changelogs and versioning
- Hooks are fast feedback, not security: CI is the real gate
- Use `husky || true` (or `HUSKY=0`) in production and CI installs
- Lefthook and simple-git-hooks are good lighter or polyglot alternatives

**Next:** [Linting and Formatting](./04_linting-and-formatting.md)
