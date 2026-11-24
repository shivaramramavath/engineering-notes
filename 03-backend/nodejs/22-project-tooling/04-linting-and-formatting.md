# Linting and Formatting

Two different jobs, often confused:

| | **Linting** (ESLint) | **Formatting** (Prettier) |
|---|----------------------|---------------------------|
| Question | "Is this code likely **wrong** or risky?" | "Is this code **laid out** consistently?" |
| Finds | Unused variables, missing `await`, accidental globals, unreachable code | Indentation, quotes, line width, trailing commas, semicolons |
| Fixes | Some rules auto-fix; many need a human decision | Everything, automatically and deterministically |
| Opinionated about | Correctness and best practice | Appearance only |

Use **ESLint for bugs** and **Prettier for style**, and keep them from fighting each other. Add **EditorConfig** so every editor agrees on basics, and run all of it automatically on save, on commit ([Git Hooks](./03_git-hooks-and-commits.md)), and in CI.

See also: [Tooling](../00_setup/05_tooling.md), [Project Structure](./01_project-structure.md), [Testing Fundamentals](../21_testing/01_testing-fundamentals.md).

## ESLint

```bash
npm install --save-dev eslint @eslint/js globals
npm init @eslint/config@latest       # interactive setup, or write the file by hand
```

### Flat config (`eslint.config.js`)

ESLint 9 and later use the **flat config** format by default: a single JavaScript file exporting an array of config objects. (The older `.eslintrc.*` format is legacy.)

```js
// eslint.config.js
import js from '@eslint/js';
import globals from 'globals';

export default [
  // 1. Files and folders to skip entirely
  { ignores: ['dist/', 'build/', 'coverage/', 'node_modules/'] },

  // 2. A maintained baseline of recommended rules
  js.configs.recommended,

  // 3. Your project settings
  {
    files: ['**/*.js', '**/*.mjs'],
    languageOptions: {
      ecmaVersion: 'latest',
      sourceType: 'module',
      globals: { ...globals.node },          // process, Buffer, __dirname (CJS), etc.
    },
    rules: {
      'no-unused-vars': ['error', { argsIgnorePattern: '^_' }],
      eqeqeq: ['error', 'always'],
      'prefer-const': 'error',
      'no-console': 'warn',
      'no-var': 'error',
      curly: ['error', 'multi-line'],
    },
  },

  // 4. Different rules for different files
  {
    files: ['**/*.test.js', 'test/**'],
    languageOptions: { globals: { ...globals.node } },
    rules: { 'no-console': 'off' },
  },

  // 5. Browser code needs browser globals
  {
    files: ['src/client/**/*.js'],
    languageOptions: { globals: { ...globals.browser } },
  },
];
```

Config objects are applied **in order**; later ones override earlier ones for the files they match.

### Rule severity

| Value | Meaning |
|-------|---------|
| `'off'` or `0` | Disabled |
| `'warn'` or `1` | Reported, does not fail the run |
| `'error'` or `2` | Reported, exit code 1 |

Treat warnings as noise unless you enforce them with `--max-warnings=0`. A rule is either worth failing on or worth removing.

### Running ESLint

```bash
npx eslint .                         # lint the project
npx eslint . --fix                   # apply safe automatic fixes
npx eslint src --max-warnings=0      # fail on any warning (use in CI and hooks)
npx eslint . --cache                 # skip unchanged files on re-runs
npx eslint --inspect-config          # visual config inspector (newer versions)
npx eslint --report-unused-disable-directives .
```

### Rules worth turning on

| Rule | Catches |
|------|---------|
| `eqeqeq` | `==` coercion surprises ([Type Conversion](../01_fundamentals/03_type-conversion-and-equality.md)) |
| `no-unused-vars` | Dead code, typos in names |
| `prefer-const` | Variables that never change |
| `no-var` | Function-scoped `var` |
| `no-undef` | Use of undeclared variables |
| `no-shadow` | Inner variable hiding an outer one |
| `no-throw-literal` / `@typescript-eslint/only-throw-error` | Throwing non-`Error` values |
| `require-await` | `async` functions that never `await` |
| `no-return-await` / `no-await-in-loop` | Needless or serial awaits (decide per case) |
| `no-promise-executor-return` | Returning values from `new Promise(...)` executors |
| `no-restricted-syntax` / `no-restricted-imports` | Enforce your own conventions |

**Floating promises** (a promise started and never awaited or handled) are a top source of async bugs. In TypeScript projects, `@typescript-eslint/no-floating-promises` catches them. For plain JavaScript, `eslint-plugin-promise` helps with related mistakes.

### Plugins and shared configs

