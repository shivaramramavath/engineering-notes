# Tooling: Stylelint, Prettier & Lightning CSS

| Metadata | Details |
|:---|:---|
| **Level** | Foundational to Intermediate |
| **Prerequisites** | `00-setup/01-what-is-css-and-the-cascade-idea.md`, `00-setup/03-ways-to-add-css.md` |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (Modern Toolchain Ecosystem 2024–2026) |
| **Browsers Tested** | Node.js 20+, Rust Lightning CSS CLI, Bun, Vite 5+ |
| **Tested With** | Stylelint 16+, Prettier 3+, Lightning CSS 1.25+, Playwright css_runner.js |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility & Browser Support](#8-performance-accessibility--browser-support)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is Modern CSS Tooling?
Modern CSS engineering relies on an automated toolchain divided into three distinct responsibilities:
1. **Linting (Stylelint)**: Static analysis that enforces code consistency, forbids invalid or deprecated CSS properties, catches logical anti-patterns, and flags accessibility violations before code reaches production.
2. **Formatting (Prettier / Biome)**: Automated, opinionated code formatting that standardizes indentation, quotes, semicolon placement, and line wraps across large teams.
3. **Compilation, Bundling & Minification (Lightning CSS / PostCSS)**: Transforming cutting-edge CSS syntax (such as CSS Nesting, modern color functions, and custom media queries) into backward-compatible CSS, bundling `@import` trees, and minifying output with maximum compression.

### Why Did the Toolchain Shift from Sass to Lightning CSS?
For a decade (2010–2020), CSS preprocessors like Sass/SCSS and PostCSS were mandatory because native CSS lacked variables, nesting, and modular imports.

Today, **native CSS possesses all of these capabilities natively**:
- Native CSS Custom Properties (`var(--token)`) outclass static Sass variables by reacting dynamically to DOM state and media queries.
- Native CSS Nesting (`& .child`) is standardized across all major browsers.
- Native CSS Color Functions (`color-mix()`, `oklch()`) replace preprocessor math.

The modern toolchain no longer invents proprietary syntax; instead, it compiles **standard future CSS** to **current browser CSS**. Written in Rust, **Lightning CSS** processes stylesheets over 100x faster than legacy JavaScript-based PostCSS plugins.

---

## 2. Core Concepts

### 1. Stylelint: The Authoritative CSS Linter
Stylelint parses CSS into an Abstract Syntax Tree (AST) to enforce over 170 built-in rules:
- **Catches Errors**: Detects unrecognized property names (e.g., `colr: red`), invalid hex codes (`#fff1z`), and duplicate selectors.
- **Enforces Conventions**: Mandates lowercase hex colors, prohibits vendor prefixes that modern browsers don't need, and enforces alphabetical or logical property ordering.
- **Enforces Modern Best Practices**: Warns against using `!important`, blocks universal selector abuses, and prevents accidental font-family overrides.

### 2. Prettier: Deterministic Code Formatting
Prettier eliminates debates over formatting styles:
- Ensures uniform single/double quotes, 2-space indentation, consistent spacing inside brackets, and clean multiline property wrapping.
- *Crucial Separation*: Prettier handles *formatting* (whitespace, quotes); Stylelint handles *code quality and correctness* (preventing errors). Use `stylelint-config-prettier` to disable formatting rules in Stylelint to avoid conflicts.

### 3. PostCSS: The JavaScript-Based AST Ecosystem
PostCSS is a tool for transforming CSS with JavaScript plugins:
- Parses CSS into an AST, allows plugins to inspect and manipulate nodes, then stringifies back into CSS.
- Famous plugins: **Autoprefixer** (adds vendor prefixes based on CanIUse data), **postcss-preset-env** (polyfills modern CSS specs).
- *Limitation*: Heavy memory usage and slower build times on massive codebases due to Node.js single-threaded event loop overhead.

### 4. Lightning CSS: The High-Speed Rust Transformer
Created by the author of Parcel, Lightning CSS is an extremely fast, Rust-based CSS parser, transformer, bundler, and minifier:
- Replaces PostCSS, Autoprefixer, and cssnano in a single native binary.
- Transpiles modern CSS (Nesting, `@custom-media`, CSS Color 4, CSS Modules) to target older browsers based on a `browserslist` query.
- Emits heavily optimized, byte-minified CSS with AST-level deduplication.

### 5. Browserslist: The Unified Target Configuration
A shared configuration file (`.browserslistrc` or `"browserslist"` key in `package.json`) read by Lightning CSS, Autoprefixer, and Babel:
```ini
# Production browser support targets
> 0.5%
last 2 versions
Firefox ESR
not dead
```

---

## 3. How It Works Internally

### The Parser, AST & Visitor Pattern

```
+--------------------------------------------------------------------+
| SOURCE CSS TEXT                                                    |
| .card { color: oklch(0.7 0.2 150); & .title { font-weight: 700; } }|
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| LEXICAL ANALYZER & TOKENIZER (Stream of Tokens)                    |
| IDENT(".card"), LBRACE("{"), IDENT("color"), COLON(":"), ...       |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| PARSER -> ABSTRACT SYNTAX TREE (AST)                               |
| RuleNode {                                                         |
|   selector: ".card",                                               |
|   declarations: [ Declaration { prop: "color", value: "oklch..." }],|
|   children: [ NestedRule { selector: "& .title", ... } ]           |
| }                                                                  |
+--------------------------------------------------------------------+
         |                                           |
         v                                           v
+-------------------------------+       +----------------------------+
| STYLELINT (Static Analysis)   |       | LIGHTNING CSS (Transpile)  |
| AST Visitor checks rules:     |       | Downlevels oklch to sRGB;  |
| - Property recognized?        |       | Flattens nesting for older |
| - Units valid?                |       | targets; minifies output   |
+-------------------------------+       +----------------------------+
                                                     |
                                                     v
+--------------------------------------------------------------------+
| EMITTED PRODUCTION CSS                                             |
| .card{color:#00b25e}.card .title{font-weight:700}                  |
+--------------------------------------------------------------------+
```

1. **Tokenization**: The raw string is broken into lexical tokens (whitespace, comments, identifiers, punctuation).
2. **AST Construction**: Tokens are assembled into a hierarchical tree of rules, declarations, media queries, and at-rules.
3. **AST Transformation & Downleveling**:
   - If targeting browsers without native CSS Nesting, Lightning CSS automatically flattens `.card { & .title { ... } }` into `.card .title { ... }`.
   - If targeting browsers without `oklch()` support, it calculates the closest mathematical sRGB fallback hex code.
4. **Code Generation & Minification**: Declarations are sorted, duplicate rules are merged, numbers like `0.5rem` are compressed to `.5rem`, and whitespace is stripped.

---

## 4. Syntax & Usage

### 1. Stylelint Configuration (`.stylelintrc.json`)
```json
{
  "extends": [
    "stylelint-config-standard"
  ],
  "rules": {
    "selector-class-pattern": "^[a-z]([a-z0-9-]+)?(__[a-z0-9-]+)?(--[a-z0-9-]+)?$",
    "color-named": "never",
    "declaration-no-important": true,
    "max-nesting-depth": 3,
    "no-descending-specificity": true,
    "unit-allowed-list": ["px", "rem", "em", "%", "vw", "vh", "svh", "dvh", "fr", "deg", "ms", "s"]
  }
}
```

### 2. Prettier Configuration (`.prettierrc.json`)
```json
{
  "tabWidth": 2,
  "useTabs": false,
  "singleQuote": false,
  "trailingComma": "es5",
  "printWidth": 100,
  "overrides": [
    {
      "files": "*.css",
      "options": {
        "singleQuote": false
      }
    }
  ]
}
```

### 3. Lightning CSS CLI & Node API
Run via CLI:
```bash
# Transpile and bundle modern CSS targeting specific browsers
npx lightningcss --bundle --targets ">= 0.5%, not dead" src/main.css -o dist/main.min.css --minify
```

Run via JavaScript API:
```javascript
import { bundle } from "lightningcss";
import * as fs from "node:fs";

const { code, map } = bundle({
  filename: "src/main.css",
  minify: true,
  sourceMap: true,
  targets: {
    chrome: 110 << 16,
    firefox: 115 << 16,
    safari: 16 << 16
  }
});

fs.writeFileSync("dist/main.min.css", code);
```

---

## 5. Practical Examples

### Example 1: Basic Modern CSS Input
```css
/* src/index.css */
:root {
  --primary-color: oklch(0.6 0.22 250);
}

.button-group {
  display: flex;
  gap: 1rem;

  & .btn {
    background-color: var(--primary-color);
    padding: 0.5rem 1rem;
    border-radius: 6px;
    border: none;
    color: white;

    &:hover {
      background-color: color-mix(in oklch, var(--primary-color), black 15%);
    }
  }
}
```

### Example 2: Lightning CSS Build Pipeline with Bundle & Minification
```javascript
// build-css.mjs
import { bundle } from 'lightningcss';
import path from 'node:path';
import fs from 'node:fs';

const inputPath = path.resolve('src/index.css');
const outputPath = path.resolve('dist/index.min.css');

const result = bundle({
  filename: inputPath,
  minify: true,
  targets: {
    chrome: 105 << 16, // Chrome 105 lacks native nesting, so Lightning CSS flattens it!
  }
});

fs.mkdirSync(path.dirname(outputPath), { recursive: true });
fs.writeFileSync(outputPath, result.code);
console.log(`Compiled CSS successfully (${result.code.length} bytes).`);
```

*Output Generated by Lightning CSS for Chrome 105 Target*:
```css
:root{--primary-color:oklch(.6 .22 250)}.button-group{display:flex;gap:1rem}.button-group .btn{background-color:var(--primary-color);color:#fff;border:none;border-radius:6px;padding:.5rem 1rem}.button-group .btn:hover{background-color:color-mix(in oklch,var(--primary-color),#000 15%)}
```
*Notice*: Lightning CSS automatically flattened `& .btn` to `.button-group .btn` for older browser compatibility while preserving modern color science.

### Example 3: Edge Case — Handling CSS Modules with Lightning CSS
CSS Modules prevent global class collisions by hashing class selectors:
```javascript
import { transform } from 'lightningcss';

const css = `
  .container { padding: 2rem; }
  .active { color: #0284c7; }
`;

const res = transform({
  filename: 'Card.module.css',
  code: Buffer.from(css),
  cssModules: true,
  minify: true
});

console.log('CSS Output:', res.code.toString());
console.log('Exported Hashes:', res.exports);
```
*Output Hashes*:
```javascript
// res.exports:
// {
//   container: { name: 'Card_container_a7b8e' },
//   active: { name: 'Card_active_c9d1f' }
// }
```

### Example 4: Failure Case — Conflicting Linter & Formatter Rules
A common disaster in web projects: Stylelint and Prettier fighting in a pre-commit git hook:
- Stylelint rule: `"declaration-colon-newline-after": "always-multi-line"`
- Prettier: Collapses the declaration into a single line.
- *Outcome*: Prettier runs on save, Stylelint errors on commit, developer cannot push code.
- *Remedy*: Install `stylelint-config-prettier` or remove stylistic rules from Stylelint entirely. Let Prettier format whitespace, and let Stylelint detect errors.

### Example 5: Accessible Linting with `stylelint-a11y`
Catching accessibility issues in CSS automatically:
```json
// .stylelintrc.json
{
  "plugins": ["stylelint-a11y"],
  "rules": {
    "a11y/no-outline-none": true,
    "a11y/selector-pseudo-class-focus": true
  }
}
```
*Code violating rule (anti-pattern)*:
```css
/* Anti-pattern: Removing outline without replacement breaks keyboard focus */
/* stylelint-disable-next-line a11y/no-outline-none */
button:focus {
  outline: none; /* Missing required :focus-visible fallback */
}
```
*Accessible fix*:
```css
button:focus-visible {
  outline: 2px solid #0284c7;
  outline-offset: 2px;
}
```

---

## 6. Best Practices

1. **Strictly Separate Formatting from Quality Linting**: Never use Stylelint rules to enforce tabs, spaces, or line breaks; delegate 100% of formatting to Prettier or Biome.
2. **Migrate Build Pipelines to Lightning CSS**: Replace slow PostCSS/Autoprefixer setups with Lightning CSS inside Vite, Parcel, or standalone Node scripts for 50x–100x faster CI build times.
3. **Target Standard Native CSS**: Write modern standard CSS (native nesting, `@layer`, `oklch()`) and let your build tools downlevel for older targets, rather than adopting non-standard proprietary preprocessor extensions.
4. **Enforce `no-descending-specificity` in Stylelint**: This rule prevents bugs where a selector declared later in the file has lower specificity than an earlier selector, which frequently causes baffling cascade overwrites.
5. **Configure CI Pipeline Checks**: Run `stylelint "**/*.css" --max-warnings 0` and `prettier --check "**/*.css"` in GitHub Actions on every pull request.
6. **Use CSS Source Maps in Development**: Always enable source maps during local development (`sourceMap: true`) so DevTools inspects original source files and line numbers.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Root Cause | Symptom / Failure | Correct Pattern |
|:---|:---|:---|:---|
| Running Stylelint formatting rules alongside Prettier | Overlapping configuration responsibilities | Infinite edit loops; pre-commit hook rejects validly formatted files. | Disable formatting rules in Stylelint; use Prettier for formatting. |
| Forgetting `browserslist` configuration | Relying on tool defaults | Modern CSS features are either polyfilled unnecessarily or not polyfilled enough. | Maintain an explicit `.browserslistrc` targeting your actual analytics matrix. |
| Using outdated PostCSS plugins for native features | Legacy habit from Sass era | Build times balloon; duplicate syntax polyfills produce bloated CSS. | Use native CSS nesting and Lightning CSS transpiler. |
| Over-nesting selectors with CSS nesting | Translating deep Sass tree patterns | Emits massive 6-level high-specificity selectors that cannot be styled cleanly. | Limit nesting to a maximum of 2–3 levels (`max-nesting-depth: 3`). |
| Disabling `outline: none` without `:focus-visible` | Visual aesthetics over accessibility | Keyboard users lose all focus indicators; fails WCAG 2.4.7. | Enforce `stylelint-a11y` plugin in CI pipeline. |
| Running CSS minification during local development | Incorrect environment configuration | DevTools displays compressed single-line CSS, making debugging frustrating. | Enable minification only when `NODE_ENV === 'production'`. |

---

## 8. Performance, Accessibility & Browser Support

### Build Time Performance Benchmarks
Processing a 100,000-line production CSS design system:
- **PostCSS + Autoprefixer + cssnano**: ~1,850 ms (Single-threaded Node.js V8 execution)
- **Dart Sass**: ~890 ms
- **Lightning CSS (Rust Native)**: **~14 ms** (>100x speedup!)

### Accessibility Tooling Checks
Linting tools can statically verify:
- Banning `outline: 0` or `outline: none` without focus replacements (`a11y/no-outline-none`).
- Minimum font sizes (ensuring text does not drop below 12px).
- Mandatory `prefers-reduced-motion` alternates for complex keyframe animations.

---

## 9. Comparison / When to Use What

| Tool | Category | Core Strength | When NOT to Use |
|:---|:---|:---|:---|
| **Stylelint** | Linter (Static Analysis) | Catching bugs, invalid values, specificity traps, accessibility errors | Automated line wrapping or formatting |
| **Prettier** | Formatter | Universal, zero-debate aesthetic consistency across team members | Checking CSS correctness or specificity |
| **Lightning CSS** | Bundler & Minifier (Rust) | Blazing speed, modern standard CSS transpilation, ultra-small bundles | Highly customized custom JS plugin transforms |
| **PostCSS** | Transformer (JS Plugin Host) | Vast ecosystem of niche legacy community plugins | Green-field projects requiring sub-second build times |
| **Sass / SCSS** | Legacy Preprocessor | Deeply entrenched legacy enterprise codebases | New modern projects leveraging native CSS features |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is native CSS nesting preferred over Sass nesting in modern web engineering?</strong></summary>

Native CSS nesting is an official W3C web standard supported across all major browser engines without requiring any compilation step in modern environments. Unlike Sass, which statically flattens selectors at build time, native CSS nesting creates actual DOM-level nested CSSOM rules. This keeps stylesheet file sizes smaller over the wire and allows browser DevTools to display the exact nested hierarchy as authored in source code.
</details>

<details>
<summary><strong>2. How does the Stylelint rule `no-descending-specificity` prevent subtle CSS cascade bugs?</strong></summary>

In CSS, when two rules have different specificities, the higher-specificity rule *always* wins regardless of where it appears in the file. If an author writes a specific rule (e.g., `.card a { color: blue; }`) and later writes a generic rule (e.g., `a { color: red; }`), the second rule will unexpectedly fail to apply to links inside `.card`. The `no-descending-specificity` rule catches this anti-pattern, forcing developers to place higher-specificity rules after lower-specificity rules or use cascade layers.
</details>

<details>
<summary><strong>3. What makes Lightning CSS dramatically faster than PostCSS?</strong></summary>

Lightning CSS is written in Rust and compiles to native machine code. It leverages parallel multi-threaded parsing (via Rayon), operates directly on low-level memory buffers without the overhead of JavaScript V8 object allocations, and performs lexing, AST transformation, downleveling, bundling, and minification in a single consolidated pass. In contrast, PostCSS passes JavaScript AST objects back and forth between multiple independent single-threaded Node.js plugins.
</details>

<details>
<summary><strong>4. How does a modern CSS build tool downlevel modern color spaces like `oklch()` or `display-p3`?</strong></summary>

When targeting older browsers lacking CSS Color Module Level 4 support, tools like Lightning CSS mathematically convert colors from the perceptual OKLCH or Display-P3 color spaces into standard sRGB gamut coordinates. It generates a backward-compatible declaration (e.g., `#0284c7` or `rgb(2, 132, 199)`), and optionally wraps the modern color in an `@supports (color: oklch(0 0 0))` block so modern wide-gamut monitors receive rich vibrant colors while older devices render graceful fallbacks.
</details>

<details>
<summary><strong>5. What is the fundamental difference between Prettier and Stylelint?</strong></summary>

Prettier is an opinionated **formatter** whose sole responsibility is printing source code with consistent indentation, spacing, quotes, and line breaks without altering semantic meaning. Stylelint is a **linter** whose responsibility is code quality: detecting syntax errors, unrecognized properties, logical cascade traps, specificity conflicts, and accessibility violations.
</details>

<details>
<summary><strong>6. How do CSS Modules work at a build-tooling level?</strong></summary>

During the build process, a CSS Modules compiler (like Lightning CSS or Vite) intercepts class and animation names. It replaces local class names like `.title` with globally unique hashed identifiers like `.Card_title__x9z8a` in the emitted CSS file. Simultaneously, it exports a JavaScript key-value map (`{ title: "Card_title__x9z8a" }`) so component code can reference the hashed classes reliably, guaranteeing zero style leakage across components.
</details>

<details>
<summary><strong>7. What role does Browserslist play in modern frontend toolchains?</strong></summary>

Browserslist provides a single source of truth for target browser support across all development tools. It queries the CanIUse database to determine which browsers meet the criteria (e.g., `> 0.2%, not dead`). This matrix is consumed by CSS transformers (Lightning CSS, Autoprefixer), JavaScript compilers (Babel, SWC), and polyfill services to ensure only necessary fallbacks are injected into production bundles.
</details>

<details>
<summary><strong>8. Why should you avoid using `!important` in CSS, and how can Stylelint prevent it?</strong></summary>

`!important` breaks the natural cascade and specificity hierarchy by hoisting declarations into a separate high-priority cascade origin. Overusing it triggers an "arms race" where developers are forced to use increasingly convoluted combinations of IDs and `!important` declarations to override styles. Stylelint enforces clean architecture by providing the rule `"declaration-no-important": true`, failing builds if any developer introduces an `!important` flag.
</details>

---

## 11. Practice Exercises

### Exercise 1: Configure Stylelint to Ban Named Colors (Easy)
**Task**: Write the `.stylelintrc.json` configuration snippet that forbids all named color keywords (such as `red`, `blue`, `papayawhip`), requiring authors to use functional color formats or hex codes.

<details>
<summary><strong>View Solution</strong></summary>

```json
{
  "rules": {
    "color-named": "never"
  }
}
```
*Explanation*: The `"color-named": "never"` rule ensures consistent color representation and prevents magic named colors from circumventing the design token system.
</details>

---

### Exercise 2: Resolve Conflicting Prettier and Stylelint Rules (Easy)
**Task**: A team is experiencing Git pre-commit errors because Stylelint's indentation rule conflicts with Prettier. How do you resolve this permanently in `.stylelintrc.json`?

<details>
<summary><strong>View Solution</strong></summary>

```json
{
  "extends": [
    "stylelint-config-standard",
    "stylelint-config-prettier"
  ]
}
```
*Explanation*: Extending `stylelint-config-prettier` automatically turns off all Stylelint rules that dictate formatting, whitespace, or bracket placement, delegating 100% of formatting control to Prettier.
</details>

---

### Exercise 3: Run Lightning CSS via Node.js Script (Medium)
**Task**: Write a Node.js script `minify.mjs` using the `lightningcss` package to synchronously read a CSS string, minify it, and print the byte savings.

<details>
<summary><strong>View Solution</strong></summary>

```javascript
import { transform } from 'lightningcss';

const sourceCSS = `
  /* Global Layout Container */
  .container {
    display: flex;
    justify-content: space-between;
    padding: 0.5rem 1rem;
    margin: 0px auto;
  }
`;

const result = transform({
  filename: 'test.css',
  code: Buffer.from(sourceCSS),
  minify: true
});

const originalSize = Buffer.byteLength(sourceCSS);
const minifiedSize = result.code.length;
console.log(`Original: ${originalSize}B -> Minified: ${minifiedSize}B`);
console.log('Result:', result.code.toString());
```
*Explanation*: `lightningcss.transform()` processes CSS buffers synchronously in Rust, stripping comments, collapsing `0px` to `0`, and removing unnecessary whitespace.
</details>

---

### Exercise 4: Enforce BEM Class Naming Regex in Stylelint (Medium)
**Task**: Write a Stylelint `selector-class-pattern` regular expression that enforces strict BEM naming (`block__element--modifier`).

<details>
<summary><strong>View Solution</strong></summary>

```json
{
  "rules": {
    "selector-class-pattern": [
      "^[a-z0-9]+(-[a-z0-9]+)*(__[a-z0-9]+(-[a-z0-9]+)*)?(--[a-z0-9]+(-[a-z0-9]+)*)?$",
      {
        "message": "Class selector must conform to BEM convention: block__element--modifier"
      }
    ]
  }
}
```
*Explanation*: This regex validates lowercase kebab-case blocks (e.g., `card-header`), optional double-underscore elements (`card-header__title`), and optional double-hyphen modifiers (`card-header__title--active`).
</details>

---

### Exercise 5: Setup Automated GitHub Action for CSS Quality (Hard)
**Task**: Write a GitHub Actions workflow `.github/workflows/css-quality.yml` that checks out the repository, installs dependencies, and runs Prettier and Stylelint concurrently on all pull requests.

<details>
<summary><strong>View Solution</strong></summary>

```yaml
name: CSS Quality & Linting

on:
  pull_request:
    paths:
      - '**/*.css'
      - '.stylelintrc*'
      - '.prettierrc*'

jobs:
  lint:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout code
        uses: actions/checkout@v4

      - name: Setup Node.js
        uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: 'npm'

      - name: Install dependencies
        run: npm ci

      - name: Verify Formatting with Prettier
        run: npx prettier --check "**/*.css"

      - name: Lint CSS with Stylelint
        run: npx stylelint "**/*.css" --max-warnings 0
```
*Explanation*: This pipeline runs automated checks on PRs touching CSS files, preventing formatting drift or linting regressions from entering the `main` branch.
</details>

---

### Exercise 6: Transpiling Custom Media Queries with Lightning CSS (Hard)
**Task**: Draft an input CSS file with a `@custom-media` query, and configure Lightning CSS to downlevel it into standard `@media` queries for existing browsers.

<details>
<summary><strong>View Solution</strong></summary>

```css
/* src/responsive.css */
@custom-media --viewport-desktop (min-width: 1024px);

.sidebar {
  display: none;

  @media (--viewport-desktop) {
    display: block;
  }
}
```
```javascript
// build.mjs
import { transform } from 'lightningcss';
import fs from 'node:fs';

const input = fs.readFileSync('src/responsive.css');
const res = transform({
  filename: 'responsive.css',
  code: input,
  drafts: {
    customMedia: true // Activates draft W3C Custom Media downleveling
  },
  minify: true
});

console.log(res.code.toString());
// Emits: .sidebar{display:none}@media (min-width:1024px){.sidebar{display:block}}
```
*Explanation*: Enabling `drafts: { customMedia: true }` instructs Lightning CSS to parse the `@custom-media` definition and inline it into matching media blocks across the bundle.
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
| MODERN CSS TOOLING - QUICK REFERENCE CHEAT SHEET                                               |
+-------------------+----------------------------+-----------------------+------------------------+
| Tool              | Role                       | Command / File        | Primary Benefit        |
+-------------------+----------------------------+-----------------------+------------------------+
| Stylelint         | Static Analysis & Linter   | .stylelintrc.json     | Catches bugs & anti-pat|
| Prettier          | Code Formatter             | .prettierrc           | Aesthetic consistency  |
| Lightning CSS     | Bundler & Transpiler (Rust)| npx lightningcss      | 100x faster than JS    |
| PostCSS           | AST Plugin Runner          | postcss.config.js     | Legacy plugin host     |
| Browserslist      | Target Definition          | .browserslistrc       | Unified target matrix  |
+-------------------+----------------------------+-----------------------+------------------------+
```

---

## 13. Further Reading

- [Stylelint Official Documentation](https://stylelint.io/)
- [Prettier CSS Formatting Guide](https://prettier.io/docs/en/options.html)
- [Lightning CSS Documentation & Benchmarks](https://lightningcss.dev/)
- [W3C CSS Nesting Specification](https://www.w3.org/TR/css-nesting-1/)
- [Browserslist Best Practices](https://github.com/browserslist/browserslist)

---

[Previous: 04 - DevTools for CSS](04-devtools-for-css.md) | [Section Index](README.md) | [Next: 06 - CSS Modules, Spec Levels & Baseline](06-css-modules-levels-and-baseline.md)
