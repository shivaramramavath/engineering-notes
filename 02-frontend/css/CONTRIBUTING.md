# Contributing to the CSS Learning Track (CONTRIBUTING.md)

Thank you for contributing to the CSS engineering curriculum. This guide outlines authoring standards, verification requirements, and repository constraints.

---

## 1. Markdown-Only Output Constraint

- The repository accepts **strictly `.md` files** (and `tree.txt`).
- **Never commit** raw `.css`, `.html`, `.js`, or `.json` files.
- All stylesheets, markup examples, and configuration files must reside within fenced markdown code blocks with explicit language tags:
  ````css
  .container {
    display: flex;
    gap: 1rem;
  }
  ````

---

## 2. Mandatory 13-Section Concept Note Template

Every concept note must strictly adhere to the 13 sections defined in `_templates/concept-template.md`:
1. Title & Metadata Table (Level, Prerequisites, Time, Status, Baseline Status, Browsers Tested, Tested With)
2. Table of Contents
3. 1 What & Why
4. 2 Core Concepts
5. 3 How It Works Internally (Algorithm + ASCII or Mermaid diagram)
6. 4 Syntax & Usage (with cross-browser Baseline status box)
7. 5 Practical Examples (5 progressive verified examples: Basic, Real-World, Edge Case, Failure Case, Accessible/Production-Grade)
8. 6 Best Practices (6 actionable guidelines)
9. 7 Common Mistakes & Pitfalls (table with 6+ rows)
10. 8 Performance, Accessibility & Browser Support
11. 9 Comparison / When to Use What (and when NOT to use)
12. 10 Interview Questions (8 deep technical questions with `<details>` answers)
13. 11 Practice Exercises (6 exercises: 2 easy, 2 medium, 2 hard with `<details>` solutions)
14. 12 Summary / Cheat Sheet
15. 13 Further Reading (verified W3C, MDN, web.dev URLs)
- Footer Navigation: `[Previous](...) | [Section Index](README.md) | [Next](...)`

---

## 3. Playwright Scratch Verification Toolchain

All examples must be verified using the Playwright runner located outside the repository at `scratch/css_runner.js`:
- Renders HTML+CSS in headless Chromium, Firefox, and WebKit.
- Reads computed styles using `window.getComputedStyle()`.
- Measures element dimensions using `getBoundingClientRect()`.
- Validates CSS rules against stylelint.
- Verifies color contrast using mathematical relative luminance calculations.

---

## 4. Web Platform Baseline & Standards Rigor

- Always cite the feature's Web Platform Baseline status (Widely Available / Newly Available / Limited Availability).
- Any feature that is not Widely Available must be accompanied by an `@supports` fallback code pattern.
- Explicitly flag and contrast legacy anti-patterns (e.g., float layouts, clearfix hacks, `!important` abuse, 100vh bugs).
