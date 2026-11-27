# Module 01: Fundamentals & Core Syntax

Welcome to **Module 01: Fundamentals & Core Syntax**. This module forms the bedrock of CSS mastery. Here we deconstruct how the CSS parser tokenizes rules, how selectors match elements across the DOM tree, how the mathematical specificity algorithm resolves conflicting declarations, how modern Cascade Layers (`@layer`) tame global stylesheet collisions, and how units, custom properties (`--tokens`), and color functions operate in modern web engines.

---

## Module Roadmap & Navigation

| # | Note | Focus Area | Core Concepts & Capabilities |
|:---:|:---|:---|:---|
| **01** | [Syntax, Rules & Declarations](01-syntax-rules-and-declarations.md) | Syntax & Grammar | Selectors, declaration blocks, properties, values, vendor prefixes, parsing resilience |
| **02** | [Selectors: Type, Class, ID & Attribute](02-selectors.md) | DOM Matching | Combinators (` `, `>`, `+`, `~`), attribute selectors (`[attr]`, `[attr^=""]`, `[attr$=""]`, `[attr*=""]`) |
| **03** | [Pseudo-Classes](03-pseudo-classes.md) | State & Hierarchy | Dynamic states (`:hover`, `:focus-visible`), structural (`:nth-child`, `:has`, `:is`, `:where`, `:not`) |
| **04** | [Pseudo-Elements](04-pseudo-elements.md) | Virtual Elements | Generated content (`::before`, `::after`), typography (`::first-letter`, `::marker`, `::placeholder`, `::selection`) |
| **05** | [Specificity Calculation](05-specificity.md) | Cascade Algorithm | The specificity tuple `[A, B, C]`, specificity hacks (`:where()` 0-specificity vs `:is()`), calculation matrix |
| **06** | [Cascade: Origins, Importance & Order](06-cascade-origins-importance-and-order.md) | Cascade Resolution | User-agent, User, Author origins, normal vs `!important` reverse precedence, source order |
| **07** | [Cascade Layers (`@layer`)](07-cascade-layers.md) | Architecture | Modern `@layer` architecture, layer sorting, layer vs specificity inversion, third-party reset isolation |
| **08** | [Inheritance: `initial`, `inherit`, `unset`, `revert`](08-inheritance-initial-unset-revert.md) | Value Propagation | Inherited vs non-inherited properties, universal reset values, `revert-layer` |
| **09** | [CSS Units: Absolute vs Relative](09-units.md) | Dimensional Math | `px` vs `rem`/`em`, viewport units (`vw`, `vh`, `svh`, `lvh`, `dvh`), container query units (`cqi`, `cqw`) |
| **10** | [Colours & Color Spaces](10-colours.md) | Color Science | Modern color formats (`oklch()`, `color-mix()`, `display-p3`, `hsl()`, Relative Color Syntax) |
| **11** | [Custom Properties (CSS Variables)](11-custom-properties.md) | Dynamic Tokens | Declaring `--var`, consuming `var()`, fallbacks, runtime DOM mutation, typed properties with `@property` |
| **12** | [CSS Values & Math Functions](12-values-and-functions.md) | Computational CSS | Math functions (`calc()`, `min()`, `max()`, `clamp()`, `round()`, `mod()`, `sin()`, `cos()`) |
| **13** | [Fonts & Text Basics](13-fonts-and-text-basics.md) | Fundamental Typography | `font-family` system font stacks, `font-size`, `line-height`, `font-weight`, text alignment, font rendering |
| **14** | [Shorthand vs Longhand Properties](14-shorthand-and-longhand-properties.md) | Declarations | Reset traps in shorthands (`margin`, `padding`, `background`, `border`, `font`), mental models |
| **15** | [CSS Comments & Code Formatting](15-css-comments-and-formatting.md) | Code Standards | Comment syntax (`/* */`), section banners, architectural documentation, formatting best practices |

---

## Key Learning Outcomes

1. **Calculate Specificity Instantly**: Master the three-column tuple `[ID, Class, Type]` and leverage `:where()` to author zero-specificity utility classes.
2. **Master the Cascade Order of Precedence**: Understand the 8-step resolution order from User Agent defaults up to Transition and Animation origins.
3. **Architect with Cascade Layers (`@layer`)**: Eliminate `!important` wars by organizing styles into distinct layers (`reset`, `tokens`, `components`, `utilities`).
4. **Harness Modern Color Spaces**: Transition beyond legacy sRGB hex codes into perceptually uniform **OKLCH** and dynamic **Relative Color Syntax**.
5. **Implement Responsive Math**: Replace rigid media query breakpoints with fluid responsive typography and fluid spacing using `clamp()` and `min()`.

---

[Previous Module: 00 - Setup & Browser Engine Architecture](../00-setup/README.md) | [Next: 01 - Syntax, Rules & Declarations](01-syntax-rules-and-declarations.md)
