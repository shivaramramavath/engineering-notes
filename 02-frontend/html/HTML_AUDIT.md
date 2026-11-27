# HTML Track Audit Report (HTML_AUDIT.md)

**Audit Date:** 2026-11-26  
**Auditor:** HTML Curriculum Engineer & Technical Reviewer  
**Location:** `01-languages/html/` (Repo root: `E:\Learning\engineering-notes`)

---

## 1. Initial State Assessment

Prior to this initiative, the repository contained:
- **`01-languages/html/`**: Did not exist.
- **`02-frontend/html/`**: Empty directory with zero files.
- **`02-frontend/reactjs/08-accessibility/00-semantic-html.md`**: Single React-specific accessibility overview (cross-linked to the new canonical HTML track).

**Conclusion**: Build the entire HTML track from the ground up at `01-languages/html/` to match the depth, quality, and rigorous 13-section standards of the Java, Python, and SQL tracks.

---

## 2. Scope & Standards Compliance

The target curriculum is structured into **13 cohesive modules** (00 through 12):
- **00-setup**: Architecture, Web Standards, Rendering Pipeline, DevTools, Validators.
- **01-fundamentals**: Document structure, syntax rules, text content, lists, links, images, encoding, global attributes.
- **02-semantics-and-structure**: Semantic HTML, landmarks, heading outlines, grouping, exclusive accordions, accessible tables.
- **03-forms**: Full form controls, modern inputs, constraint validation API, FormData, accessibility, popover forms.
- **04-media-and-embeds**: Audio, video, responsive images (`srcset`, `picture`), iframes, SVG, Canvas, MathML.
- **05-head-metadata-and-seo**: Head anatomy, Open Graph, JSON-LD, resource hints, script loading (`defer`/`async`/import maps), CSP.
- **06-accessibility**: WCAG 2.2, ARIA rules, accessibility tree, keyboard focus, accessible forms/tables/dialogs, testing.
- **07-modern-html-and-apis**: `<dialog>`, Popover API, `<details name="...">`, Declarative Shadow DOM, Invokers (`commandfor`), Customizable select.
- **08-performance-and-security**: Critical rendering path, preload scanner, priority hints (`fetchpriority`), XSS prevention, CSP, sandboxing.
- **09-patterns-and-architecture**: Layout skeletons, component markup, progressive enhancement, SSR templating, i18n (`dir`/`lang`).
- **10-projects**: 11 reference production projects (4-file specification in `.md` format).
- **11-interview**: 8 deep technical interview Q&A modules.
- **12-cheatsheets**: 10 operational cheatsheets.

---

## 3. Toolchain & Verification Status

- **Runner Script**: `scratch/html_runner.js` created and tested in scratch directory outside the repository.
- **Verification Capabilities**:
  - Full document conformance: DOCTYPE, `html[lang]`, `meta[charset]`, `title`, `meta[viewport]`.
  - Obsolete element detection: `<center>`, `<font>`, `<marquee>`, `<strike>`, `<big>`, etc.
  - Accessibility integrity: `<img>` alt attribute enforcement, `<label for>` association, heading order verification.
  - Security checks: `target="_blank"` with `rel="noopener noreferrer"`, inline event handler avoidance.
  - W3C Nu HTML Checker and axe-core compatibility.
