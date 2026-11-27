# HTML Track Master Completion Plan (HTML_COMPLETION_PLAN.md)

**Curriculum Scope:** `01-languages/html/`  
**Governing Standard:** WHATWG HTML Living Standard (2026 Baseline)  
**Total Target Modules:** 13 (00 through 12) + Track Root Files

---

## 1. Complete Syllabus & Module Index

| Module | Title | Target Notes / Files | Core Capabilities Covered |
|:---|:---|:---:|:---|
| **Track Root** | Track Governance & Index | 12 files | README, INDEX, ROADMAP, GLOSSARY, CONTRIBUTING, FACTS, CONVENTIONS, AUDIT, DECISIONS, PLAN, PROGRESS, CHANGELOG |
| **`00-setup/`** | Setup, DevTools & Browser Engine Architecture | 6 notes + README | Parser, DOM tree, CSSOM, Render Tree, Paint, History, Living Standard, Validators, DevTools |
| **`01-fundamentals/`** | Document Structure & Core Syntax | 9 notes + README | DOCTYPE, syntax, parser error recovery, text content, lists, links, images, encoding, global attributes (`inert`, `popover`, `hidden`) |
| **`02-semantics-and-structure/`** | Semantic HTML & Content Architecture | 7 notes + README | Semantics, landmarks, heading outlines, grouping elements, exclusive accordions (`details[name]`), accessible tables, element chooser guide |
| **`03-forms/`** | Forms, Input Modalities & Validation | 15 notes + README | Form basics, input types, mobile keyboards (`inputmode`, `enterkeyhint`), controls, Constraint Validation API, FormData, a11y, security, popover forms |
| **`04-media-and-embeds/`** | Media, Responsive Images & Embeds | 11 notes + README | Audio/Video, responsive images (`srcset`, `<picture>`), iframes & sandbox, SVG, Canvas, MathML, fonts & preload, priority hints (`fetchpriority`) |
| **`05-head-metadata-and-seo/`** | Head Metadata, Social Cards & SEO | 11 notes + README | Head anatomy, Open Graph, Twitter cards, JSON-LD, resource hints (`preload`, `prefetch`, `preconnect`), script loading (`defer`, `async`, import maps), CSP |
| **`06-accessibility/`** | Accessibility (a11y) & WCAG 2.2 | 15 notes + README | WCAG 2.2, ARIA rules, AOM (Accessibility Tree), keyboard focus, skip links, alt text, accessible forms/tables/dialogs, live regions, testing (axe) |
| **`07-modern-html-and-apis/`** | Modern HTML Elements & APIs | 14 notes + README | `<dialog>`, Popover API, `<details name="...">`, Declarative Shadow DOM, Invokers (`commandfor`), Customizable select, Speculation Rules API |
| **`08-performance-and-security/`** | Performance, Security & Sanitization | 10 notes + README | Critical Rendering Path, Preload Scanner, `fetchpriority`, XSS escaping, CSP, `rel="noopener"`, Clickjacking (`frame-ancestors`), CSRF, HTML email limits |
| **`09-patterns-and-architecture/`** | Real-World Component & Page Patterns | 8 notes + README | Layout skeletons (landing, docs, app shell), component patterns (modals, accordions, tabs), progressive enhancement, SSR templating, i18n (`dir`/`lang`) |
| **`10-projects/`** | Production Reference Architectures | 11 projects (44 files) + README | Portfolio, Blog, Landing Page, Docs, Accessible Signup Form, Restaurant Menu Table, Resume, Email Template, Dashboard, Pricing Page, Multilingual Site |
| **`11-interview/`** | Technical Interview Q&A | 8 notes + README | HTML basics, Semantics & A11y, Forms, SEO, Performance & Security, Parsing & Output, Code challenges, Architecture discussion |
| **`12-cheatsheets/`** | Operational Cheatsheets | 10 cheatsheets + README | Elements by purpose, Attributes reference, Global attributes, Forms & inputs, Meta & link tags, ARIA quick ref, Entities, Element chooser, A11y checklist, Validation errors |

---

## 2. Execution Batches

- **Batch 1 (Track Setup & Module 00)**: Governance files, templates, Module 00 README + 6 notes.
- **Batch 2 (Module 01 Fundamentals)**: Module 01 README + 9 notes (01 through 09).
- **Batch 3 (Module 02 Semantics & Structure)**: Module 02 README + 7 notes (01 through 07).
- **Batch 4 (Module 03 Forms & Controls)**: Module 03 README + 15 notes (01 through 15).
- **Batch 5 (Module 04 Media & Embeds)**: Module 04 README + 11 notes (01 through 11).
- **Batch 6 (Module 05 Head Metadata & SEO)**: Module 05 README + 11 notes (01 through 11).
- **Batch 7 (Module 06 Accessibility & WCAG 2.2)**: Module 06 README + 15 notes (01 through 15).
- **Batch 8 (Module 07 Modern HTML & APIs)**: Module 07 README + 14 notes (01 through 14).
- **Batch 9 (Module 08 Performance & Security)**: Module 08 README + 10 notes (01 through 10).
- **Batch 10 (Module 09 Patterns & Architecture)**: Module 09 README + 8 notes (01 through 08).
- **Batch 11 (Module 10 Projects)**: 11 production reference projects (44 files).
- **Batch 12 (Module 11 Interview & Module 12 Cheatsheets)**: 8 interview notes + 10 cheatsheets.
- **Batch 13 (Whole-Track Verification & Final Report)**: Verification script, tree generation, `HTML_FINAL_REPORT.md`.

---

## 3. Extra Expert Topics Included

1. **Parser Tokenization & Insertion Modes**: The 80+ state WHATWG tokenizer state machine, tree construction dispatcher, and foster parenting.
2. **Declarative Shadow DOM & SSR Hydration**: How DSD eliminates client-side Flash of Unstyled Content (FOUC) and enables server-rendered Web Components.
3. **Speculation Rules API & Document Prerendering**: JSON-driven prefetching and prerendering replacing legacy `<link rel="prerender">`.
4. **Command & Invoker Attributes (`commandfor`, `command`)**: The emerging zero-JS declarative command pattern for buttons, dialogs, and popovers.
5. **Customizable Select & `<selectedcontent>`**: Open UI standard for rich styling of native select triggers.
6. **Accessible Name and Description Computation (AccName 1.2)**: Exact algorithmic steps screen readers use to resolve element names.
7. **HTML Sanitization & Safe DOM Parsing**: `setHTMLUnsafe()`, `parseHTMLUnsafe()`, and preventing DOM XSS.
8. **Responsive Picture & Art Direction Mechanics**: DPR-based resolution switching vs layout-driven art direction using `<picture>` and media queries.
