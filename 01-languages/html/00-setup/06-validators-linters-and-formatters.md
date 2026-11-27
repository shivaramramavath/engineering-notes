# HTML Validators, Linters & Formatters

## Metadata
| Attribute | Details |
|:---|:---|
| **Level** | Intermediate |
| **Prerequisites** | `00-setup/01-what-is-html-and-the-web.md`, `00-setup/05-your-first-page.md` |
| **Time to Complete** | 35 minutes |
| **Status** | Complete |
| **Spec Level / Baseline Status** | Baseline: Widely Available (Automated Toolchain Standard) |
| **Browsers Tested** | Chromium 130+, Gecko 130+, WebKit 18+ |
| **Tested With** | Node v24.11.1, html_runner.js, W3C Validator rules |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [The W3C Nu HTML Checker (v.Nu)](#the-w3c-nu-html-checker-vnu)
4. [Modern HTML Linting with html-validate](#modern-html-linting-with-html-validate)
5. [Code Formatting with Prettier](#code-formatting-with-prettier)
6. [Automating Accessibility Testing with axe-core](#automating-accessibility-testing-with-axe-core)
7. [Syntax & Configuration Rules](#syntax--configuration-rules)
8. [Practical Examples](#practical-examples)
9. [Best Practices](#best-practices)
10. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
11. [Performance, Accessibility, SEO & Security Notes](#performance-accessibility-seo--security-notes)
12. [Comparison Matrix: HTML Verification Tools](#comparison-matrix-html-verification-tools)
13. [Real-World Interview Questions](#real-world-interview-questions)
14. [Practice Exercises](#practice-exercises)
15. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

Because web browser parsers are designed never to crash—automatically forgiving typos, missing tags, and malformed nesting—software engineers face a dangerous illusion: **"If it renders on my screen, it must be correct."**

In reality, a page that appears visual identical to the author may be completely unparseable to search engine crawlers, unusable to screen reader users, or silently broken on mobile WebKit.

Automated verification tools eliminate this blind spot:
- **Validators (W3C Nu HTML Checker)**: Prove strict compliance with the WHATWG HTML Living Standard specification.
- **Linters (`html-validate`, `axe-core`)**: Enforce enterprise coding standards, block deprecated tags, detect missing accessibility labels, and prevent security regressions.
- **Formatters (`Prettier`)**: Enforce consistent indentation, attribute line wrapping, and deterministic whitespace sensitivity across multi-developer teams.

Integrating automated validation into pre-commit git hooks and CI/CD deployment pipelines guarantees that broken markup never reaches production.

---

## Core Concepts

### 1. Validation vs Linting
- **HTML Validation**: Objective, binary conformance to the formal grammar of the WHATWG specification (e.g. "An `<li>` element is only permitted as a child of `<ul>` or `<ol>`").
- **HTML Linting**: Contextual, opinionated enforcement of best practices, accessibility guidelines, security rules, and team style guides (e.g. "Disallow inline `onclick` handlers" or "Warn if an `<img>` tag lacks explicit `width` and `height`").

### 2. Prettier Whitespace Sensitivity
HTML is whitespace-sensitive: consecutive spaces and newlines between inline elements (like `<span>` or `<a>`) render as physical space characters on screen.
- In Prettier, the setting `htmlWhitespaceSensitivity: "css"` respects CSS display properties: it wraps tags carefully to prevent accidentally injecting unwanted visual whitespace between inline text nodes.

---

## The W3C Nu HTML Checker (v.Nu)

The **Nu HTML Checker (v.Nu)** is the official, authoritative validator backend maintained by the W3C and WHATWG (the engine powering `validator.w3.org/nu/`).

### Running Locally or in CI/CD:
```bash
# Running via Docker container locally
docker run -it --rm -p 8888:8888 ghcr.io/validator/validator:latest

# Validating via curl:
curl -H "Content-Type: text/html; charset=utf-8" \
     --data-binary @index.html \
     https://validator.w3.org/nu/?out=json
```

---

## Modern HTML Linting with html-validate

For modern Node.js-based frontend projects, **`html-validate`** is the premier offline HTML linter: it parses HTML, SVG, and template files with pluggable rule configurations.

### Installation & Configuration:
```bash
npm install --save-dev html-validate
```

Create `.htmlvalidate.json` in project root:
```json
{
  "extends": ["html-validate:recommended", "html-validate:a11y"],
  "rules": {
    "doctype-html": "error",
    "element-required-attributes": "error",
    "no-deprecated-attr": "error",
    "no-inline-style": "warn",
    "void-style": ["error", { "style": "omit" }],
    "input-missing-label": "error",
    "heading-level": "error"
  }
}
```

Run in terminal or CI:
```bash
npx html-validate "src/**/*.html"
```

---

## Code Formatting with Prettier

Prettier standardizes HTML formatting across engineering organizations.

### Recommended `.prettierrc.json`:
```json
{
  "parser": "html",
  "printWidth": 100,
  "tabWidth": 2,
  "useTabs": false,
  "singleQuote": false,
  "htmlWhitespaceSensitivity": "css",
  "bracketSameLine": false
}
```

---

## Automating Accessibility Testing with axe-core

While syntax linters catch malformed tags, **axe-core** (by Deque Systems) is the industry standard for catching **WCAG 2.2 Level A and AA** accessibility violations.

### Running axe via Playwright or CLI in CI:
```bash
npm install --save-dev @axe-core/playwright
```

```javascript
// automated-a11y.test.js
const { test, expect } = require('@playwright/test');
const AxeBuilder = require('@axe-core/playwright').default;

test('Homepage should have no detectable accessibility violations', async ({ page }) => {
  await page.goto('http://localhost:8000');
  const accessibilityScanResults = await new AxeBuilder({ page }).analyze();
  expect(accessibilityScanResults.violations).toEqual([]);
});
```

---

## Syntax & Configuration Rules

```yaml
# GitHub Actions CI Workflow: .github/workflows/html-audit.yml
name: HTML Standards & Accessibility Audit

on: [push, pull_request]

jobs:
  validate:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 22

      - name: Install Linting Tools
        run: npm install -g html-validate @axe-core/cli

      - name: Run HTML-Validate
        run: html-validate "src/**/*.html"

      - name: Run axe-core A11y Audit
        run: axe "http://localhost:8000" --tags wcag2a,wcag2aa
```

---

## Practical Examples

### Example 1: Catching Invalid Nesting in html-validate
- **Goal**: Detect an invalid element nesting bug before deployment.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Invalid Nesting</title>
</head>
<body>
  <!-- INVALID: A block element <p> cannot be placed inside an inline <span> -->
  <span>
    <p>Misnested paragraph inside inline span.</p>
  </span>
</body>
</html>
```
- **Real Validator & DOM Output**:
```json
{
  "status": "INVALID",
  "issueCount": 1,
  "issues": [
    {
      "severity": "error",
      "code": "element-permitted-content",
      "message": "<p> element is not permitted as a descendant of <span> (violates phrasing content rules)."
    }
  ],
  "fullDocument": true
}
```
- **Explanation**: The W3C specification dictates that `<span>` only permits phrasing content. Placing block-level `<p>` inside `<span>` fails validation and can trigger parser-forced tag closure.

### Example 2: Catching Missing Alt Text on Informative Images
- **Goal**: Demonstrate how linters enforce WCAG Success Criterion 1.1.1 (Non-text Content).
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Missing Alt</title>
</head>
<body>
  <nav>
    <a href="/">
      <img src="logo.png">
    </a>
  </nav>
</body>
</html>
```
- **Real Validator & DOM Output**:
```json
{
  "status": "INVALID",
  "issueCount": 1,
  "issues": [
    {
      "severity": "error",
      "code": "img-alt-missing",
      "message": "<img> element missing alt attribute. Add alt=\"\" for decorative images or descriptive text for informative images."
    }
  ],
  "fullDocument": true
}
```
- **Explanation**: A linked image without an `alt` attribute creates an unlabelled interactive control: screen readers announce only the raw image file path (e.g. "link, slash logo dot png"), failing WCAG 2.2 Level A.

### Example 3: Edge Case — Unlabelled Input with Hidden Visual Text
- **Goal**: Verify how linters check form control and label association.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Unassociated Form Label</title>
</head>
<body>
  <form>
    <span>Email Address:</span>
    <input type="email" id="user-email" name="email">
    <button type="submit">Submit</button>
  </form>
</body>
</html>
```
- **Real Validator & DOM Output**:
```json
{
  "status": "INVALID",
  "issueCount": 1,
  "issues": [
    {
      "severity": "warning",
      "code": "input-missing-label",
      "message": "<input id=\"user-email\"> has no associated <label for=\"user-email\"> or aria-label."
    }
  ],
  "fullDocument": true
}
```
- **Explanation**: Wrapping the label in a generic `<span>` fails to associate it programmatically with the `<input>`. Clicking the text does not focus the input, and assistive technologies announce an unlabelled edit field.

### Example 4: Failure Mode — Skipped Heading Levels
- **Goal**: Detect an accessibility outline hierarchy failure.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Skipped Heading</title>
</head>
<body>
  <h1>Quarterly Engineering Report</h1>
  <!-- ANTI-PATTERN: Skipping from h1 directly to h3! -->
  <h3>Executive Summary</h3>
  <p>Our distributed database migration completed with zero downtime.</p>
</body>
</html>
```
- **Real Validator & DOM Output**:
```json
{
  "status": "VALID",
  "issueCount": 1,
  "issues": [
    {
      "severity": "warning",
      "code": "heading-order-skipped",
      "message": "Heading level skipped: <h1> directly followed by <h3> without <h2>."
    }
  ],
  "fullDocument": true
}
```
- **Explanation**: Screen reader users navigate pages by jumping through heading levels. Skipping from `<h1>` directly to `<h3>` implies that an entire parent section is missing, confusing navigational flow.

### Example 5: Production-Grade Pre-Commit Verified Markup
- **Goal**: Full document passing all linter, validator, and accessibility checks.
- **Code**:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Customer Account Registration</title>
</head>
<body>
  <main>
    <h1>Create Your Account</h1>
    <form action="/register" method="post">
      <p>
        <label for="reg-email">Work Email</label>
        <input type="email" id="reg-email" name="email" required autocomplete="email">
      </p>
      <p>
        <button type="submit">Register</button>
      </p>
    </form>
  </main>
</body>
</html>
```
- **Real Validator & DOM Output**:
```json
{
  "status": "VALID",
  "issueCount": 0,
  "issues": [],
  "fullDocument": true
}
```
- **Explanation**: Strictly passes W3C Nu Checker and `html-validate`. Features programmatic label association, valid form landmarks, and autocomplete tokens.

---

## Best Practices

1. **Integrate Validation into CI/CD Pipelines**: Never rely on developers manually validating HTML. Run `html-validate` as a mandatory pull-request check.
2. **Use Prettier with `htmlWhitespaceSensitivity: "css"`**: Guarantees that formatting never introduces accidental whitespace between inline tags.
3. **Configure Git Pre-Commit Hooks**: Use `husky` and `lint-staged` to run linters on staged `.html` files before commits are created.
4. **Automate axe-core Accessibility Scans**: Run automated accessibility tests in unit or end-to-end tests (Playwright/Cypress) to prevent WCAG regressions.
5. **Always Set `"void-style": "omit"`**: Configure your linter to disallow trailing XML slashes (`<br />` $\rightarrow$ `<br>`), enforcing clean modern HTML5 syntax.

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Broken links or inputs slip into production** | Relying on visual browser rendering rather than automated HTML linting. | Add `html-validate` and W3C Nu Checker to CI. |
| **Random single-space gaps between words in UI** | Formatting HTML with reckless newline wrapping between inline tags. | Configure Prettier with `htmlWhitespaceSensitivity: "css"`. |
| **CI build passes, but screen reader users blocked** | Relying solely on syntax validators without running semantic/a11y tools (`axe-core`). | Combine syntax validation with `@axe-core/playwright`. |
| **Linter throws 500 errors on trailing slashes** | Using XHTML-style `<input />` when linter is configured for standard HTML5. | Configure `"void-style": ["error", {"style": "omit"}]`. |
| **Form inputs lack clickable labels** | Using plain text or `<span>` instead of `<label for="id">`. | Always wrap label text in `<label for="inputId">`. |
| **W3C validator throws "character not permitted"** | Saving files as UTF-8 with BOM (Byte Order Mark) or Latin-1. | Configure editor to save strictly in UTF-8 without BOM. |

---

## Performance, Accessibility, SEO & Security Notes

### Performance
Automated linters enforce performance rules, such as warning when an `<img>` tag lacks `loading="lazy"` or explicit `width`/`height` dimensions.

### Accessibility
`axe-core` automatically verifies color contrast ratios, ARIA attribute validity, form control labels, landmark uniqueness, and focus traps.

### SEO
Validators verify that canonical URLs, robots meta tags, and structured data JSON-LD scripts are syntactically well-formed, preventing search indexing penalties.

### Security
Linters flag inline event handlers (`onclick="..."`) and links with `target="_blank"` missing `rel="noopener noreferrer"`.

---

## Comparison Matrix: HTML Verification Tools

| Tool | Focus Area | Runtime / Execution | CI/CD Integration | Spec Authority |
|:---|:---|:---|:---|:---|
| **W3C Nu HTML Checker (v.Nu)** | Pure Specification Conformance | Java / Docker / HTTP Service | Excellent (Docker/Action) | **Official WHATWG/W3C** |
| **`html-validate`** | Code Style, Best Practices, A11y | Node.js CLI / Library | Fast, lightweight npm tool | Industry Standard |
| **`axe-core`** | WCAG 2.2 Accessibility Auditing | Browser Engine / Playwright | Universal a11y testing | **Deque / W3C WAI** |
| **Prettier** | Code Formatting & Indentation | Node.js / Editor plugin | Fast pre-commit hook | Opinionated Formatter |
| **Lighthouse CI** | Overall Web Vitals, SEO, A11y | Headless Chromium | Cloud/CI metric runner | Google Chrome Team |

---

## Real-World Interview Questions

<details>
<summary><strong>1. Why is a page that renders perfectly in Google Chrome not necessarily valid HTML?</strong></summary>

**Answer:**
Google Chrome (like all modern browser engines) implements the WHATWG error-recovery algorithm: its primary mission is to present content to the user regardless of how broken, malformed, or invalid the underlying HTML syntax is. 

For instance, Chrome will visually render misnested tags (e.g. `<b><i>text</b></i>`), automatically synthesize missing `<tbody>` elements, and ignore missing `alt` attributes. However, this visual forgiveness masks severe flaws:
1. Assistive technologies (screen readers) relying on the Accessibility Tree may encounter missing names or broken roles.
2. Search engine crawlers (Googlebot, Bingbot) may misinterpret content hierarchy.
3. Minor variations between browser rendering engines (Chromium vs WebKit vs Gecko) can result in completely different recovery paths.
Automated validation proves the markup conforms to the universal standard across all consumers.
</details>

<details>
<summary><strong>2. What is htmlWhitespaceSensitivity in Prettier, and why is it critical for HTML?</strong></summary>

**Answer:**
In HTML, whitespace between inline elements (such as `<a>`, `<span>`, `<img>`) is significant: a newline or space between tags produces a physical visible space character on screen.

Prettier's `htmlWhitespaceSensitivity` controls how the formatter wraps tags:
- **`"css"` (Default / Recommended)**: Prettier inspects the default CSS display mode of the tags. For inline tags, it wraps lines so that no new whitespace is injected (often placing the closing `>` on the next line immediately before the next tag).
- **`"strict"`**: Treats all whitespace everywhere as significant, refusing to reformat lines that might alter spaces.
- **`"ignore"`**: Treats all whitespace as insignificant, wrapping tags cleanly at line boundaries. (Can introduce unwanted space gaps between styled inline elements).
</details>

<details>
<summary><strong>3. How does axe-core test for accessibility violations, and what are its limitations?</strong></summary>

**Answer:**
`axe-core` executes an automated ruleset directly inside a live browser rendering engine (or headless browser via Playwright/Puppeteer):
- It inspects the computed DOM, computed styles (CSS color values for contrast calculations), and the Accessibility Object Model.
- It tests deterministic WCAG criteria: missing `alt` attributes, invalid ARIA roles, form inputs without labels, insufficient color contrast ratios, duplicate IDs, and missing document language.

**Limitations**: Automated a11y testing catches only roughly **30% to 50% of real accessibility barriers**. It cannot evaluate whether an `alt` description is contextually meaningful, whether dynamic focus management feels intuitive, or whether tab order matches the logical reading order. Manual keyboard and screen reader testing remains mandatory.
</details>

<details>
<summary><strong>4. What is the difference between the W3C Nu HTML Checker and html-validate?</strong></summary>

**Answer:**
- **W3C Nu HTML Checker (v.Nu)**: The official reference validator maintained by WHATWG and W3C. It strictly validates whole HTML documents against the latest Living Standard grammar. It is typically run via Docker or Java.
- **`html-validate`**: A modern Node.js linter designed for developer workflows. It works on both full documents and partial template fragments (e.g. Vue/React templates, Jinja partials, web components), supports custom rule configurations, and integrates seamlessly into `npm test` and pre-commit hooks.
</details>

<details>
<summary><strong>5. How can you set up a Git pre-commit hook to prevent invalid HTML from being committed?</strong></summary>

**Answer:**
Using `husky` and `lint-staged`:
1. Install tools: `npm install --save-dev husky lint-staged html-validate`.
2. Configure `lint-staged` in `package.json`:
   ```json
   "lint-staged": {
     "*.html": ["html-validate", "prettier --write"]
   }
   ```
3. Initialize husky hook:
   ```bash
   npx husky init
   echo "npx lint-staged" > .husky/pre-commit
   ```
Whenever a developer runs `git commit`, `lint-staged` automatically runs `html-validate` against staged HTML files. If a validation error is detected, the commit is aborted immediately.
</details>

<details>
<summary><strong>6. Why does html-validate flag void elements with trailing slashes (e.g. &lt;img ... /&gt;) by default?</strong></summary>

**Answer:**
Under the modern WHATWG HTML Living Standard, trailing slashes on void elements (`<img />`, `<br />`, `<input />`) are a legacy artifact of XHTML 1.0. In HTML5, the parser completely ignores the slash.

Furthermore, developers often mistakenly assume the slash self-closes non-void elements (writing `<div />` or `<span />`), which leads to severe DOM nesting bugs. To eliminate confusion and enforce clean HTML5 syntax, linters default to `"void-style": "omit"`, requiring `<img src="...">` and `<br>`.
</details>

<details>
<summary><strong>7. What is an "ambiguous ampersand" error in HTML validation?</strong></summary>

**Answer:**
In HTML, the ampersand character (`&`) introduces an entity reference (e.g. `&amp;`, `&lt;`, `&copy;`).
If an author writes `Fish & Chips` or a query string `href="page?a=1&b=2"`, the parser encounters `&` followed by characters that do not form a valid named character reference. 

While modern HTML parsers recover by emitting a literal `&`, the W3C validator flags this as an **Ambiguous Ampersand**: if a future HTML standard ever adds an entity named `&b;`, the link URL would silently mutate. The ampersand must always be explicitly escaped as `&amp;` (`Fish &amp; Chips`, `href="page?a=1&amp;b=2"`).
</details>

<details>
<summary><strong>8. Can linters detect CSS and JavaScript bugs inside HTML documents?</strong></summary>

**Answer:**
Yes. Advanced HTML linters (and ESLint plugins like `eslint-plugin-html` or Stylelint processors) extract inline `<style>` and `<script>` blocks from HTML documents and pipe them into their respective linters. This allows developers to catch invalid CSS syntax, unsupported vendor prefixes, JavaScript syntax errors, and CSP violations directly within single HTML files.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Configuring Prettier for Clean HTML</strong></summary>

**Problem:** Write a `.prettierrc.json` configuration file that formats HTML files with 2-space indentation, 100-character line wrap, double quotes, and CSS whitespace sensitivity.

**Solution:**
```json
{
  "parser": "html",
  "tabWidth": 2,
  "printWidth": 100,
  "singleQuote": false,
  "htmlWhitespaceSensitivity": "css"
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Resolving an Ambiguous Ampersand</strong></summary>

**Problem:** The W3C validator throws an error on this line:
```html
<a href="/search?category=books&author=tolkien">Search</a>
```
Fix the URL according to W3C validation rules.

**Solution:**
```html
<a href="/search?category=books&amp;author=tolkien">Search</a>
```
*(In HTML attributes, raw ampersands in query parameters must be escaped as `&amp;`).*
</details>

<details>
<summary><strong>Exercise 3 (Medium): Writing an html-validate Configuration File</strong></summary>

**Problem:** Write a `.htmlvalidate.json` file that extends recommended rules, enforces standard HTML5 void tags (no trailing slashes), requires labels on all form inputs, and warns on inline styles.

**Solution:**
```json
{
  "extends": ["html-validate:recommended"],
  "rules": {
    "void-style": ["error", { "style": "omit" }],
    "input-missing-label": "error",
    "no-inline-style": "warn",
    "doctype-html": "error"
  }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Auditing HTML Validation in Python</strong></summary>

**Problem:** Write a Python script that calls our scratch runner `html_runner.js` to validate an HTML file and exits with code 1 if any validation error is detected.

**Solution:**
```python
import subprocess, sys, json

def validate_file(file_path):
    runner = r"C:\Users\RAMAVATH SHIVA RAM\.gemini\antigravity\brain\a9bfc182-e985-4a5d-9df0-ebf568360240\scratch\html_runner.js"
    res = subprocess.run(["node", runner, file_path], capture_output=True, text=True)
    data = json.loads(res.stdout)
    if data["status"] != "VALID":
        print(f"Validation FAILED ({data['issueCount']} issues):")
        for issue in data["issues"]:
            print(f"  [{issue['severity'].upper()}] {issue['code']}: {issue['message']}")
        sys.exit(1)
    print("Validation PASSED successfully.")

if __name__ == "__main__":
    validate_file(sys.argv[1])
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Creating an Automated Playwright + axe-core Test</strong></summary>

**Problem:** Write a complete Playwright test in JavaScript that visits `http://localhost:8000/signup`, executes an axe-core accessibility audit, and logs any WCAG 2.2 Level AA violations.

**Solution:**
```javascript
const { chromium } = require('playwright');
const AxeBuilder = require('@axe-core/playwright').default;

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  await page.goto('http://localhost:8000/signup');

  const results = await new AxeBuilder({ page })
    .withTags(['wcag2a', 'wcag2aa'])
    .analyze();

  if (results.violations.length > 0) {
    console.error(`Found ${results.violations.length} accessibility violations:`);
    for (let v of results.violations) {
      console.error(`- [${v.impact.toUpperCase()}] ${v.help}: ${v.helpUrl}`);
    }
    process.exit(1);
  } else {
    console.log('Zero accessibility violations detected!');
  }

  await browser.close();
})();
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Writing a GitHub Actions PR Linter</strong></summary>

**Problem:** Author a complete GitHub Actions YAML workflow that checks out code, installs Node.js, runs `html-validate` on all HTML files in `public/`, and fails the pull request if errors exist.

**Solution:**
```yaml
name: HTML Linter

on:
  pull_request:
    paths:
      - 'public/**/*.html'

jobs:
  lint:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Setup Node.js
        uses: actions/setup-node@v4
        with:
          node-version: 22

      - name: Install html-validate
        run: npm install -g html-validate

      - name: Lint HTML Files
        run: html-validate "public/**/*.html"
```
</details>

---

## Summary & Further Reading

- Never rely on visual browser rendering alone to verify markup correctness; browsers automatically forgive broken syntax.
- **W3C Nu HTML Checker** enforces strict Living Standard conformance.
- **`html-validate`** provides configurable, offline linting for Node.js workflows and CI/CD.
- **`axe-core`** automates WCAG 2.2 Level A and AA accessibility auditing.
- Use **Prettier** with `htmlWhitespaceSensitivity: "css"` to format HTML deterministically without injecting unwanted inline whitespace.
- **Next Topic**: [Module 01: Fundamentals & Core Syntax](../01-fundamentals/README.md) explores document structure, syntax rules, text content, links, images, and global attributes.

---
[Previous](05-your-first-page.md) | [Section Index](README.md) | [Next](../01-fundamentals/README.md)
