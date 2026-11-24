# Architectural Decision Records: ReactJS Track (REACT_DECISIONS.md)

> Record of all key architectural, naming, structural, and tooling decisions made during the React track audit and completion.

---

## ADR 001: Stray README Cleanup & Consolidation
- **Context**: 12 duplicate/stray README files existed with parenthesized collision names: `README (2).md`, `README (3).md`, etc. in modules 04, 05, 06, 08, 09, 10, 11, 12, 16, 17, 19, 20. In module 10, both `README.md` and `README (5).md` existed.
- **Decision**: Audited content; canonical `README.md` preserved and extended with complete syllabus tables. All 12 stray collision files permanently removed.
- **Status**: Implemented.

---

## ADR 002: Authoritative Templates Specification
- **Context**: `_templates/concept.md` was empty (0 bytes), and no `project-template.md` existed.
- **Decision**: Created canonical 13-section `_templates/concept.md` and feature-sliced `_templates/project-template.md` tailored specifically for React 19, Vite 6, TypeScript 5.7, Vitest, and modern UI tooling.
- **Status**: Implemented.

---

## ADR 003: Baseline Runtime and API Standards
- **Context**: React 19 is now Generally Available (shipped Dec 2024), bringing breaking changes (`ref` as prop, deprecation of `forwardRef`, native metadata tags, Actions, `useActionState`, `useFormStatus`, `useOptimistic`, `use()`). Routing ecosystem underwent major merger (Remix $\to$ React Router v7). TanStack Query v5 replaced `{ suspense: true }` with dedicated suspense hooks.
- **Decision**: Set baseline to **React 19.0.0+**. All notes teach modern React 19 idioms first, explicitly highlighting differences from React 17 and 18. Deprecated patterns (CRA, `forwardRef`, manual `useMemo` when compiler is enabled, propTypes) are flagged as legacy anti-patterns.
- **Status**: Implemented.

---

## ADR 004: Markdown-Only In-Repository Policy
- **Context**: User requires 100% markdown-only files inside `02-frontend/reactjs/`. No source files (`.tsx`, `.ts`, `.json`), build configs, or node_modules inside the repository.
- **Decision**: All runnable code, tests, configurations, and scripts live in fenced code blocks inside `.md` notes. Real verification is executed via scratch directories (`scratch/react_verify/`) using real `tsc`, `vitest`, and `vite build`.
- **Status**: Implemented.

---

## ADR 005: Library Index Expansion
- **Context**: `_libraries/` previously contained only `socket.io-react-hook.md`.
- **Decision**: Standardize and write concise reference notes for all 18 core ecosystem libraries: `tanstack-query`, `tanstack-table`, `react-router`, `react-hook-form`, `zod`, `zustand`, `redux-toolkit`, `framer-motion`, `radix-ui`, `shadcn-ui`, `tailwind`, `dnd-kit`, `recharts`, `msw`, `testing-library`, `storybook`, `date-fns`, `i18next`.
- **Status**: Planned in Phase 2.
