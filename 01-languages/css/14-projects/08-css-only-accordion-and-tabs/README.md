# Project 08: Native Standards Accordion & Tabs Suite

## Metadata
| Attribute | Details |
|:---|:---|
| **Tier** | Specialized Project |
| **Prerequisites** | [13-layout-and-component-recipes/11-tabs-accordions](../13-layout-and-component-recipes/11-tabs-accordions-and-toggles.md), [13-layout-and-component-recipes/14-css-only-limits](../13-layout-and-component-recipes/14-css-only-components-limits.md) |
| **Estimated Completion Time** | 4 Hours |
| **Target Baseline** | Modern Web Baseline (2024–2026) |
| **Core Layout Engine** | HTML `<details name>`, CSS Grid Dynamic Height & Segmented Radios |
| **Tested With** | Playwright `css_runner.js`, axe-core |

---

## 1. Project Overview & Architecture
This project demonstrates modern standards-compliant disclosure components that replace legacy JavaScript state managers and fragile checkbox hacks.

Key features:
- **Native Exclusive Accordion Group (`name="faq"`)**: Uses the HTML5 / Baseline 2024 `name` attribute to provide automatic mutual exclusion (expanding one question collapses all others) with zero script.
- **CSS Grid Dynamic Height Transition**: Smoothly animates accordion bodies from `grid-template-rows: 0fr` to `1fr` with `min-height: 0` without `max-height` hacks.
- **Accessible Segmented Tab Controller**: Keyboard-navigable segmented control driven by radio inputs and connected panels with high-contrast active indicators.
- **Browser Find-in-Page Integration**: Native `<details>` elements auto-expand when text is searched via <kbd>Ctrl+F</kbd>.

---

## 2. File Organization
1. [`01-design.md`](./01-design.md): Interaction specifications, transition curves, keyboard focus models.
2. [`02-html-and-css.md`](./02-html-and-css.md): Complete production semantic implementation.
3. [`03-measured-results.md`](./03-measured-results.md): Expansion timing measurements, height interpolation verification, and keyboard accessibility tests.
4. [`04-limitations-and-next-steps.md`](./04-limitations-and-next-steps.md): Deep nesting limitations, ARIA synchronization trade-offs, and `interpolate-size` upgrades.
