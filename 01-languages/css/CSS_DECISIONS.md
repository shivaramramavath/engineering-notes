# CSS Learning Track Architectural Decisions (CSS_DECISIONS.md)

**Initialized:** 2026-11-26  
**Status:** Active  

This document logs all architectural decisions, track organization rules, and technical trade-offs adopted across `01-languages/css/`.

---

## 1. Directory Location & Deprecation of `02-frontend/css/`
- **Decision**: The canonical CSS curriculum lives in `01-languages/css/`, placed alongside `html/`, `javascript/`, `typescript/`, `python/`, `java/`, and `sql/`.
- **Rationale**: CSS is a fundamental web programming language with its own formal grammar, execution model, type system (`@property`), and specifications.
- **Handling of `02-frontend/css/`**: The existing directory `02-frontend/css` is completely empty. We keep it untouched to avoid breaking any external path references, but document that `01-languages/css/` is the sole authoritative location.

## 2. Strictly Markdown-Only Output
- **Decision**: No raw `.css`, `.html`, `.js`, or `.json` files are committed to the repository.
- **Rationale**: The engineering notes repository is structured for reading, searching, and technical reviewing in markdown formats. All code snippets are stored in fenced markdown blocks (`css`, `html`, `javascript`).

## 3. Playwright Runner in Scratch Directory
- **Decision**: Scratch scripts, DOM rendering harnesses, and stylelint testing files reside strictly outside the repository in the user's conversation scratch directory: `C:\Users\RAMAVATH SHIVA RAM\.gemini\antigravity\brain\a9bfc182-e985-4a5d-9df0-ebf568360240\scratch\css_runner.js`.
- **Rationale**: Isolates Node.js execution environments and prevents temporary test fixtures from cluttering git tracking.

## 4. Modern Web Baseline as the Benchmark
- **Decision**: Use Web Platform Baseline (2024–2026) as the primary benchmark. Features that are Widely Available (such as Cascade Layers, `:has()`, CSS Nesting, Container Queries, Subgrid, and `oklch()`) are taught as standard primary techniques.
- **Handling of Emerging Features**: Non-baseline features (like `display: grid-lanes`, `if()`, and `text-wrap: pretty`) are clearly labeled as "Limited Availability" with explicit `@supports` fallback strategies.

## 5. Explicit Flagging of Historical Anti-Patterns
- **Decision**: The curriculum actively identifies and deconstructs obsolete legacy techniques (float layouts, clearfix hacks, `!important` abuse, ID selectors, 100vh bugs) and contrasts them with idiomatic modern solutions.