| Plugin | Purpose |
|--------|---------|
| `typescript-eslint` | TypeScript parser and rules |
| `eslint-plugin-n` | Node.js rules (`n/no-process-env`, `n/no-missing-import`, deprecated API checks) |
| `eslint-plugin-import` / `eslint-plugin-import-x` | Import order, unresolved or duplicate imports, cycles |
| `eslint-plugin-unicorn` | Many modern-JavaScript best-practice rules (including `filename-case`) |
| `eslint-plugin-promise` | Promise best practices |
| `eslint-plugin-security` | Flags some risky patterns (use with realistic expectations) |
| `eslint-plugin-vitest` / `eslint-plugin-jest` | Test-specific rules (`no-focused-tests`, `no-disabled-tests`) |
| `eslint-plugin-react`, `react-hooks`, `jsx-a11y` | React projects |
| `eslint-plugin-vue`, `eslint-plugin-svelte` | Other frameworks |
| `eslint-config-prettier` | **Turns off** rules that conflict with Prettier |

Enforce the "single env module" convention from [Environment Variables](./02_environment-variables.md):

```js
import nodePlugin from 'eslint-plugin-n';

export default [
  nodePlugin.configs['flat/recommended'],
  { rules: { 'n/no-process-env': 'error' } },
  { files: ['src/config/**'], rules: { 'n/no-process-env': 'off' } },   // the one place that may read it
];
```

### TypeScript

```bash
npm install --save-dev typescript typescript-eslint
```

```js
import js from '@eslint/js';
import tseslint from 'typescript-eslint';

export default tseslint.config(
  { ignores: ['dist/'] },
  js.configs.recommended,
  ...tseslint.configs.recommended,          // or recommendedTypeChecked for type-aware rules (slower, stronger)
  {
    rules: {
      '@typescript-eslint/no-unused-vars': ['error', { argsIgnorePattern: '^_' }],
      '@typescript-eslint/consistent-type-imports': 'error',
    },
  },
);
```

ESLint does not type-check. Run the compiler separately: `tsc --noEmit` (the `typecheck` script).

### Disabling rules responsibly

```js
// eslint-disable-next-line no-console -- startup banner is intentional
console.log('Server ready');
```

- Prefer the narrowest scope (`-next-line`) over disabling a whole file
- Always add a reason after `--`
- Use `--report-unused-disable-directives` so stale disables get cleaned up
- If you disable the same rule everywhere, turn the rule off in the config instead

## Prettier

An **opinionated formatter**: it parses your code and prints it again in a consistent style. You stop debating style in code review.

```bash
npm install --save-dev --save-exact prettier
```

```json
// .prettierrc
{
  "semi": true,
  "singleQuote": true,
  "trailingComma": "all",
  "printWidth": 100,
  "tabWidth": 2,
  "arrowParens": "always"
}
```

```gitignore
# .prettierignore
dist
build
coverage
package-lock.json
pnpm-lock.yaml
*.min.js
```

```bash
npx prettier --write .          # format everything
npx prettier --check .          # report unformatted files (use in CI; exits non-zero)
npx prettier --write "src/**/*.{js,ts,json,md}"
```

Prettier has **few options on purpose**. The most debated ones (semicolons, quotes, width) are set once in `.prettierrc`; do not tune every detail. It also formats JSON, Markdown, CSS, HTML, YAML, and GraphQL.

Pin Prettier's exact version (`--save-exact`): new releases occasionally change output, which would create noisy formatting diffs across the codebase.

Per-file overrides:

```json
{
  "singleQuote": true,
  "overrides": [
    { "files": "*.md", "options": { "proseWrap": "always", "printWidth": 80 } }
  ]
}
```

### Making ESLint and Prettier cooperate

ESLint's stylistic rules can contradict Prettier's output. Disable the conflicting ones with `eslint-config-prettier`, **placed last** in the config array:

```bash
npm install --save-dev eslint-config-prettier
```

```js
import js from '@eslint/js';
import prettier from 'eslint-config-prettier';

export default [
  js.configs.recommended,
  { /* your rules */ },
  prettier,                      // last: switches off formatting-related rules
];
```

Run them as **separate steps** (`prettier --write` then `eslint --fix`). Avoid running Prettier **inside** ESLint (`eslint-plugin-prettier`), which is slower and reports style problems as lint errors.

## EditorConfig

A tiny, editor-independent file that sets basics before any formatter runs. Most editors support it natively or via a plugin.

```ini
# .editorconfig
root = true

[*]
charset = utf-8
end_of_line = lf
indent_style = space
indent_size = 2
insert_final_newline = true
trim_trailing_whitespace = true

[*.md]
trim_trailing_whitespace = false

[Makefile]
indent_style = tab
```

`end_of_line = lf` prevents Windows and Unix line-ending churn; combine it with `* text=auto eol=lf` in `.gitattributes` if your team mixes platforms. Prettier reads `.editorconfig` for indentation settings.

## Editor integration

Make the right thing automatic:

```json
// .vscode/settings.json
{
  "editor.formatOnSave": true,
  "editor.defaultFormatter": "esbenp.prettier-vscode",
  "editor.codeActionsOnSave": { "source.fixAll.eslint": "explicit" },
  "eslint.useFlatConfig": true,
  "files.eol": "\n"
}
```

