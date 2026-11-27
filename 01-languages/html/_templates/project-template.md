# Project Template: Production Reference Architecture

Every project in `10-projects/` consists of a project directory containing exactly 4 markdown specification files:

1. `README.md`: Project overview, architectural scope, requirements, WCAG compliance targets, and structure.
2. `01-design.md`: Information architecture, component breakdown, accessible hierarchy, and responsive layout wireframes (ASCII diagrams).
3. `02-html.md`: Complete, production-grade markup with every file presented in fenced code blocks (`<!DOCTYPE html>...`).
4. `03-validation-and-a11y-results.md`: Real Nu HTML Checker validation logs, axe-core audit outputs, keyboard navigation audit, and screen reader announcements.
5. `04-limitations-and-next-steps.md`: Production trade-offs, progressive enhancement fallbacks, future web platform enhancements (e.g. Invokers, customizable select), and performance metrics.

---

## Project Specification Structure

```
10-projects/<project-name>/
├── README.md
├── 01-design.md
├── 02-html.md
├── 03-validation-and-a11y-results.md
└── 04-limitations-and-next-steps.md
```

### Constraints:
- Markdown-only: All HTML source code lives in fenced code blocks inside `02-html.md`.
- No raw `.html`, `.css`, or `.js` files stored in the repository.
- Verification must be run in the scratch directory outside the repo, with real outputs recorded in `03-validation-and-a11y-results.md`.
