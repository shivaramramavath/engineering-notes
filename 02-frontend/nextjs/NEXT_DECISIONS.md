# Next.js Architecture Decisions Record (ADR)

> This document records architectural, structural, and naming decisions made during the overhaul of the Next.js curriculum.

---

## ADR 001: Standardization of Stray `README (N).md` Files
- **Date**: 2026-11-24
- **Context**: 11 folders contained duplicate or improperly named READMEs (`README (1).md` in 00-setup, 02-routing, 03-components, 04-rendering, 05-data-fetching, 07-server-actions, 08-route-handlers-and-proxy, 09-styling-and-assets, 10-state-management, 11-authentication, and `README (2).md` in 12-database).
- **Decision**: Renamed all 11 files cleanly to `README.md` across their respective directories using git tracking (`git mv`). All 27 modules now have standard `README.md`.

## ADR 002: Framework Baseline & Version Tagging
- **Date**: 2026-11-24
- **Decision**: Next.js 15.2+ / 16.0 canary line is established as the primary baseline, with React 19.0.0 (GA).
- **Conventions**:
  - Request APIs (`cookies()`, `headers()`, `params`, `searchParams`) are treated as asynchronous `Promise`s.
  - Caching is taught as dynamic-by-default with `"use cache"`, `cacheLife`, `cacheTag`, `updateTag`, and `revalidateTag(tag, profile)`.
  - The network interceptor is documented as `proxy.ts` (Next.js 16) with explicit notes on the deprecated `middleware.ts` naming.
  - `next/form` is standard for search navigation and server actions.
  - Non-blocking post-response execution uses `after()`.
  - Authentication errors throw `unauthorized()` and `forbidden()` with custom interrupt UI.

## ADR 003: Authoritative Template Definition in `_templates/`
- **Date**: 2026-11-24
- **Decision**: Derived `_templates/concept.md` (13 mandatory sections with Next.js metadata fields: Next.js version, React version, Node version, Router type, Runtime, Tested With) and `_templates/project-template.md` (5-part architecture and reproduction structure).

## ADR 004: Strict Markdown-Only Repository Policy
- **Date**: 2026-11-24
- **Decision**: Zero non-markdown files inside `02-frontend/nextjs/`. All project implementations, configurations, Docker compose files, schemas, and tests are presented in structured markdown code blocks under explicit file path headings. Verification is performed in scratch workspaces outside the repository.
