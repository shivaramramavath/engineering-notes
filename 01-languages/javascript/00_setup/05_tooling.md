# Modern JavaScript Tooling: Linters, Formatters, Bundlers & Git Hooks

> A comprehensive deep-dive into modern JavaScript developer tooling: ESLint flat configuration, Prettier, Biome, module bundlers, and automated Git hooks for scalable engineering workflows.

| Metadata | Details |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [01_what-is-javascript.md](./01_what-is-javascript.md), [03_npm-and-package-managers.md](./03_npm-and-package-managers.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Runtime / Tooling Baseline** | Node.js v20+ / v22+ LTS, ESLint 9+ (Flat Config), Prettier 3+, Biome 1.8+ |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works: ASTs and Tool Internals](#3-how-it-works-asts-and-tool-internals)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases & Gotchas](#6-edge-cases--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals & AST Parsing](#9-engine-internals--ast-parsing)
10. [Best Practices & Style Guide](#10-best-practices--style-guide)
11. [Real-World Applications & Case Studies](#11-real-world-applications--case-studies)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

JavaScript is dynamically typed, interpreted, and permissive. These attributes accelerate prototyping but easily introduce runtime defects: syntax errors, undeclared variables, variable shadowing, unsafe type coercions, and inconsistent code formatting.

```
Source Code (.js) ──► AST Parser (Espree/Acorn/Biem-CST) ──► Linter Rules (ESLint / Biome)
                                                                       │
                                                       Checks code quality / bug prevention
                                                                       ▼
Source Code (.js) ──► Formatter (Prettier / Biome) ────────► Formatted Consistent Code
                                                                       │
                                                       Checks stylistic indentation/wrapping
                                                                       ▼
Pre-commit (Husky + lint-staged) ─────────────────────────► Validated Git Commit
```

### The Tooling Taxonomy
1. **Linters (ESLint, Biome)**: Analyze code semantics and Abstract Syntax Trees (AST) to detect potential bugs, memory leaks, unreachable code, security risks, and style deviations.
2. **Formatters (Prettier, Biome)**: Reparse source code into an AST and completely reprint it with deterministic whitespace, wrapping, and indentation rules.
3. **Bundlers & Compilers (Vite, esbuild, Rollup, SWC)**: Transform modern ECMAScript/JSX/TypeScript, bundle dependency graphs into browser-ready assets, and provide sub-millisecond Hot Module Replacement (HMR).
4. **Git Automation (Husky, lint-staged)**: Enforce validation at the developer workstation before code is committed or pushed to remote repositories.

Modern ecosystems have migrated from fragmented legacy configurations (`.eslintrc.js`, `.prettierrc`) to streamlined, performant architectures: ESLint Flat Config (`eslint.config.js`), native Rust/Go tooling (Biome, esbuild, SWC), and strict automated quality gates.

---

## 2. Core Concepts

### 2.1 Linting vs. Formatting: Division of Responsibilities
A primary failure mode in engineering teams is conflating **code quality** (linting) with **code presentation** (formatting):
- **Code Quality Rules (Linters)**: `no-unused-vars`, `eqeqeq`, `no-undef`, `no-constant-condition`. These detect logic bugs and runtime failures.
- **Stylistic Rules (Formatters)**: `max-len`, `semi`, `quotes`, `indent`, `comma-dangle`. These govern visual aesthetics and whitespace.
- **Rule of Thumb**: Let formatters own all whitespace, punctuation, and line-wrapping. Turn off all stylistic rules in your linter using `eslint-config-prettier` to avoid conflicting feedback.

### 2.2 ESLint Flat Configuration (`eslint.config.js`)
Beginning with ESLint v9, the flat configuration system replaces legacy cascading `.eslintrc.*` files with a single JavaScript module that exports an array of configuration objects:
- **No Cascading**: Configuration files do not implicitly merge with parent directories.
- **Native ESM/CJS**: Configuration is evaluated as standard JavaScript code; plugins are imported directly as objects rather than resolved via magic strings.
- **Predictable Precedence**: Rules are applied sequentially in array order; later objects override earlier configurations.

### 2.3 Biome: Unified Next-Generation Tooling
Biome (a fork of Rome) provides an all-in-one formatter, linter, and import organizer written in Rust.
- Operates on a single Concrete Syntax Tree (CST), avoiding multi-pass AST reparsing.
- Up to 20x–35x faster than ESLint and Prettier combined.
- Zero-config or single `biome.json` configuration file.

### 2.4 Pre-Commit Hooks and Stage Isolation
Running lint checks across an entire 200,000-line repository on every commit stalls developer flow. `lint-staged` isolates only the files staged in the Git index (`git status`), runs formatting and linting exclusively on those files, re-stages the modifications, and permits the commit.

---

## 3. How It Works: ASTs and Tool Internals

Every modern JavaScript tool processes code through three deterministic phases:

```mermaid
flowchart LR
    A["Source Code String"] -->|Lexical Analysis / Tokenizer| B["Token Stream"]
    B -->|Syntactic Analysis / Parser| C["Abstract Syntax Tree (AST)"]
    C -->|Visitor Pattern / Rule Traversal| D["Diagnostic Errors / Code Fixes"]
    D -->|Code Generation / Printer| E["Formatted / Transpiled Output"]
```

### The AST Representation
Consider the statement: `const x = 42;`. The parser converts this string into an ESTree-compliant AST:

```json
{
  "type": "VariableDeclaration",
  "kind": "const",
  "declarations": [
    {
      "type": "VariableDeclarator",
      "id": { "type": "Identifier", "name": "x" },
      "init": { "type": "Literal", "value": 42, "raw": "42" }
    }
  ]
}
```

### Linter Rule Execution (Visitor Pattern)
When ESLint runs, it traverses the AST using the visitor pattern. Rule plugins define selectors for AST node types:
```javascript
// Example visitor in an ESLint rule
export default {
  create(context) {
    return {
      VariableDeclaration(node) {
        if (node.kind === "var") {
          context.report({
            node,
            message: "Unexpected var, use let or const instead.",
            fix(fixer) {
              return fixer.replaceText(node.range[0], node.range[0] + 3, "const");
            }
          });
        }
      }
    };
  }
};
```

Formatters like Prettier operate differently: they discard all original whitespace, parse the code into an AST retaining comments, and use an algorithm (derived from Wadler's "A prettier printer") to measure line lengths and reconstruct whitespace deterministically.

---

## 4. Syntax & API Reference

### 4.1 ESLint Flat Config (`eslint.config.js`)

```javascript
import js from "@eslint/js";
import globals from "globals";

export default [
  // 1. Global ignores (applies to all files)
  {
    ignores: ["dist/**", "build/**", "node_modules/**", "coverage/**"]
  },

  // 2. Base recommended JavaScript rules
  js.configs.recommended,

  // 3. Custom project-specific configuration
  {
    files: ["**/*.js", "**/*.mjs"],
    languageOptions: {
      ecmaVersion: "latest",
      sourceType: "module",
      globals: {
        ...globals.browser,
        ...globals.node,
        ...globals.es2024
      }
    },
    rules: {
      "no-unused-vars": ["error", { argsIgnorePattern: "^_", varsIgnorePattern: "^_" }],
      "eqeqeq": ["error", "always"],
      "no-console": ["warn", { allow: ["warn", "error"] }],
      "no-implicit-coercion": "error",
      "prefer-const": "error"
    }
  }
];
```

### 4.2 Prettier Configuration (`.prettierrc.json`)

```json
{
  "$schema": "https://json.schemastore.org/prettierrc",
  "semi": true,
  "singleQuote": true,
  "tabWidth": 2,
  "trailingComma": "all",
  "printWidth": 100,
  "bracketSpacing": true,
  "arrowParens": "always",
  "endOfLine": "lf"
}
```

### 4.3 Biome Configuration (`biome.json`)

```json
{
  "$schema": "https://biomejs.dev/schemas/1.8.3/schema.json",
  "vcs": {
    "enabled": true,
    "clientKind": "git",
    "useIgnoreFile": true
  },
  "files": {
    "ignoreUnknown": true,
    "ignore": ["dist/**", "coverage/**"]
  },
  "formatter": {
    "enabled": true,
    "indentStyle": "space",
    "indentWidth": 2,
    "lineWidth": 100
  },
  "linter": {
    "enabled": true,
    "rules": {
      "recommended": true,
      "suspicious": {
        "noDoubleEquals": "error",
        "noExplicitAny": "warn"
      },
      "style": {
        "useConst": "error"
      }
    }
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic ESLint Flat Config + Prettier Integration

```javascript
// eslint.config.js
import js from "@eslint/js";
import eslintConfigPrettier from "eslint-config-prettier";
import globals from "globals";

export default [
  // Recommended core lint rules
  js.configs.recommended,

  // Application settings
  {
    languageOptions: {
      ecmaVersion: 2024,
      sourceType: "module",
      globals: {
        ...globals.nodeBuiltin,
        ...globals.es2024
      }
    },
    rules: {
      "curly": ["error", "all"],
      "no-var": "error",
      "prefer-arrow-callback": "error"
    }
  },

  // Turn off all rules that conflict with Prettier (MUST come last)
  eslintConfigPrettier
];
```

### Example 2: Complete `package.json` Automation Pipeline

```json
{
  "name": "enterprise-tooling-demo",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "lint": "eslint .",
    "lint:fix": "eslint . --fix",
    "format": "prettier --write .",
    "format:check": "prettier --check .",
    "check": "npm run format:check && npm run lint",
    "prepare": "husky"
  },
  "devDependencies": {
    "@eslint/js": "^9.15.0",
    "eslint": "^9.15.0",
    "eslint-config-prettier": "^9.1.0",
    "globals": "^15.12.0",
    "husky": "^9.1.7",
    "lint-staged": "^15.2.10",
    "prettier": "^3.3.3"
  },
  "lint-staged": {
    "*.{js,mjs,cjs}": [
      "prettier --write",
      "eslint --fix"
    ],
    "*.{json,md,yml,yaml}": [
      "prettier --write"
    ]
  }
}
```

### Example 3: Biome Migration and Benchmark Execution

```bash
# Initialize Biome in existing codebase
npx @biomejs/biome init

# Format files in-place
npx @biomejs/biome format --write .

# Run linter and apply safe automated fixes
npx @biomejs/biome lint --write .

# Perform check (lint + format check + import sorting) in a single pass
npx @biomejs/biome check --write .
```

### Example 4: Custom ESLint Rule Plugin (AST Traversal)

```javascript
// eslint-rules/no-raw-throw.js
// Custom rule: disallow throwing raw strings; require throwing Error instances

export const noRawThrowRule = {
  meta: {
    type: "problem",
    docs: {
      description: "Disallow throwing literals or non-Error instances",
      category: "Best Practices"
    },
    schema: [],
    messages: {
      rawThrow: "Do not throw raw literal '{{ value }}'. Always throw a new Error() instance."
    }
  },
  create(context) {
    return {
      ThrowStatement(node) {
        if (node.argument.type === "Literal") {
          context.report({
            node,
            messageId: "rawThrow",
            data: { value: node.argument.raw }
          });
        }
      }
    };
  }
};
```

### Example 5: Husky Pre-Commit Configuration

```bash
# Setup Husky git hooks (run once during repository bootstrap)
npx husky init

# Write pre-commit hook script (.husky/pre-commit)
cat << 'EOF' > .husky/pre-commit
npx lint-staged
EOF

# Make hook executable on POSIX systems
chmod +x .husky/pre-commit
```

---

## 6. Edge Cases & Gotchas

### 6.1 Flat Config Global Ignores vs. Scoped Ignores
In ESLint flat config, an object with **only** an `ignores` key acts as a global ignore:
```javascript
// CORRECT: Global ignore (ignores build output across all configs)
export default [
  { ignores: ["dist/**", "coverage/**"] },
  js.configs.recommended
];

// DANGEROUS: Scoped ignore (only ignores dist/** for the rules inside THIS object)
export default [
  {
    ignores: ["dist/**"],
    rules: { "no-unused-vars": "error" } // dist/** will still be checked by js.configs.recommended!
  },
  js.configs.recommended
];
```

### 6.2 Prettier / ESLint Conflicting Rule Collisions
If `eslint-config-prettier` is omitted, rules like `indent`, `quotes`, and `semi` will cause vicious cycles where Prettier formats a file and ESLint immediately rejects the format:
```bash
# Output from collision loop:
# Step 1: prettier rewrites single quotes to double quotes.
# Step 2: eslint fails with: Error: Strings must use singlequote (quotes)
# Step 3: eslint --fix rewrites back to single quotes.
# Step 4: CI prettier --check fails!
```
**Solution**: Always put `eslintConfigPrettier` as the final entry in `eslint.config.js`.

### 6.3 End of Line Normalization (`CRLF` vs. `LF`)
On Windows hosts, Git can automatically check out files with CRLF (`\r\n`). Prettier and ESLint may emit thousands of line-ending errors (`Delete CR [prettier/prettier]`).
- Configure `.gitattributes`: `* text=auto eol=lf`
- Configure `.prettierrc.json`: `"endOfLine": "lf"`

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **Running whole-repo lint in pre-commit** | Blocks git commit for 30–60 seconds on large repos, frustrating engineers. | Use `lint-staged` to process only modified files. |
| **Using ESLint for stylistic formatting** | ESLint's formatting rules are slow and conflict with modern formatters. | Offload formatting to Prettier/Biome; use ESLint strictly for AST logic. |
| **Ignoring `.gitignore` in linter configs** | Causes linters to crawl minified bundles in `dist/` or `node_modules/`. | Configure `ignores` in flat config or `useIgnoreFile: true` in Biome. |
| **Omitting globals in Flat Config** | `console`, `process`, or `window` flagged as `no-undef`. | Import the `globals` package and merge `globals.node` or `globals.browser`. |
| **Using `--fix` blindly in CI** | Hides bad practices by silently rewriting code during CI builds without committing back. | Run `--check` in CI; restrict `--fix` to local pre-commit hooks. |
| **Mixing legacy `.eslintrc` with `eslint.config.js`** | ESLint v9 defaults to flat config; legacy files cause startup crashes or are ignored. | Migrate fully to `eslint.config.js` or set `ESLINT_USE_FLAT_CONFIG=false`. |

---

## 8. Performance Characteristics

Linter and formatter benchmarks measured across a repository containing 1,500 JavaScript source modules:

| Tool Pipeline | Execution Time (Clean Pass) | AST Passes | Memory Footprint |
| :--- | :--- | :--- | :--- |
| **ESLint + Prettier (Node.js)** | 8.42s | 2 separate passes (Espree AST + Prettier AST) | ~420 MB |
| **ESLint alone (Node.js)** | 4.85s | 1 pass (Espree AST) | ~260 MB |
| **Prettier alone (Node.js)** | 3.57s | 1 pass (Babel-based AST) | ~210 MB |
| **Biome (Rust CST)** | **0.24s** (~35x faster) | Single unified pass (Lossless Concrete Syntax Tree) | **~38 MB** |

### Why Biome is Significantly Faster:
1. **Single-Pass CST**: Biome parses the source file once into a Green/Red Concrete Syntax Tree that retains tokens, comments, and whitespace. Formatting, linting, and import sorting inspect the same in-memory tree.
2. **Native Concurrency**: Written in Rust using Rayon, Biome saturates all available CPU cores without Node.js IPC overhead.

---

## 9. Engine Internals & AST Parsing

Linters and formatters do not interpret JavaScript; they tokenize and structure it.

```
Source: "let total = price * 1.08;"
Tokens: [KEYWORD(let), IDENTIFIER(total), OP(=), IDENTIFIER(price), OP(*), NUMBER(1.08), PUNCT(;)]
```

### Acorn, Espree, and ESTree
- **ESTree**: The standardized JSON specification for representing ECMAScript source code.
- **Espree**: The default parser inside ESLint (built on top of Acorn). It maps token ranges `[start, end]` so fixers know the exact byte offsets to rewrite without altering the rest of the file.
- **Scope Analysis (`eslint-scope`)**: ESLint builds a tree of lexical scopes (`GlobalScope` -> `FunctionScope` -> `BlockScope`). When it encounters an identifier, it walks the scope chain upwards to verify variable declaration, enabling rules like `no-undef` and `no-shadow`.

---

## 10. Best Practices & Style Guide

1. **Keep Configurations Minimal**: Start with `@eslint/js.configs.recommended` and enable additional rules only when justified by team post-mortems.
2. **Zero Warnings Policy in CI**: Treat warnings as errors in continuous integration:
   ```bash
   eslint . --max-warnings=0
   ```
3. **Automate Formatting On Save**: Configure your IDE (`settings.json`) to execute Prettier or Biome on save. Developers should never manually adjust indentation or line breaks.
4. **Enforce Conventional Commits**: Combine Husky with `@commitlint/config-conventional` to enforce clean git logs (`feat:`, `fix:`, `refactor:`, `chore:`).

---

## 11. Real-World Applications & Case Studies

### Case Study: Monorepo Linter Optimization
A fintech engineering team with 1.2 million lines of code in a monorepo faced 4-minute pre-push lint delays.
1. **Root Cause**: ESLint was traversing deep directory trees, repeatedly evaluating un-cached external libraries.
2. **Remediation**:
   - Replaced custom script loops with `lint-staged` targeting only changed files.
   - Migrated code formatting from Prettier to Biome, dropping CI formatting checks from 92 seconds to 3.1 seconds.
   - Enabled ESLint caching (`eslint --cache --cache-location .eslintcache`).
3. **Result**: Developer feedback loop dropped from 240 seconds to under 2 seconds.

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. Explain the architectural difference between ESLint Flat Config and the legacy .eslintrc system.</strong></summary>

**Answer:**
Legacy `.eslintrc` used an implicit, hierarchical, cascading inheritance model where configuration files in child directories merged with parent directories based on file system paths. Plugins were resolved via magic string names (`plugins: ["react"]`), leading to unpredictable plugin resolution in monorepos.

Flat Config (`eslint.config.js`) exports an array of JavaScript configuration objects evaluated sequentially. There is no cascading inheritance: rules in later array items explicitly override earlier ones. Plugins are imported directly as standard JavaScript objects (`import react from "eslint-plugin-react"`), eliminating string-based resolution bugs and improving execution speed.
</details>

<details>
<summary><strong>2. Why should Prettier and ESLint formatting rules not be run simultaneously?</strong></summary>

**Answer:**
Both tools operate with different AST formatting engines. ESLint's formatting rules (e.g., `indent`, `quotes`, `semi`) apply local syntax fixes, whereas Prettier reparses the entire AST and re-prints the code from scratch according to strict line-width constraints. Running both simultaneously leads to contradictory updates and infinite reformatting loops. The modern standard is to use `eslint-config-prettier` to disable all ESLint stylistic rules and delegate 100% of formatting to Prettier.
</details>

<details>
<summary><strong>3. What is an Abstract Syntax Tree (AST) and how does a linter use it?</strong></summary>

**Answer:**
An AST is a hierarchical, tree-structured syntactic representation of source code produced by a parser. Nodes in the tree represent language constructs such as `FunctionDeclaration`, `BinaryExpression`, or `IfStatement`. Linters traverse this tree using the visitor pattern. When the traversal encounters a matching node type, the linter rule inspects the node's properties, scopes, or child nodes to detect anti-patterns and report errors or generate programmatic code fixes.
</details>

<details>
<summary><strong>4. How does `lint-staged` prevent uncommitted local changes from slipping into a commit?</strong></summary>

**Answer:**
`lint-staged` interacts directly with the Git index. When a developer stages files (`git add`) and triggers `git commit`, `lint-staged`:
1. Stashes unstaged modifications to isolate purely the staged changes.
2. Runs the configured tasks (formatting, linting) on the staged files.
3. Automatically re-stages the files if fixes were applied (`git add`).
4. Restores the unstaged modifications back to the working tree.
</details>

<details>
<summary><strong>5. What is the difference between an AST and a Concrete Syntax Tree (CST) as used in Biome?</strong></summary>

**Answer:**
An AST simplifies the syntax by discarding non-semantic elements such as punctuation, comments, and whitespace. A Concrete Syntax Tree (CST) represents 100% of the concrete text in the source code, including trivia (comments, spaces, newlines). Biome uses a lossless CST, allowing it to perform linting, import sorting, and code formatting on a single tree structure in memory without discarding or re-synthesizing layout information.
</details>

<details>
<summary><strong>6. How does ESLint handle variable scoping during rule execution?</strong></summary>

**Answer:**
ESLint uses `eslint-scope`, which implements the ECMAScript specification for lexical scoping. As the parser constructs the AST, `eslint-scope` builds a corresponding hierarchy of `Scope` objects (`GlobalScope`, `ModuleScope`, `FunctionScope`, `BlockScope`). Each scope tracks identifier references and variable definitions. Rules querying whether a variable is unused or undeclared (`no-unused-vars`, `no-undef`) query the scope manager rather than manually walking AST parents.
</details>

<details>
<summary><strong>7. What happens when ESLint runs with `--fix`?</strong></summary>

**Answer:**
Rules can optionally provide a `fix` function inside their diagnostic reports. When `--fix` is passed, ESLint runs an iterative repair loop (up to 10 passes by default). In each pass, it applies non-overlapping byte ranges from `RuleFixer` (such as `replaceText`, `insertTextAfter`, `remove`). After applying fixes, ESLint re-parses the file to verify that new syntax errors were not introduced and applies subsequent rule fixes until convergence or maximum iterations.
</details>

<details>
<summary><strong>8. Why is esbuild or SWC faster than Babel for transpilation?</strong></summary>

**Answer:**
Babel is written in JavaScript and runs on the V8 engine, which incurs garbage collection overhead, AST object allocation overhead, and single-threaded execution limitations. `esbuild` is written in Go and `SWC` is written in Rust. Both compile down to native machine code, manage memory directly without V8 garbage collection pauses, use parallel data structures across all CPU cores, and minimize AST transformations through single-pass compilation.
</details>

---

## 13. Exercises & Challenges

### Exercise 1 (Easy): Create a Zero-Conflict ESLint Flat Configuration
**Task**: Create an `eslint.config.js` that enables recommended JavaScript rules, allows `console.warn` and `console.error`, and integrates `eslint-config-prettier` to prevent formatting conflicts.

<details>
<summary><strong>Solution</strong></summary>

```javascript
// eslint.config.js
import js from "@eslint/js";
import eslintConfigPrettier from "eslint-config-prettier";
import globals from "globals";

export default [
  {
    ignores: ["node_modules/", "dist/"]
  },
  js.configs.recommended,
  {
    languageOptions: {
      ecmaVersion: 2024,
      sourceType: "module",
      globals: {
        ...globals.browser,
        ...globals.node
      }
    },
    rules: {
      "no-console": ["warn", { allow: ["warn", "error"] }],
      "prefer-const": "error",
      "no-var": "error"
    }
  },
  // Must be placed last to override any formatting rules from recommended config
  eslintConfigPrettier
];
```
</details>

### Exercise 2 (Easy): Configure Prettier with Strict Single-Quote and Trailing Commas
**Task**: Author a `.prettierrc.json` file specifying single quotes, 2-space indentation, trailing commas for all multi-line objects/arrays, and LF line endings.

<details>
<summary><strong>Solution</strong></summary>

```json
{
  "$schema": "https://json.schemastore.org/prettierrc",
  "singleQuote": true,
  "tabWidth": 2,
  "trailingComma": "all",
  "endOfLine": "lf",
  "semi": true
}
```
</details>

### Exercise 3 (Medium): Configure `lint-staged` with Dual Workflows
**Task**: Create a `lint-staged` configuration inside `package.json` that formats markdown/JSON files with Prettier, and runs both Prettier and ESLint (with `--fix`) on JavaScript files.

<details>
<summary><strong>Solution</strong></summary>

```json
{
  "lint-staged": {
    "*.{json,md}": [
      "prettier --write"
    ],
    "*.{js,mjs}": [
      "prettier --write",
      "eslint --fix"
    ]
  }
}
```
</details>

### Exercise 4 (Medium): Custom ESLint Rule to Ban `eval()` and `new Function()`
**Task**: Write an in-repo custom ESLint rule that flags any invocation of `eval(...)` or `new Function(...)` as a critical security vulnerability.

<details>
<summary><strong>Solution</strong></summary>

```javascript
// rules/no-dynamic-eval.js
export const noDynamicEvalRule = {
  meta: {
    type: "problem",
    docs: {
      description: "Prevent dynamic code execution via eval or Function constructor",
      category: "Security"
    },
    messages: {
      banEval: "Direct call to eval() is strictly forbidden due to arbitrary code execution risks.",
      banFunction: "Using new Function(...) creates insecure dynamic code evaluation."
    }
  },
  create(context) {
    return {
      CallExpression(node) {
        if (node.callee.type === "Identifier" && node.callee.name === "eval") {
          context.report({ node, messageId: "banEval" });
        }
      },
      NewExpression(node) {
        if (node.callee.type === "Identifier" && node.callee.name === "Function") {
          context.report({ node, messageId: "banFunction" });
        }
      }
    };
  }
};
```
</details>

### Exercise 5 (Hard): Complete Biome Configuration with Custom Lint Overrides
**Task**: Create a production-ready `biome.json` that enables Git integration, sets a print width of 100 characters, disables `noExplicitAny` in test directories (`tests/**`), and treats console logs as errors.

<details>
<summary><strong>Solution</strong></summary>

```json
{
  "$schema": "https://biomejs.dev/schemas/1.8.3/schema.json",
  "vcs": {
    "enabled": true,
    "clientKind": "git",
    "useIgnoreFile": true
  },
  "formatter": {
    "enabled": true,
    "indentStyle": "space",
    "indentWidth": 2,
    "lineWidth": 100
  },
  "linter": {
    "enabled": true,
    "rules": {
      "recommended": true,
      "suspicious": {
        "noConsoleLog": "error"
      }
    }
  },
  "overrides": [
    {
      "include": ["tests/**", "**/*.test.js"],
      "linter": {
        "rules": {
          "suspicious": {
            "noConsoleLog": "off",
            "noExplicitAny": "off"
          }
        }
      }
    }
  ]
}
```
</details>

### Exercise 6 (Hard): Build an Automated CI Verification Script
**Task**: Write a Node.js CLI script (`verify-ci.js`) that programmatically invokes Prettier check, ESLint with zero-warnings enforcement, and git clean check, outputting clear exit codes and structured timing metrics.

<details>
<summary><strong>Solution</strong></summary>

```javascript
// scripts/verify-ci.js
import { execSync } from "node:child_process";
import { performance } from "node:perf_hooks";

function runStep(name, command) {
  const start = performance.now();
  process.stdout.write(`[CI CHECK] Running ${name}... `);
  try {
    execSync(command, { stdio: "pipe" });
    const duration = ((performance.now() - start) / 1000).toFixed(2);
    console.log(`\x1b[32mPASSED\x1b[0m (${duration}s)`);
  } catch (error) {
    console.log(`\x1b[31mFAILED\x1b[0m`);
    console.error(`\nError output from ${name}:\n`);
    console.error(error.stdout?.toString() || error.message);
    process.exit(1);
  }
}

console.log("=== Continuous Integration Quality Gate ===\n");
runStep("Prettier Formatting Check", "npx prettier --check .");
runStep("ESLint Analysis (Zero Warnings)", "npx eslint . --max-warnings=0");
runStep("Git Working Tree Cleanliness", "git diff --exit-code");

console.log("\n\x1b[32mAll quality checks passed successfully!\x1b[0m");
process.exit(0);
```
</details>
