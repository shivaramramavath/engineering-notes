# HTML Learning Track Conventions (HTML_CONVENTIONS.md)

**Curriculum Scope:** `01-languages/html/`  
**Governing Standard:** WHATWG HTML Living Standard  
**Baseline Standard:** Web Platform Baseline (W3C WebDX / web.dev)  
**Verification Toolchain:** Scratch runner `scratch/html_runner.js` (Node.js v24.11.1)

---

## 1. Directory Structure & File Naming Conventions

All files within `01-languages/html/` must adhere to these rules:
1. **Module Directories**: Two-digit prefix in kebab-case:
   - `00-setup/`
   - `01-fundamentals/`
   - `02-semantics-and-structure/`
   - `03-forms/`
   - `04-media-and-embeds/`
   - `05-head-metadata-and-seo/`
   - `06-accessibility/`
   - `07-modern-html-and-apis/`
   - `08-performance-and-security/`
   - `09-patterns-and-architecture/`
   - `10-projects/`
   - `11-interview/`
   - `12-cheatsheets/`
2. **Note Files**: Numbered sequentially with two digits and descriptive kebab-case names:
   - `01-document-structure.md`, `02-syntax-rules.md`, etc.
   - Module README in every folder: `README.md`.
3. **Strictly Markdown-Only**:
   - Zero `.html`, `.css`, `.js`, `.json`, `.png` files inside the repository.
   - All code, schemas, and configurations must reside in fenced markdown code blocks with explicit language tags (````html`, ````css`, ````javascript`, ````json`).
   - Scratch runner scripts, DOM tests, and temporary HTML fixtures reside exclusively outside the repository in the designated scratch directory.

---

## 2. Note Metadata Specification

Every concept note must open with the standardized metadata table:

```markdown
# Note Title

## Metadata
| Attribute | Details |
|:---|:---|
| **Level** | Beginner / Intermediate / Advanced / Architectural |
| **Prerequisites** | Relative links to prior notes |
| **Time to Complete** | e.g. 25-35 minutes |
| **Status** | Complete / Draft |
| **Spec Level / Baseline Status** | Baseline: Widely Available / Baseline: Newly Available / Limited Availability / Experimental |
| **Browsers Tested** | Chromium 130+ (Blink), Gecko 130+ (Firefox), WebKit 18+ (Safari) |
| **Tested With** | Node v24.11.1, html_runner.js, W3C Validator rules |
```

---

## 3. Mandatory 13-Section Concept Note Template

Every concept note must contain the following 13 sections (target length: 450–750 lines):

1. **Title & Metadata Table**
2. **Table of Contents**
3. **1. What & Why**: Formal definition, historical problem statement, real-world justification.
4. **2. Core Concepts**: Essential theoretical and practical mental models.
5. **3. How It Works Internally**:
   - Parsing algorithm state machine, DOM construction, error recovery modes.
   - Accessibility Tree (AOM) mapping and rendering pipeline stages.
   - Clear ASCII or Mermaid diagram.
6. **4. Syntax & Usage**: Formal syntax, attributes, Baseline status box.
7. **5. Practical Examples**: Exactly 5 progressive, production-grade examples:
   - Example 1: Basic / Foundational
   - Example 2: Real-World Enterprise Scenario
   - Example 3: Edge Case / Subtle Interaction
   - Example 4: Failure Mode & Error Recovery (what goes wrong, why it breaks, and the fix)
   - Example 5: Production-Grade / Fully Accessible Implementation
   - Each example must have Goal, Code snippet, Real Validator / DOM / A11y Output, and Explanation.
8. **6. Best Practices**: 5+ actionable, production-proven engineering guidelines.
9. **7. Common Mistakes & Pitfalls**: Structured table with at least 6 distinct rows (`Symptom`, `Root Cause`, `Correct Approach`).
10. **8. Performance, Accessibility, SEO & Security Notes**:
    - Performance (layout shifts, paint blocking, Core Web Vitals)
    - Accessibility (WCAG 2.2 criteria, ARIA, screen reader behavior)
    - SEO (crawling, indexing, semantic landmarks)
    - Security (XSS prevention, iframe isolation, tabnabbing, CSP)
11. **9. Comparison / When to Use What (and when NOT)**: Comparison matrix and trade-off analysis.
12. **10. Real-World Interview Questions**: Exactly 8 deep interview questions with detailed, technical answers inside `<details>` blocks.
13. **11. Practice Exercises**: Exactly 6 hands-on exercises (2 Easy, 2 Medium, 2 Hard) with verified solutions inside `<details>` blocks.
14. **12. Summary / Cheat Sheet**: High-density reference summary.
15. **13. Further Reading**: Verified external links (WHATWG Living Standard, MDN Web Docs, W3C WAI).
16. **Footer Navigation**: `[Previous](...) | [Section Index](README.md) | [Next](...)`

---

## 4. Cross-Dialect & Browser Engine Box Format

Whenever documenting HTML elements, attributes, or APIs with engine variances or emerging Baseline status, use the standardized comparison box:

```
+----------------------------------------------------------------------------------------------------+
| SPEC LEVEL & BROWSER ENGINE SUPPORT MATRIX                                                        |
+-------------------+--------------------------------------------------------------------------------+
| Baseline Status   | Baseline: Widely Available / Newly Available / Limited Availability            |
+-------------------+--------------------------------------------------------------------------------+
| Chromium (Blink)  | Chrome 116+, Edge 116+, Opera 102+, Android WebView                            |
+-------------------+--------------------------------------------------------------------------------+
| Gecko (Firefox)   | Firefox 125+, Firefox for Android                                              |
+-------------------+--------------------------------------------------------------------------------+
| WebKit (Safari)   | Safari 17.0+, Safari on iOS 17.0+                                              |
+-------------------+--------------------------------------------------------------------------------+
| Fallback / Polyfill| Recommended progressive enhancement or graceful degradation strategy           |
+-------------------+--------------------------------------------------------------------------------+
```

---

## 5. Verification Rules

1. All examples must be verified through the scratch validation toolchain.
2. Real validator, DOM tree, and accessibility outputs must be pasted directly into notes.
3. No fabricated numbers, benchmarks, or Lighthouse scores.
4. If a feature is experimental or not widely available, state the required browser version and progressive enhancement fallback clearly.
