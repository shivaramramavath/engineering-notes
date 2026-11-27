# Contributing to the HTML Learning Track (CONTRIBUTING.md)

Thank you for your interest in contributing to the **HTML Living Standard & Web Platform** engineering track.

This track is an authoritative, production-grade reference. All contributions must maintain the exact formatting, depth, and rigorous technical standards outlined below.

---

## 1. Governance & Standards Adherence

1. **Governing Standard**: All markup patterns, attributes, and APIs must conform to the **WHATWG HTML Living Standard**.
2. **Baseline Classification**: All elements and attributes must be labeled with their current **Web Platform Baseline** status:
   - **Baseline: Widely Available**: Universally supported for 30+ months across Chrome, Edge, Firefox, and Safari.
   - **Baseline: Newly Available**: Supported across all major engines within the last 30 months.
   - **Limited Availability**: Supported in only 1 or 2 engines (e.g. Speculation Rules, customizable select); must document explicit progressive enhancement fallbacks.
3. **Strictly Markdown-Only**:
   - The repository contains only `.md` files.
   - Never commit raw `.html`, `.css`, `.js`, or image files into the repo. All examples must be in syntax-highlighted fenced code blocks.

---

## 2. Note Structure Requirements

Every concept note must adhere strictly to the **13-section concept note template** defined in [`_templates/concept-template.md`](_templates/concept-template.md):
- Target length: **450 to 750 lines** of clear, technical prose.
- Metadata table with Level, Prerequisites, Time, Status, Baseline Status, and Tested With.
- Exactly **5 progressive practical examples**:
  1. Basic / Foundational
  2. Real-World Enterprise Scenario
  3. Edge Case / Subtle Interaction
  4. Failure Mode & Error Recovery (with root cause analysis)
  5. Production-Grade / Fully Accessible Implementation
- Real validator and DOM/AOM output pasted directly into each example.
- Best Practices list (5+ actionable guidelines).
- Structured Common Mistakes table (6+ distinct rows).
- Performance, Accessibility (WCAG 2.2 Level AA), SEO, and Security notes.
- Comparison matrix and decision guide.
- Exactly **8 technical interview questions** with detailed answers in `<details>` blocks.
- Exactly **6 hands-on practice exercises** (2 Easy, 2 Medium, 2 Hard) with verified solutions in `<details>` blocks.
- Summary and verified external reading links (WHATWG, MDN, W3C WAI).

---

## 3. Verification Protocol

Before submitting or updating notes:
1. Run all HTML code blocks through the verification runner located outside the repository:
   ```bash
   node scratch/html_runner.js path/to/snippet.html
   ```
2. Verify that documents pass:
   - W3C Nu HTML Checker validation rules (valid DOCTYPE, `html[lang]`, `meta[charset]`, `<title>`, properly closed tags).
   - WCAG 2.2 accessibility rules (valid `alt` text, accessible form labels, skip links, semantic landmarks).
   - Security best practices (no inline event handlers, `rel="noopener noreferrer"` on `target="_blank"`).
3. Paste the actual validator outputs into the note. Never invent or fake validator outputs or performance metrics.

---

## 4. Submitting Changes

1. Open a pull request targeting the main branch.
2. Ensure `HTML_PROGRESS.md` is updated to reflect any completed, draft, or modified notes.
3. Append a detailed entry to `HTML_CHANGELOG.md` documenting what changed, why, and the governing specification version.
