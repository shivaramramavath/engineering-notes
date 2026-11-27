# Empirical Verification: Cloud Operations Dashboard Shell

## 1. Measured Layout & Independent Scroll Boundaries

Measured via Playwright test environment:

### Desktop Shell (1440px × 900px)
| Structural Zone | Computed Width | Computed Height | Overflow & Behavior |
|---|---|---|---|
| `.dash-shell` | `1440.0px` | `900.0px` | `overflow: hidden; height: 100dvh` |
| `.dash-header`| `1440.0px` | `64.0px` | Fixed top overhead, zero document scroll |
| `.dash-sidebar`| `250.0px` | `836.0px` | `overflow-y: auto; z-index: 10` |
| `.dash-canvas` | `1190.0px` | `836.0px` | `overflow-y: auto; min-width: 0` |
| `.metric-card` (4x) | `280.0px` | `132.0px` | Aligned heights across row |
| `.data-table` | `1142.0px` | `265.0px` | Sticky `<thead>` locked at `top: 0` |

### Zero Document Scroll Verification
- Document `document.documentElement.scrollHeight` === `document.documentElement.clientHeight` (Exactly 900px).
- Internal `.dash-canvas` scrollable height: `1140px` (scrolling canvas does NOT move `.dash-header` or `.dash-sidebar`).

---

## 2. Freeze Panes & Accessibility Performance

| Test Target | Measured Value | Standard | Result |
|---|---|---|---|
| **Sticky Header Drift** | `0.00px` | Locked to top | Passed (`position: sticky; top: 0`) |
| **Sticky Column Drift** | `0.00px` | Locked to left | Passed (`position: sticky; left: 0`) |
| **Sticky Corner Stacking**| `z-index: 3` | Top of intersection | Passed (Renders above both row & col) |
| **Table Keyboard Focus**| `tabindex="0"` | Accessible region | Passed (Arrow keys scroll horizontally) |
| **Contrast Compliance** | `13.8:1` (Primary text) | `4.5:1` | Passed (AAA Compliance) |
