# System Design, Architecture & Behavioural Interview Questions

| Topic | Module | Level | Target Roles | Date | Prerequisites |
|---|---|---|---|---|---|
| CSS System Design & Engineering Leadership | 15: Interview Questions | Senior to Staff / Principal | Principal Architect, Frontend Staff Engineer | 2026-03-31 | Modules 00–14 |

---

## Overview
This guide addresses high-level architectural decisions, system design trade-offs, cross-platform design token scaling, legacy migration strategies, and technical leadership questions asked during Staff/Principal engineering loops.

---

## Technical Questions & Strategic Answers

### Q1: Compare Tailwind CSS vs CSS Modules vs CSS-in-JS (Emotion/Styled-Components) vs Modern Vanilla CSS with `@layer`. Provide an objective architectural decision matrix.
<details>
<summary>View Complete Architectural Answer</summary>

**Answer:**

| Architectural Criteria | Tailwind CSS | CSS Modules | Runtime CSS-in-JS (Styled) | Modern Vanilla CSS (@layer) |
|---|---|---|---|---|
| **Runtime Performance** | **Zero runtime** (Static CSS) | **Zero runtime** (Static CSS) | **High runtime overhead** (Style serialization on render) | **Zero runtime** (Native engine) |
| **Bundle Size Scaling** | **Asymptotic / Flat** (Reuses existing utilities) | Linear (Grows with component count) | Linear + JS runtime library (~15KB) | Modular / Tiered |
| **Server-Side Rendering (SSR)** | Perfect (Zero hydration mismatch) | Perfect | High complexity (Critical CSS extraction) | Perfect |
| **Dynamic Prop Theming** | Relies on inline styles or classes | Requires custom CSS variable bridging | Native JS expressions (`props => ...`) | Native CSS variables (`var()`) |
| **Design System Governance** | Hard to enforce semantic boundaries | Scoped locally to files | Scoped to components | Explicit hierarchy via `@layer` |
| **Future Standards Alignment**| Proprietary DSL syntax | Standard CSS syntax | Non-standard | 100% W3C Baseline Standards |

**Architectural Recommendation**:
- For high-velocity SaaS applications and rapid prototyping: **Tailwind CSS**.
- For enterprise micro-frontends with isolated styling needs: **CSS Modules** or **Modern Vanilla CSS with `@layer`**.
- **Avoid Runtime CSS-in-JS** in modern React 19 / Next.js / Server Component architectures due to fatal SSR hydration bottlenecks and main-thread serialization costs.
</details>

---

### Q2: How would you architect a migration strategy for a 10-year-old, 80,000-line legacy Sass codebase riddled with `!important` and specificity wars?
<details>
<summary>View Complete Architectural Answer</summary>

**Answer:**
A successful migration must be **incremental, non-destructive, and verified by automated testing**:

1. **Phase 1: Automated Visual Regression Baseline**:
   Before touching code, configure Playwright or Percy to snapshot critical user journeys across desktop and mobile. This establishes an objective pixel-diff safety net.
2. **Phase 2: Encapsulate the Legacy Codebase in `@layer legacy`**:
   Wrap the entire existing legacy Sass build into an explicit bottom layer:
   ```css
   @layer legacy, modern;
   @import "legacy-bundle.css" layer(legacy);
   ```
   *The Immediate Win*: Any new code written in `@layer modern` automatically wins over legacy styles, regardless of how many IDs or nested selectors the legacy code used, instantly ending specificity escalation.
3. **Phase 3: Establish Centralized Design Tokens**:
   Extract hard-coded hex colors, spacing values, and font sizes into native CSS custom properties. Use automated codemods (AST grep or PostCSS plugins) to search and replace recurring values across legacy files.
4. **Phase 4: Component-by-Component Strangler Fig Migration**:
   Re-architect UI components one by one into modern modular components (using CSS Modules or modern CSS). Route migrated pages to the modern component layer and decommission corresponding legacy Sass partials.
5. **Phase 5: Automated Governance Safeguards**:
   Add Stylelint rules (`selector-max-id: 0`, `declaration-no-important: true`) to pull request CI/CD gates to prevent regressions from re-entering the codebase.
</details>

---

### Q3: How do you design an enterprise Design Token Architecture that synchronizes across Web, iOS (SwiftUI), and Android (Jetpack Compose)?
<details>
<summary>View Complete Architectural Answer</summary>

**Answer:**
1. **Single Source of Truth (Git-Backed JSON/YAML)**:
   Tokens must never be authored directly in CSS or Swift. They are declared in structured, platform-agnostic JSON (e.g. following the Design Tokens Community Group (DTCG) W3C specification).
