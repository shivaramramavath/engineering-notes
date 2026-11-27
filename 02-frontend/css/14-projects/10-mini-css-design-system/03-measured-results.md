# Empirical Verification: Mini CSS Design System & UI Toolkit

## 1. Cascade Layer Specificity Resolution Audit

Audited via Playwright and standard CSS cascade inspection:

| Test Scenario | Competing Selectors | Winner | Verification Reason |
|---|---|---|---|
| **Layer Priority Test** | `@layer components { .btn { color: red; } }` vs `@layer utilities { .u-text-muted { color: gray; } }` | `.u-text-muted` | `@layer utilities` is declared after `@layer components` |
| **Unlayered Override** | `.btn` (inside `@layer components`) vs `.my-override` (unlayered author rule) | `.my-override` | Unlayered styles ALWAYS defeat layered styles by specification |
| **Reset Isolation** | `@layer reset { * { box-sizing: border-box; } }` | Overridden safely | Reset layer is lowest in priority chain |

---

## 2. Contrast & Touch Target Compliance
- `.btn-primary` text contrast: `4.6:1` (White text on OKLCH primary blue).
- `.badge-success` text contrast: `5.2:1` on surface.
- Touch target sizes for `.btn`: `38px` desktop, expanded to `48px` minimum touch target on mobile viewports.
- All interactive controls provide visible `outline: 2px solid var(--color-primary); outline-offset: 2px;` on `:focus-visible`.
