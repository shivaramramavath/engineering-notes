# Next.js Track Changelog

All notable changes, structural migrations, and note creations across `02-frontend/nextjs` are documented in this file.

## [2026-11-24] - Track Audit, Hygiene, & Architectural Standardization

### Added
- Created `NEXT_FACTS.md` with verified Next.js 15.2+/16 facts, CVE details (CVE-2025-29927, CVE-2025-55184), async request APIs, `"use cache"`, `after()`, `proxy.ts`, and `next/form`.
- Created `NEXT_DECISIONS.md` recording ADRs 001 - 004.
- Created `NEXT_AUDIT.md` mapping file-by-file status, line counts, and gaps across all 27 modules.
- Created `NEXT_COMPLETION_PLAN.md` with detailed batch milestones.
- Created `NEXT_PROGRESS.md` resumable tracking checklist.
- Derived and created authoritative templates:
  - `02-frontend/nextjs/_templates/concept.md` (13 mandatory sections with Next.js/React/Node/Router metadata fields).
  - `02-frontend/nextjs/_templates/project-template.md` (5-part architecture and reproduction specification).

### Fixed & Standardized
- Resolved 11 stray duplicate README files (`README (1).md` and `README (2).md`) across modules `00-setup`, `02-routing`, `03-components`, `04-rendering`, `05-data-fetching`, `07-server-actions`, `08-route-handlers-and-proxy`, `09-styling-and-assets`, `10-state-management`, `11-authentication`, and `12-database` by standardizing them cleanly to `README.md` via `git mv`.