2. **Automated Translation Pipeline (Style Dictionary)**:
   Integrate an automated transformation engine (such as Amazon's **Style Dictionary**):
   - **Web Transform**: Exports CSS Custom Properties (`:root { --color-brand: oklch(...); }`) and TypeScript type definitions.
   - **iOS Transform**: Exports Swift structs and SwiftUI Color extensions (`Color("BrandPrimary")`).
   - **Android Transform**: Exports Kotlin Compose objects and XML resources (`val BrandPrimary = Color(...)`).
3. **Semantic Abstraction Layers**:
   Enforce strict token taxonomy:
   - *Global Primitives*: `color.blue.500 = #2563eb` (Engineers never reference this directly).
   - *Semantic Intents*: `color.interactive.primary = {ref: color.blue.500}`.
   - *Component Tokens*: `button.primary.background = {ref: color.interactive.primary}`.
4. **Continuous Deployment (CI/CD)**:
   When designers update tokens in Figma (via Tokens Studio), a GitHub Action commits the JSON changes, runs automated contrast validation tests, compiles platform packages, and publishes updated npm and CocoaPods/Maven libraries automatically.
</details>

---

### Q4: How do you enforce CSS quality, performance budgets, and governance across 150+ engineers?
<details>
<summary>View Complete Architectural Answer</summary>

**Answer:**
Engineering discipline cannot scale on manual code reviews alone; it requires **automated tooling gates**:

1. **Pre-Commit Static Analysis (Stylelint 16)**:
   - Enforce architectural specificity limits: `selector-max-specificity: "0,3,0"`.
   - Ban ID selectors in styling: `selector-max-id: 0`.
   - Ban rogue overrides: `declaration-no-important: true`.
   - Restrict colors to approved tokens: `color-no-hex: true` (only `var(--token)` allowed).
2. **Continuous Integration (CI) Bundle Size Budgets**:
   Use tools like Size Limit or bundlewatch to enforce hard gzip/Brotli byte thresholds. If a pull request increases global CSS bundle size by >5KB, the CI build fails automatically.
3. **Automated Dead Code Elimination (PurgeCSS / LightningCSS)**:
   Run automated static tree-shaking during production builds to purge unused CSS selectors from dependencies.
4. **Design System Internal RFC Process**:
   Before any team creates custom one-off components, they must submit a lightweight Request For Comments (RFC) to the Design Systems core team, preventing component duplication across squads.
</details>

---

### Q5: How do you handle cross-browser compatibility between modern evergreen browsers and lagging enterprise/mobile engines?
<details>
<summary>View Complete Architectural Answer</summary>

**Answer:**
Follow a disciplined **Progressive Enhancement Architecture**:

1. **Consult Web Platform Baseline**:
   Categorize features into **Baseline Widely Available** (safe to use everywhere: Grid, Flexbox, `dvh`, `@layer`) versus **Baseline Newly Available** (use with fallbacks: Anchor Positioning, `field-sizing`).
2. **Feature Queries (`@supports`)**:
   Never use user-agent sniffing. Check capabilities directly:
   ```css
   /* Baseline fallback */
   .card-grid { display: flex; flex-direction: column; }

   /* Modern enhancement */
   @supports (display: subgrid) {
     .card-grid { display: grid; grid-template-rows: subgrid; }
   }
   ```
3. **Graceful Degradation vs Equal Appearance**:
   Educate product managers and design leadership on the core tenet of the web: **"Websites do not need to look identical in every browser."** A user on older iOS Safari may experience an instant dialog popup rather than an `@starting-style` spring zoom, but the core business transaction functions with 100% fidelity.
</details>

---

### Q6: How do you evaluate and guarantee WCAG 2.2 Level AA compliance across an enterprise UI component library?
<details>
<summary>View Complete Architectural Answer</summary>

**Answer:**
Accessibility is built into the CSS architecture through four verification pillars:

1. **Automated Unit Testing (axe-core / Playwright)**:
   Run headless automated accessibility checks on every component state (idle, hover, active, focus, disabled, loading). Catch missing labels, insufficient color contrast, and invalid ARIA attributes automatically.
2. **Mathematical Contrast Verification**:
   Ensure all normal text meets at least **4.5:1** contrast against backgrounds, large text meets **3:1**, and UI boundaries (input borders) meet **3:1** per WCAG 1.4.3 and 1.4.11.
3. **High Contrast Mode Support (`forced-colors: active`)**:
   Ensure that when Windows High Contrast Mode is active, all cards, buttons, badges, and inputs retain visible boundaries using system color keywords (`ButtonBorder`, `Highlight`, `CanvasText`).
4. **Physical Touch Target Verification**:
   Verify that all interactive controls on touchscreens provide at least a **44x44px** or **48x48px** clickable bounding box (via `::before` pseudo-element touch expanders) per WCAG 2.5.5 / 2.5.8.
</details>

---

## Navigation
- **Previous Guide**: [Practical Layout Challenges Q&A](./09-practical-layout-challenges.md)
- **Next Module**: [Module 16: Operational Cheatsheets](../16-cheatsheets/README.md)
