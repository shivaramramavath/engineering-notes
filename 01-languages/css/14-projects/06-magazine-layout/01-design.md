# Design System & Specifications: High-End Editorial Magazine Layout

## 1. Typographic Hierarchy & Font Pairings

The editorial system pairs classic literary serifs for body text with stark modernist grotesques for mastheads and annotations:

| Role | Font Family | Size Specification | Line Height |
|---|---|---|---|
| **Masthead** | Modern Sans (`system-ui, sans-serif`) | `clamp(2.5rem, 1.5rem + 4vw, 5.5rem)` | `1.02` |
| **Article Title** | Serif (`Charter, "Bitstream Charter", "Sitka Text", Cambria, serif`) | `clamp(2rem, 1.25rem + 2.5vw, 3.5rem)` | `1.15` |
| **Lead Paragraph**| Serif (`Charter, serif`) | `clamp(1.125rem, 1rem + 0.4vw, 1.35rem)` | `1.6` |
| **Body Columns** | Serif (`Charter, serif`) | `1.0625rem` (17px) | `1.75` |
| **Captions & Meta**| Sans (`system-ui, sans-serif`) | `0.8125rem` (13px) | `1.4` |

---

## 2. Multi-Column Metrics

```
Article Container (Max-Width: 1040px)
+-------------------------------------------------------------------+
| Masthead & Article Header (Full Width)                             |
+---------------------------------+---------------------------------+
| Column 1 (min-width: 320px)     | Column 2 (min-width: 320px)     |
| [Drop Cap] Body paragraph...    | ...continuing narrative text...  |
+---------------------------------+---------------------------------+
| Pull Quote (column-span: all; spanning both columns)              |
+---------------------------------+---------------------------------+
| Column 1                        | Column 2                        |
| ...secondary section text...    | [Figure with Caption]           |
+---------------------------------+---------------------------------+
```

- **Column Rule**: `1px solid #e2e8f0` dividing column tracks cleanly.
- **Typographic Measure**: Optimal 65 characters per line (`max-width: 65ch` in single-column fallback).
