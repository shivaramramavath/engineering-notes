# Frontend Architecture — Senior Interview Questions & Deep Dives

> Feature-Sliced Design (FSD), Monorepo scaling, micro-frontends (Module Federation), Design System governance, and enterprise CI/CD gates.

---

## Question 1: How does Feature-Sliced Design (FSD) solve architectural decay in massive frontend codebases?

### The Core Problem with Layer-Based Architectures
In naive setups (`components/`, `hooks/`, `services/`), files are grouped by technical type rather than business domain.
- Components in `components/` cross-import services and hooks arbitrarily.
- Circular dependencies develop, making it impossible to isolate, delete, or refactor a single product feature without breaking unrelated pages.

### The FSD Layered Hierarchy
Feature-Sliced Design enforces strict **unidirectional dependency rules**:
```
app ──> processes ──> pages ──> widgets ──> features ──> entities ──> shared
```
1. **App**: Routing, global providers, root configuration.
2. **Pages**: Route-level views composing widgets.
3. **Widgets**: Self-contained UI compositions (e.g., `Header`, `UserCardList`).
4. **Features**: User interactions delivering business value (e.g., `authByEmail`, `addToCart`).
5. **Entities**: Business domain models (e.g., `user`, `product`, `order`).
6. **Shared**: Reusable UI primitives, generic utilities, API clients.

**Golden Rule**: Modules can only import from layers **strictly below them**. An entity cannot import a feature; a feature cannot import a page.

---

## Question 2: When are Micro-Frontends via Module Federation an anti-pattern versus an operational necessity?

### When Micro-Frontends Are Justified (Operational Scale)
1. **Autonomous Organizational Velocity**: 15+ engineering squads across different global time zones deploying independently multiple times a day without blocked release trains.
2. **Poly-repo Migration**: Gradually migrating legacy monoliths without a high-risk full rewrite.

### When They Are an Anti-Pattern
1. **Premature Optimization**: Small teams (<20 engineers) will spend more time debugging Webpack federation configs, runtime CSS conflicts, shared dependency mismatches, and multi-app orchestration than building user features.
2. **Performance Overhead**: Duplicate dependencies (multiple React or runtime copies) degrade initial page load times and user experience.
