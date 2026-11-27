# CSS Learning Track Conventions (CSS_CONVENTIONS.md)

**Curriculum Scope:** `01-languages/css/`  
**Governing Standard:** W3C Cascading Style Sheets (CSSWG) Specifications  
**Baseline Standard:** Web Platform Baseline (W3C WebDX / MDN / web.dev)  
**Verification Toolchain:** Playwright Runner `scratch/css_runner.js` (Node.js v24.11.1)

---

## 1. Directory Structure & File Naming Conventions

All files within `01-languages/css/` must strictly adhere to the following conventions:

1. **Module Directories**: Two-digit prefix with descriptive kebab-case naming:
   - `00-setup/`
   - `01-fundamentals/`
   - `02-box-model-and-layout-basics/`
   - `03-flexbox/`
   - `04-grid/`
   - `05-responsive-design/`
   - `06-typography-and-text/`
   - `07-visual-styling/`
   - `08-motion/`
   - `09-modern-css/`
   - `10-architecture-and-methodologies/`
   - `11-accessibility-and-ux-in-css/`
   - `12-performance-and-debugging/`
   - `13-layout-and-component-recipes/`
   - `14-projects/`
   - `15-interview/`
   - `16-cheatsheets/`

2. **Note Files**: Two-digit prefix with kebab-case filenames:
   - `01-syntax-rules-and-declarations.md`, `02-selectors.md`, etc.
   - Module overview in every directory: `README.md`.

3. **Strictly Markdown-Only Policy**:
   - Zero raw `.css`, `.html`, `.js`, or `.json` files inside the repository.
   - All code, stylesheets, and examples must reside in fenced markdown code blocks with explicit language tags (````css`, ````html`, ````javascript`).
   - Playwright test runners, stylelint fixtures, and intermediate logs reside exclusively outside the repository in the designated scratch folder.

---

## 2. Note Metadata Specification

Every concept note must open with the standardized metadata table:

```markdown
# Note Title

| Metadata | Details |
|:---|:---|
| **Level** | Foundational / Intermediate / Advanced / Architectural |
| **Prerequisites** | Relative links to prior concept notes |
| **Estimated Reading Time** | e.g. 25-35 minutes |
| **Status** | Current / Draft |
| **Baseline Status** | Widely Available / Newly Available / Limited Availability / Experimental |
| **Browsers Tested** | Chromium 130+ (Blink), Gecko 130+ (Firefox), WebKit 18+ (Safari) |
| **Tested With** | Playwright css_runner.js, getComputedStyle(), getBoundingClientRect(), stylelint |
```

---

## 3. Mandatory 13-Section Concept Note Template

Every concept note must contain the following 13 numbered sections (target length: 450–750 lines):

1. **Title & Metadata Table**
2. **Table of Contents**
3. **1. What & Why**: Formal definition, historical motivation, real-world engineering problem solved.
4. **2. Core Concepts**: Mental models, terminology, spec definitions.
5. **3. How It Works Internally**:
   - Spec layout algorithm, cascade resolution tree, stacking context creation, paint layers, or GPU compositing.
   - ASCII or Mermaid architectural diagram.
6. **4. Syntax & Usage**: Formal syntax, properties, keywords, Baseline status box.
7. **5. Practical Examples** (5 progressive, verified examples):
   - Example 1: Basic / Conceptual
   - Example 2: Real-World Production
   - Example 3: Edge Case / Complex Layout
   - Example 4: Failure Case / Anti-Pattern
   - Example 5: Accessible / Production-Grade Component
   - *Requirement*: Each example must include Goal, HTML+CSS, Computed/Measured Values (from Playwright / `getComputedStyle`), and Technical Explanation.
8. **6. Best Practices**: 6 actionable rules for maintainability and performance.
9. **7. Common Mistakes & Pitfalls**: Markdown table with 6+ rows (Mistake, Severity, Consequence, Correction).
10. **8. Performance, Accessibility & Browser-Support Notes**:
    - Layout thrashing, paint costs, compositor-only animation.
    - Accessibility: WCAG contrast, `:focus-visible`, reduced motion, high contrast mode.
    - Baseline status, fallback strategies (`@supports`).
11. **9. Comparison / When to Use What**: Decision matrix comparing alternative CSS features or layout models (including when NOT to use).
12. **10. Interview Questions**: 8 deep technical questions with verified `<details>` answers.
13. **11. Practice Exercises**: 6 exercises (2 easy, 2 medium, 2 hard) with verified `<details>` code solutions.
14. **12. Summary / Cheat Sheet**: Bulleted key takeaways and syntax quick reference.
15. **13. Further Reading**: Verified URLs to W3C specifications, MDN Web Docs, and web.dev articles.
- **Footer Navigation**: `[Previous](...) | [Section Index](README.md) | [Next](...)`

---

## 4. Four-File Project Specification

Every reference project in `14-projects/` must contain 4 markdown files:
- `README.md`: Project overview, architecture, live layout preview, requirements.
- `01-design.md`: Design system, color tokens, typography scale, breakpoint strategy.
- `02-html-and-css.md`: Complete production HTML and CSS in fenced markdown blocks.
- `03-measured-results.md`: Real computed values, bounding client rect dimensions, and verified viewport layouts.
- `04-limitations-and-next-steps.md`: Known trade-offs, progressive enhancements, and future spec upgrades.
