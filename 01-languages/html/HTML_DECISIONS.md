# HTML Track Engineering Decisions (HTML_DECISIONS.md)

| Date | Decision | Rationale | Impact |
|:---|:---|:---|:---|
| 2026-11-26 | Canonical track location at `01-languages/html/` | User requested HTML track in `01-languages/html/` alongside Java, Python, and SQL. | Primary directory established. |
| 2026-11-26 | Re-link `02-frontend/html/` and existing tracks | `02-frontend/html/` was empty; cross-link from frontend, CSS, JS, React, and backend tracks. | No duplication; clean cross-references. |
| 2026-11-26 | Markdown-only repository enforcement | Repository strict rule: zero raw `.html`, `.css`, `.js`, `.json` files in the git tree. | All source code in fenced blocks. |
| 2026-11-26 | External scratch verification runner | Verification runner `scratch/html_runner.js` lives outside the repo in `<artifactDir>/scratch/`. | Keeps repository clean; ensures real validator logs. |
| 2026-11-26 | Web Platform Baseline integration | Categorize every HTML feature as Baseline Widely Available, Baseline Newly Available, or Limited. | Modern, authoritative web platform tracking. |
| 2026-11-26 | 13-Module canonical curriculum structure | Structured into 13 modules (00 through 12) + 11 projects + 8 interview files + 10 cheatsheets. | Comprehensive production-grade coverage. |
| 2026-11-26 | Mandatory 13-section concept note template | Consistency with Java, Python, and SQL tracks: 5 progressive examples, 8 interview Qs, 6 exercises. | Depth standard strictly preserved. |
