# Design Tokens and Theming Architecture

| Metadata | Details |
| :--- | :--- |
| **Spec Level** | W3C Design Tokens Community Group (DTCG) Specification / CSS Custom Properties L1 |
| **Baseline Status** | Universal Design System Engineering Standards |
| **Target Audience** | Design System Engineers, UI Architects, Frontend Leads |
| **Tested With** | Style Dictionary v4, Tokens Studio for Figma, Enterprise Multi-Brand Platforms |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility & Browser Support](#8-performance-accessibility--browser-support)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In modern digital product development, design decisions (colors, typography, spacing, border radii, shadows, animation timings) must be synchronized across disparate platforms: web browsers (CSS), mobile native apps (iOS Swift, Android Kotlin), design tools (Figma), and third-party partner portals.

When design values are hardcoded as raw hex codes or pixel numbers in stylesheets, severe problems arise:
- Changing a primary brand blue requires manual find-and-replace across thousands of CSS declarations.
- Visual divergence occurs between iOS, Android, and Web implementations.
- Supporting multi-brand, white-label, or multi-tenant theming requires maintaining separate duplicate stylesheets.

**Design Tokens** solve this by formalizing visual design decisions as platform-agnostic data (JSON/YAML). Popularized by Jina Anne at Salesforce and standardized by the **W3C Design Tokens Community Group (DTCG)**, tokens act as the single source of truth:
- Designers manage tokens in Figma.
- A build tool (like **Style Dictionary**) compiles the JSON tokens into CSS Custom Properties, Swift enums, and Android XML.
- CSS components consume semantic custom properties rather than raw values.

---

## 2. Core Concepts

### The 3-Tier Token Architecture
The industry standard design token architecture is structured into three progressive abstraction tiers:

```
[ Tier 1: Global / Reference Tokens ]
  Raw primitives with no semantic meaning.
  e.g., --blue-500: #3b82f6; --space-4: 16px;

                     │
                     ▼
[ Tier 2: Semantic / System Tokens ]
  Applies context, role, and intent to global tokens.
  e.g., --color-bg-primary: var(--blue-500);
        --color-text-danger: var(--red-600);

                     │
                     ▼
[ Tier 3: Component Tokens ]
  Scoped strictly to an individual component widget.
  e.g., --btn-bg: var(--color-bg-primary);
        --card-padding: var(--space-4);
```

### The W3C DTCG Format
The W3C Design Tokens Community Group standardizes token files in JSON format using `$value` and `$type` keys:
```json
{
  "color": {
    "brand": {
      "primary": {
        "$value": "#2563eb",
        "$type": "color",
        "$description": "Primary brand accent for interactive controls"
      }
    }
  }
}
```

### Why 3 Tiers are Essential
If components consume Global Tokens directly (e.g. `.btn { background: var(--blue-500); }`), changing the button color for Dark Mode or a White-Label tenant requires modifying the component CSS. By inserting **Tier 2 (Semantic Tokens)**, Dark Mode simply re-points `--color-bg-primary` to `--blue-400`, and all buttons, links, and cards update automatically without touching component code.

---

## 3. How It Works Internally

```
Design Token Pipeline & Multi-Platform Compilation:

  [Designers in Figma] ──(Tokens Studio Plugin)──> design-tokens.json (W3C DTCG)
                                                           │
                                                           ▼
                                                [Style Dictionary Compiler]
                                                           │
        ┌──────────────────────────────────────────────────┼──────────────────────────────┐
        ▼                                                  ▼                              ▼
  [Web Platform: CSS]                            [iOS: Swift Enum]             [Android: Kotlin / XML]
  :root {                                        enum Color {                  val colorPrimary =
    --color-brand: oklch(0.6 0.22 250);            case brandPrimary = ...       Color(0xFF2563EB)
    --space-md: 1rem;                            }                             }
  }
```

```
The 3-Tier Resolution Waterfall in CSS:
┌────────────────────┬──────────────────────────────────┬─────────────────────────────┐
│ Tier               │ Variable Declaration             │ Value Assigned              │
├────────────────────┼──────────────────────────────────┼─────────────────────────────┤
│ 1. Global (Raw)    │ `--palette-blue-600: #2563eb;`   │ Raw hex / oklch / pixel     │
│ 2. Semantic        │ `--color-interactive: var(--palette-blue-600);`│ Pointer to Global token     │
│ 3. Component       │ `--btn-bg: var(--color-interactive);`         │ Pointer to Semantic token   │
└────────────────────┴──────────────────────────────────┴─────────────────────────────┘
```

---

## 4. Syntax & Usage

### Pure CSS 3-Tier Token Implementation
```css
/* ========================================================
   TIER 1: Global / Reference Tokens (Never consumed directly by components!)
   ======================================================== */
:root {
  /* Primitive Palette */
  --palette-slate-50:  #f8fafc;
  --palette-slate-900: #0f172a;
  --palette-blue-500:  #3b82f6;
  --palette-blue-600:  #2563eb;
  --palette-red-600:   #dc2626;

  /* Primitive Spacing */
  --primitive-space-1: 0.25rem;
  --primitive-space-2: 0.5rem;
  --primitive-space-4: 1rem;
  --primitive-space-6: 1.5rem;

  /* Primitive Radii */
  --primitive-radius-sm: 0.25rem;
  --primitive-radius-md: 0.5rem;
  --primitive-radius-full: 9999px;
}

/* ========================================================
   TIER 2: Semantic / System Tokens (Consumed by layouts & components)
   ======================================================== */
:root {
  /* Surfaces */
  --surface-canvas: var(--palette-slate-50);
  --surface-card:   #ffffff;
  --surface-border: #e2e8f0;

  /* Text Roles */
  --text-main:      var(--palette-slate-900);
  --text-muted:     #64748b;
  --text-on-brand:  #ffffff;

  /* Action Roles */
  --action-primary:       var(--palette-blue-600);
  --action-primary-hover: #1d4ed8;
  --action-danger:        var(--palette-red-600);
}

/* Dark Mode: Re-map Semantic Tokens ONLY! */
@media (prefers-color-scheme: dark) {
  :root {
    --surface-canvas: #090d16;
    --surface-card:   #131b2e;
    --surface-border: #222f4c;
    --text-main:      #f8fafc;
    --text-muted:     #94a3b8;
    --action-primary: var(--palette-blue-500);
  }
}
```

---

## 5. Practical Examples

### Example 1: Full Component Token Integration (Tier 3)
A card and button component reading strictly from Tier 3 component tokens, backed by Tier 2 semantic tokens.

```html
<article class="c-card">
  <h3 class="c-card__title">Cloud Database</h3>
  <p class="c-card__description">High-availability managed PostgreSQL cluster.</p>
  <button class="c-btn">Provision Instance</button>
</article>
```

```css
/* Tier 3: Component Tokens */
.c-card {
  --c-card-bg:      var(--surface-card);
  --c-card-border:  var(--surface-border);
  --c-card-radius:  var(--primitive-radius-md);
  --c-card-padding: var(--primitive-space-6);

  background-color: var(--c-card-bg);
  border: 1px solid var(--c-card-border);
  border-radius: var(--c-card-radius);
  padding: var(--c-card-padding);

  .c-card__title {
    color: var(--text-main);
    margin-top: 0;
  }

  .c-card__description {
    color: var(--text-muted);
    font-size: 0.875rem;
  }
}

.c-btn {
  --c-btn-bg:     var(--action-primary);
  --c-btn-color:  var(--text-on-brand);
  --c-btn-radius: var(--primitive-radius-sm);

  background-color: var(--c-btn-bg);
  color: var(--c-btn-color);
  border-radius: var(--c-btn-radius);
  border: none;
  padding: 0.5rem 1rem;
  font-weight: 600;
  cursor: pointer;

  &:hover {
    background-color: var(--action-primary-hover);
  }

  &:focus-visible {
    outline: 2px solid var(--action-primary);
    outline-offset: 2px;
  }
}
```

### Example 2: Multi-Brand White-Label Theming
Supporting two completely different customer brands by overriding Tier 2 semantic tokens.

```html
<!-- Customer Brand A -->
<body data-brand="acme">
  <button class="c-btn">Acme Button</button>
</body>

<!-- Customer Brand B -->
<body data-brand="globex">
  <button class="c-btn">Globex Button</button>
</body>
```

```css
/* Brand A: Corporate Blue & Sharp Corners */
body[data-brand="acme"] {
  --action-primary: #0284c7;
  --action-primary-hover: #0369a1;
  --primitive-radius-sm: 2px;
  --primitive-radius-md: 4px;
}

/* Brand B: Friendly Emerald & Pill Corners */
body[data-brand="globex"] {
  --action-primary: #10b981;
  --action-primary-hover: #059669;
  --primitive-radius-sm: 9999px;
  --primitive-radius-md: 1rem;
}
```

### Example 3: Fluid Spacing and Typography Tokens
Creating mathematically proportional fluid scales using `clamp()`.

```css
:root {
  /* Fluid Typographic Scale */
  --font-size-sm: clamp(0.8rem, 0.75rem + 0.25vw, 0.875rem);
  --font-size-base: clamp(1rem, 0.95rem + 0.35vw, 1.125rem);
  --font-size-lg: clamp(1.25rem, 1.15rem + 0.6vw, 1.5rem);
  --font-size-xl: clamp(1.75rem, 1.5rem + 1.2vw, 2.25rem);

  /* Fluid Spacing Scale */
  --space-2xs: clamp(0.25rem, 0.2rem + 0.2vw, 0.375rem);
  --space-xs:  clamp(0.5rem, 0.45rem + 0.3vw, 0.75rem);
  --space-md:  clamp(1rem, 0.85rem + 0.8vw, 1.5rem);
  --space-xl:  clamp(2rem, 1.6rem + 1.8vw, 3rem);
}
```

### Example 4: Motion Design Tokens
Standardizing animation durations and physical easing curves.

```css
:root {
  /* Duration Tokens */
  --duration-instant: 100ms;
  --duration-fast:    200ms;
  --duration-moderate: 350ms;

  /* Easing Curve Tokens */
  --ease-standard: cubic-bezier(0.2, 0, 0, 1);
  --ease-spring:   cubic-bezier(0.34, 1.56, 0.64, 1);
  --ease-decelerate: cubic-bezier(0, 0, 0.2, 1);
}

.c-modal {
  transition:
    transform var(--duration-moderate) var(--ease-spring),
    opacity var(--duration-fast) var(--ease-standard);
}
```

### Example 5: Style Dictionary Configuration (`config.json`)
A configuration compiling W3C JSON tokens into CSS custom properties.

```json
{
  "source": ["tokens/**/*.json"],
  "platforms": {
    "css": {
      "transformGroup": "css",
      "buildPath": "dist/css/",
      "files": [
        {
          "destination": "tokens.css",
          "format": "css/variables",
          "options": {
            "outputReferences": true
          }
        }
      ]
    }
  }
}
```

---

## 6. Best Practices

1. **Components must NEVER consume Tier 1 Global tokens**: A component should never reference `--blue-500` or `--hex-ff0000`. Components must strictly consume Tier 2 Semantic (`--action-primary`) or Tier 3 Component (`--btn-bg`) tokens.
2. **Use W3C DTCG standard format in JSON**: When exporting tokens from Figma, adhere strictly to the `$value` and `$type` schema to ensure forward compatibility with modern toolchains.
3. **Keep semantic token names role-based, not visual**: Write `--surface-canvas`, `--surface-card`, `--text-danger`. Never name semantic tokens after colors: `--color-blue-bg` is a global token disguised as a semantic token.
4. **Export with `outputReferences: true`**: In Style Dictionary, enable reference preservation so CSS custom properties preserve variable pointers (`var(--palette-blue-500)`) in the output CSS instead of baking in raw static hex codes.
5. **Enforce fluid tokens for spacing and type**: Use `clamp()` for spacing and font sizes so layouts scale harmoniously between mobile and 4K displays.
6. **Group tokens logically by domain**: Keep color, typography, spacing, elevation, and motion tokens in separate files feeding a central index.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Consequence | Correction |
| :--- | :--- | :--- |
| Hardcoding raw hex values inside component CSS | Destroys dark mode and white-labeling; requires manual refactors. | Refactor component to read from Semantic tokens. |
| Skipping Tier 2 (Semantic) and linking Tier 3 directly to Tier 1 | Theming becomes impossible without overriding every component token. | Establish the 3-tier hierarchy: Global $\to$ Semantic $\to$ Component. |
| Naming semantic tokens after colors: `--card-blue-border` | When dark mode turns the border gray, the token name is a confusing lie. | Use intent-based names: `--card-border-accent`. |
| Creating too many one-off tokens (Token Bloat) | The design system loses consistency; tokens become unmanageable. | Constrain tokens to a strict scale (e.g. 8-step spacing, 10-step palette). |
| Forgetting `line-height` tokens when changing `font-size` | Text overlaps or creates massive vertical gaps. | Pair font-size tokens with proportional line-height tokens. |
| Neglecting dark mode contrast audits on semantic overrides | Inverting a semantic token accidentally creates unreadable $2:1$ contrast. | Audit both light and dark semantic mappings for WCAG AA compliance. |

---

## 8. Performance, Accessibility & Browser Support

### Performance
- **CSSOM Variable Mutation**: Changing a design token via JavaScript (`document.documentElement.style.setProperty('--brand', '#10b981')`) updates all dependent components instantly in native C++ without re-rendering Virtual DOM nodes.
- **Zero Paint Overhead**: CSS Custom properties parsed at `:root` have negligible impact on memory and render performance.

### Accessibility (A11y)
- **Contrast Ratios by Contract**: When architecting Tier 2 semantic tokens, pairing tokens as explicit foreground/background contracts (e.g. `--surface-card` is guaranteed to pair with `--text-main`) ensures WCAG AA compliance ($4.5:1$) across all themes.

### Browser Compatibility
- Universal. CSS Custom Properties enjoy 100% support across all modern and legacy evergreen browsers.

---

## 9. Comparison / When to Use What

| Architecture Tier | Example Token | Who Edits It? | Intended Consumer |
| :--- | :--- | :--- | :--- |
| **Tier 1: Global** | `--blue-600: #2563eb` | Brand Designers | Tier 2 Semantic Tokens |
| **Tier 2: Semantic** | `--action-primary: var(--blue-600)` | Design System Engineers | Layouts & Component Tokens |
| **Tier 3: Component**| `--btn-bg: var(--action-primary)` | Component Authors | Specific UI Widgets (`.c-btn`) |

---

## 10. Interview Questions

### Q1: What is the 3-Tier Design Token hierarchy and why is it standard in enterprise engineering?
<details>
<summary>View Answer</summary>
The 3-tier hierarchy consists of:
1. **Tier 1 (Global/Reference Tokens)**: Raw values without context (e.g. `--palette-blue-500: #3b82f6;`).
2. **Tier 2 (Semantic/System Tokens)**: Purpose-driven tokens that apply intent (e.g. `--action-primary: var(--palette-blue-500);`).
3. **Tier 3 (Component Tokens)**: Tokens scoped to a specific component (e.g. `--btn-bg: var(--action-primary);`).
It is standard because it decouples design decisions from component implementation. Supporting dark mode, high contrast, or white-labeling is achieved simply by re-pointing Tier 2 semantic tokens without modifying a single line of component CSS.
</details>

### Q2: What is the W3C Design Tokens Community Group (DTCG) specification?
<details>
<summary>View Answer</summary>
The W3C DTCG is an industry initiative establishing a standardized JSON schema for design tokens. By using `$value`, `$type`, and `$description` properties, design tools (Figma, Sketch) and compilers (Style Dictionary) can read, validate, and transform tokens into platform-specific code (CSS, Swift, Kotlin) with zero manual translation.
</details>

### Q3: Why is naming a semantic token `--color-blue-bg` considered an architectural anti-pattern?
<details>
<summary>View Answer</summary>
Semantic tokens must communicate **intent and role**, not literal visual appearance. If a token named `--color-blue-bg` is re-mapped to dark gray in Dark Mode or emerald green for a client tenant, the token name becomes contradictory and misleading. The proper semantic name is role-based: `--action-primary` or `--surface-highlight`.
</details>

### Q4: How does Style Dictionary integrate with CSS custom properties?
<details>
<summary>View Answer</summary>
Style Dictionary is an open-source build tool that consumes W3C JSON token files. During the build process, it parses token references, formats them according to CSS variable syntax (`--token-name: value;`), and wraps them inside `:root`. When configured with `outputReferences: true`, it preserves variable chains (`var(--primary)`), producing clean, human-readable CSS stylesheets.
</details>

### Q5: Can CSS design tokens be animated?
<details>
<summary>View Answer</summary>
By default, untyped custom properties cannot interpolate smoothly in gradients or angles. However, by registering design tokens using the modern CSS **`@property`** at-rule with typed syntaxes (`<color>`, `<length>`, `<angle>`), design tokens can be smoothly animated at 60 FPS via standard CSS `transition` and `@keyframes`.
</details>

### Q6: How should spacing tokens be designed to support responsive layouts?
<details>
<summary>View Answer</summary>
Rather than hardcoding static pixel numbers for spacing, modern design systems define fluid spacing tokens using `clamp()`, such as `--space-md: clamp(1rem, 0.8rem + 0.8vw, 1.5rem);`. This allows margins and gutters to expand and contract smoothly between mobile and wide desktop screens without requiring hundreds of media queries.
</details>

### Q7: What is the risk of having too many design tokens (Token Bloat)?
<details>
<summary>View Answer</summary>
Token bloat occurs when designers and developers create separate one-off tokens for every minor variation. This leads to design fragmentation, cognitive overload for engineers choosing tokens, and larger stylesheet payloads. Design systems prevent bloat by enforcing strict scales (e.g. 5-step font scales, 8-step spacing grids).
</details>

### Q8: How can a single component override its own design tokens locally?
<details>
<summary>View Answer</summary>
Because CSS custom properties inherit through the DOM tree, an author can override a Tier 3 component token directly on a class or modifier:
```css
.c-card { background: var(--c-card-bg); }
.c-card--dark { --c-card-bg: #0f172a; } /* Local override! */
```
The component automatically re-renders with the local token without modifying the global `:root`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a 3-Tier Design Token System
**Objective**: Declare a 3-tier token system in CSS for a Primary Call-to-Action button.
<details>
<summary>View Solution</summary>

```css
:root {
  /* Tier 1: Global */
  --palette-indigo-600: #4f46e5;
  --palette-white: #ffffff;
  --radius-pill: 9999px;

  /* Tier 2: Semantic */
  --action-cta: var(--palette-indigo-600);
  --text-on-cta: var(--palette-white);

  /* Tier 3: Component */
  --btn-cta-bg: var(--action-cta);
  --btn-cta-text: var(--text-on-cta);
  --btn-cta-radius: var(--radius-pill);
}

.c-btn-cta {
  background-color: var(--btn-cta-bg);
  color: var(--btn-cta-text);
  border-radius: var(--btn-cta-radius);
  padding: 0.75rem 1.75rem;
  border: none;
  font-weight: 700;
  cursor: pointer;
}
```
</details>

### Exercise 2: Fluid Typographic Scale Tokens
**Objective**: Author 4 fluid typography tokens using `clamp()` for mobile ($320\text{px}$) to desktop ($1200\text{px}$).
<details>
<summary>View Solution</summary>

```css
:root {
  --text-sm: clamp(0.75rem, 0.7rem + 0.25vw, 0.875rem);
  --text-base: clamp(1rem, 0.95rem + 0.25vw, 1.125rem);
  --text-lg: clamp(1.25rem, 1.15rem + 0.5vw, 1.5rem);
  --text-xl: clamp(1.75rem, 1.5rem + 1.25vw, 2.25rem);
}
```
</details>

### Exercise 3: Dark Mode Semantic Token Re-map
**Objective**: Implement a clean dark mode theme switch by re-mapping only Tier 2 semantic variables.
<details>
<summary>View Solution</summary>

```css
:root {
  --color-bg: #ffffff;
  --color-text: #0f172a;
  --color-border: #e2e8f0;
}

@media (prefers-color-scheme: dark) {
  :root {
    --color-bg: #090d16;
    --color-text: #f8fafc;
    --color-border: #1e293b;
  }
}
```
</details>

### Exercise 4: White-Label Customer Theme Tokens
**Objective**: Structure tokens for a multi-tenant application where customer "Nordic" uses teal brand accents and rounded corners.
<details>
<summary>View Solution</summary>

```css
body[data-tenant="nordic"] {
  --action-primary: #0d9488;
  --action-primary-hover: #0f766e;
  --primitive-radius-md: 1.25rem;
  --surface-card: #f0fdfa;
}
```
</details>

### Exercise 5: Accessible Contrast Contract Token
**Objective**: Design semantic surface and text tokens guaranteed to satisfy WCAG AA contrast.
<details>
<summary>View Solution</summary>

```css
:root {
  /* Background: Light gray (96% luminance) */
  --surface-subtle: oklch(0.96 0.01 250);
  /* Text: Dark slate (22% luminance) -> Contrast Ratio > 10:1 (AAA) */
  --text-on-subtle: oklch(0.22 0.03 250);
}

.c-alert-banner {
  background-color: var(--surface-subtle);
  color: var(--text-on-subtle);
  padding: 1rem;
}
```
</details>

### Exercise 6: Component Token Override Pattern
**Objective**: Build a card component whose background token can be overridden locally via utility classes without breaking global styles.
<details>
<summary>View Solution</summary>

```css
.c-card {
  --c-card-bg: var(--surface-card, #ffffff);
  background-color: var(--c-card-bg);
  padding: 1.5rem;
  border-radius: 0.5rem;
}

.c-card--highlight {
  --c-card-bg: #eff6ff;
}
```
</details>

---

## 12. Summary / Cheat Sheet

```css
/* 1. Global / Primitive (Raw Values) */
--palette-blue-500: #3b82f6;

/* 2. Semantic / System (Role & Intent) */
--action-primary: var(--palette-blue-500);

/* 3. Component (Scoped Widget API) */
--btn-bg: var(--action-primary);

/* Component Implementation */
.c-btn { background-color: var(--btn-bg); }
```

- Tier 1: Global Reference Tokens (Raw palette, units).
- Tier 2: Semantic Tokens (Role, intent, dark mode targets).
- Tier 3: Component Tokens (Component-scoped API).
- Components consume Tier 2 and Tier 3 tokens only.
- Compile from W3C DTCG JSON via Style Dictionary.

---

## 13. Further Reading

- [W3C Design Tokens Community Group Specification](https://design-tokens.github.io/community-group/format/)
- [Style Dictionary Official Documentation](https://styledictionary.com/)
- [Jina Anne: Design Tokens and the Future of Design Systems](https://www.designsystems.com/what-are-design-tokens/)
- [Brad Frost: Design Tokens in Atomic Design](https://bradfrost.com/blog/post/design-tokens-and-atomic-design/)

---

### Navigation
- **Previous**: [05-naming-and-file-organisation.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/05-naming-and-file-organisation.md)
- **Next**: [07-dark-mode-and-multi-theme.md](file:///e:/Learning/engineering-notes/01-languages/css/10-architecture-and-methodologies/07-dark-mode-and-multi-theme.md)
