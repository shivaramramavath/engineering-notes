# Module 10: Architecture & Methodologies (`10-architecture-and-methodologies/`)

| Metadata | Details |
| :--- | :--- |
| **Track** | CSS Curriculum |
| **Module** | 10 — Architecture & Methodologies |
| **Spec Level** | Modern CSS Architectural Patterns & Design System Paradigms |
| **Prerequisites** | Modules 00 through 09 |

---

## Executive Overview

CSS is remarkably easy to write in small isolated projects, but notoriously difficult to maintain at enterprise scale across multi-team codebases. Without disciplined architecture, stylesheets suffer from:
- Specificity wars and cascading side-effects where changing one class breaks layouts across unrelated pages.
- Uncontrolled file growth where developers add new `!important` overrides rather than modifying legacy rules out of fear of regression.
- Inconsistent design tokens, fragmented color palettes, and duplicate media query definitions.

Over the past two decades, the software engineering industry developed a series of battle-tested methodologies, naming conventions, and structural paradigms to solve these challenges. Today, modern CSS features (like Cascade Layers `@layer`, native Nesting, `@scope`, and Design Tokens) allow teams to achieve pristine modular architecture directly in standard CSS without relying on monolithic framework runtimes.

This module provides an exhaustive, production-grade guide to CSS architecture—from foundational principles (BEM, OOCSS, SMACSS, ITCSS) and utility-first philosophies (Tailwind) to modern CUBE CSS, design token pipelines, scoped CSS, CSS-in-JS trade-offs, and managing specificity at massive scale.

---

## Module Syllabus

| # | Note | Core Methodologies & Architectural Standards |
|:---|:---|:---|
| 01 | [01-css-architecture-principles.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/01-css-architecture-principles.md) | Predictability, maintainability, scalability, and reusability pillars. Separation of concerns vs co-location. |
| 02 | [02-bem-oocss-smacss-itcss.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/02-bem-oocss-smacss-itcss.md) | Block-Element-Modifier (BEM), Object-Oriented CSS (OOCSS), SMACSS categorization, and Inverted Triangle CSS (ITCSS). |
| 03 | [03-utility-first-and-tailwind-overview.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/03-utility-first-and-tailwind-overview.md) | Utility-first philosophy, composability, JIT compilation, trade-offs, and native CSS utility design. |
| 04 | [04-cube-css-and-modern-methodologies.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/04-cube-css-and-modern-methodologies.md) | CUBE CSS (Composition, Utility, Block, Exception), Andy Bell methodology, embracing the cascade. |
| 05 | [05-naming-and-file-organisation.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/05-naming-and-file-organisation.md) | Directory structures, component folders, 7-1 pattern, barrel files, naming semantic conventions. |
| 06 | [06-design-tokens-and-theming.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/06-design-tokens-and-theming.md) | Design Token tiers (Global, Semantic, Component), W3C Design Tokens Community Group (DTCG), Style Dictionary. |
| 07 | [07-dark-mode-and-multi-theme.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/07-dark-mode-and-multi-theme.md) | Multi-tenant theming, `light-dark()`, custom property cascades, local subtree theme inversions. |
| 08 | [08-resets-and-normalisation.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/08-resets-and-normalisation.md) | Eric Meyer Reset vs Normalize.css vs modern zero-specificity `@layer` and `:where()` resets. |
| 09 | [09-css-modules-and-scoped-css.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/09-css-modules-and-scoped-css.md) | CSS Modules compilation, hashed classes, `composes`, Vue/Svelte scoped styles, and native `@scope` migration. |
| 10 | [10-css-in-js-overview.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/10-css-in-js-overview.md) | Styled Components, Emotion, runtime performance costs, zero-runtime CSS (Vanilla Extract, Panda CSS, StyleX). |
| 11 | [11-sass-less-vs-native-css.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/11-sass-less-vs-native-css.md) | Preprocessor features absorbed into native CSS (nesting, variables, math, color-mix) vs lingering build-step needs. |
| 12 | [12-bootstrap-and-component-frameworks-compared.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/12-bootstrap-and-component-frameworks-compared.md) | Bootstrap, Bulma, Tailwind, Open Props, DaisyUI architectural comparison and selection matrix. |
| 13 | [13-managing-specificity-at-scale.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/13-managing-specificity-at-scale.md) | Specificity graphing, flattening specificity, `@layer` architecture, `:where()` sanitization, avoiding `!important`. |
| 14 | [14-dead-css-and-critical-css.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/14-dead-css-and-critical-css.md) | PurgeCSS, UnCSS, tree-shaking, inlining Critical Path CSS, and DevTools CSS Coverage auditing. |
| 15 | [15-component-styling-patterns.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/15-component-styling-patterns.md) | Slot-based components, container query components, composition over inheritance, and resilient CSS APIs. |

---

## Suggested Study Path

1. **Methodology Evolution**:
   - Start with `01-css-architecture-principles.md` and `02-bem-oocss-smacss-itcss.md` to understand how the web solved specificity conflicts before native scoping.
   - Study `03-utility-first-and-tailwind-overview.md` and `04-cube-css-and-modern-methodologies.md` to master contemporary utility and compositional approaches.
2. **Design Systems & Token Foundations**:
   - Master `06-design-tokens-and-theming.md` and `07-dark-mode-and-multi-theme.md` to structure 3-tier token hierarchies.
   - Refactor baseline resets with `08-resets-and-normalisation.md`.
3. **Tooling & Framework Trade-offs**:
   - Analyze modular scoping paradigms in `09-css-modules-and-scoped-css.md` and `10-css-in-js-overview.md`.
   - Transition from legacy preprocessors using `11-sass-less-vs-native-css.md`.
4. **Scale & Production Performance**:
   - Deep dive into `13-managing-specificity-at-scale.md` and `14-dead-css-and-critical-css.md` to prevent stylesheet bloat and maintain sub-50ms render times.

---

### Navigation
- **Previous Module**: [../09-modern-css/README.md](file:///e:/Learning/engineering-notes/01-languages/css/09-modern-css/README.md)
- **Next Note**: [01-css-architecture-principles.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/01-css-architecture-principles.md)