```json
// .vscode/extensions.json: suggested extensions for anyone opening the repo
{
  "recommendations": [
    "dbaeumer.vscode-eslint",
    "esbenp.prettier-vscode",
    "editorconfig.editorconfig"
  ]
}
```

Commit these two files so the whole team gets the same experience.

## Biome: one tool instead of two

**Biome** is a fast, single-binary linter **and** formatter (written in Rust) for JavaScript, TypeScript, JSON, and CSS, with Prettier-like formatting.

```bash
npm install --save-dev --save-exact @biomejs/biome
npx biome init
```

```json
// biome.json
{
  "formatter": { "indentStyle": "space", "indentWidth": 2, "lineWidth": 100 },
  "javascript": { "formatter": { "quoteStyle": "single" } },
  "linter": { "enabled": true, "rules": { "recommended": true } }
}
```

```bash
npx biome check .            # lint + format check
npx biome check --write .    # apply fixes and formatting
```

| | ESLint + Prettier | Biome |
|---|-------------------|-------|
| Speed | Good | Much faster |
| Ecosystem and plugins | Huge | Smaller, growing |
| Framework coverage (Vue, Svelte, custom rules) | Excellent | Partial: check current support |
| Setup | Two tools, two configs | One tool, one config |
| Best for | Mature projects needing plugins | New projects wanting speed and simplicity |

Both are valid. Choose by the rules and plugins you need.

## Scripts and CI

```json
{
  "scripts": {
    "lint": "eslint .",
    "lint:fix": "eslint . --fix",
    "format": "prettier --write .",
    "format:check": "prettier --check .",
    "typecheck": "tsc --noEmit"
  }
}
```

```yaml
# CI: fail the build on any violation
- run: npm run format:check
- run: npm run lint -- --max-warnings=0
- run: npm run typecheck
```

The layers fit together like this:

| When | What runs | Goal |
|------|-----------|------|
| While typing | Editor integration | Instant feedback |
| On save | Prettier, ESLint fixes | Zero manual formatting |
| On commit | lint-staged (ESLint + Prettier on staged files) | Nothing sloppy enters history |
| In CI | `format:check`, `lint`, `typecheck`, tests | The enforced gate |

## Adopting linting in an existing codebase

1. Add Prettier first; run `prettier --write .` in **one dedicated commit** and note it in `.git-blame-ignore-revs` so `git blame` skips it
2. Add ESLint with the `recommended` set only
3. Fix errors, or temporarily downgrade noisy rules to `warn` and track them
4. Tighten gradually (one rule at a time), rather than enabling everything at once
5. Enforce in CI once the baseline is clean

```bash
# .git-blame-ignore-revs
# format entire codebase with prettier
a1b2c3d4e5f6...

git config blame.ignoreRevsFile .git-blame-ignore-revs
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Using ESLint rules for formatting and Prettier | They fight and rewrite each other | `eslint-config-prettier` last in the config; one formatter |
| Running Prettier inside ESLint (`eslint-plugin-prettier`) | Slow; style noise in lint output | Run them as separate steps |
| Warnings nobody reads | They pile up and hide real problems | `--max-warnings=0`, or remove the rule |
| Disabling rules file-wide without a reason | Hides real bugs | `-next-line` with a `--` explanation |
| Enabling every rule at once on an old codebase | Thousands of errors, nobody fixes any | Incremental adoption |
| Not pinning Prettier's version | Formatting churn on upgrades | `--save-exact` and update deliberately |
| Mixing old `.eslintrc` and new flat config | Confusing, ignored settings | Migrate fully to `eslint.config.js` |
| Forgetting `ignores` (linting `dist/` or `coverage/`) | Slow runs and false errors | Ignore generated folders |
| Wrong globals (`process` flagged as undefined) | False `no-undef` errors | Set `globals.node` or `globals.browser` per file group |
| Line-ending differences across OSes | Whole-file diffs | `.editorconfig`, `.gitattributes`, `end_of_line = lf` |
| Linting only locally | Rules silently skipped on other machines | The same commands in CI |
| Treating lint passing as "tests passing" | Lint finds suspicious patterns, not behavior errors | Both lint and tests |

## Key takeaways

- **ESLint finds problems; Prettier formats.** Keep them in separate roles and use `eslint-config-prettier` so they never conflict
- ESLint 9+ uses **flat config** (`eslint.config.js`): an ordered array of config objects with `ignores`, `files`, `languageOptions`, and `rules`
- Start from `js.configs.recommended`, add only rules you will enforce, and run with `--max-warnings=0`
- Prettier needs minimal configuration; pin its version and commit `.prettierrc` and `.prettierignore`
- **EditorConfig** plus shared `.vscode` settings give everyone the same baseline
- Automate in layers: editor on save → lint-staged on commit → CI as the gate
- Biome is a fast all-in-one alternative; choose based on the plugins you need
- Adopt on legacy code incrementally and keep a `.git-blame-ignore-revs` for the big formatting commit

**Next:** [Validation Libraries](./05_validation-libraries.md)