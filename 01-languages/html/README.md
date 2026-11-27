# HTML Living Standard Engineering Track

Welcome to the comprehensive, production-grade **HTML Living Standard & Modern Web Platform** learning track.

This track is an exhaustive, standards-first reference and practical curriculum covering modern HTML as specified by the **WHATWG HTML Living Standard** and tracked by the **Web Platform Baseline (2024–2026)**.

---

## Track Structure Overview

```
01-languages/html/
├── README.md                      # Track overview & entry point
├── INDEX.md                       # Master alphabetical and categorized topic index
├── ROADMAP.md                     # 5 tailored learning paths with time estimates
├── GLOSSARY.md                    # Exhaustive web platform & HTML terminology
├── CONTRIBUTING.md                # Style guide & contribution standards
├── HTML_FACTS.md                  # Verified Baseline statuses & specification facts
├── HTML_CONVENTIONS.md            # Metadata and 13-section template standards
├── HTML_AUDIT.md                  # Track audit logs
├── HTML_DECISIONS.md              # Architectural decisions log
├── HTML_COMPLETION_PLAN.md        # Comprehensive curriculum syllabus
├── HTML_PROGRESS.md               # Resumable progress checklist
├── HTML_CHANGELOG.md              # Chronological change log
├── _templates/                    # Note and project markdown templates
├── 00-setup/                      # Architecture, Parsing, DOM, CSSOM & DevTools
├── 01-fundamentals/               # Document Structure, Syntax, Links, Images & Encoding
├── 02-semantics-and-structure/    # Landmarks, Outlines, Accordions & Tables
├── 03-forms/                      # Modern Form Controls, Validation API & Security
├── 04-media-and-embeds/           # Responsive Media, Iframes, SVG, Canvas & MathML
├── 05-head-metadata-and-seo/      # Head Metadata, Open Graph, JSON-LD & Resource Hints
├── 06-accessibility/             # WCAG 2.2, ARIA, Focus Management & AOM
├── 07-modern-html-and-apis/       # Dialog, Popover, Invokers, DSD & Speculation Rules
├── 08-performance-and-security/   # Critical Rendering Path, CSP, XSS & Priority Hints
├── 09-patterns-and-architecture/  # Layout Skeletons, Component Patterns & SSR
├── 10-projects/                   # 11 Production Reference Architectures
├── 11-interview/                  # Technical Interview Preparation & Challenges
└── 12-cheatsheets/                # Operational Quick References & Checklists
```

---

## Core Engineering Principles

1. **Semantic First**: Always leverage native HTML semantics before reaching for `<div>`, `<span>`, or ARIA attributes.
2. **Accessible by Default**: Every document and interface is built to satisfy **WCAG 2.2 Level AA** criteria.
3. **Living Standard Baseline**: Features are documented with verified Web Platform Baseline status (Widely Available, Newly Available, or Limited Availability with progressive fallbacks).
4. **Parser Determinism**: Deep focus on how browser engines (Blink, Gecko, WebKit) tokenize, build DOM trees, execute error recovery, and map to the Accessibility Tree.
5. **Strictly Markdown-Only**: All code snippets, configurations, and reference implementations reside in verified, syntax-highlighted code blocks within `.md` documents.

---

## Cross-Track Integration

- **CSS Track**: [`../../02-frontend/css/`](../../02-frontend/css/) for styling, CSS Grid, Flexbox, Container Queries, and CSSOM manipulation.
- **JavaScript Track**: [`../javascript/`](../javascript/) for DOM APIs, event listeners, dynamic client hydration, and Fetch.
- **TypeScript Track**: [`../typescript/`](../typescript/) for typed DOM elements, form validation schemas, and web component types.
- **React Track**: [`../../02-frontend/reactjs/`](../../02-frontend/reactjs/) for JSX compilation, virtual DOM reconciliation, and SSR hydration.
- **Backend Tracks**: [`../../03-backend/`](../../03-backend/) for server-side HTML templating (EJS, Thymeleaf, Jinja2, Blade) and CSRF protection.
