# Design System & Motion Specifications: Animated Components Set

## 1. Motion Tokens & Easing Curves

All micro-interactions use standardized motion tokens to maintain consistent visual physics:

| Token | Timing Function | Value | Usage |
|---|---|---|---|
| `--ease-spring-snappy` | Elastic Spring | `cubic-bezier(0.16, 1, 0.3, 1)` | Modals, drawers, floating labels |
| `--ease-tactile-press` | Quick Decel | `cubic-bezier(0.4, 0, 0.2, 1)` | Button clicks, switch toggles |
| `--duration-instant` | Instant | `100ms` | Micro-presses, active states |
| `--duration-moderate`| Moderate | `200ms` | Dropdowns, tooltips, fades |
| `--duration-deliberate`| Deliberate | `300ms` | Modals, off-canvas drawers |

---

## 2. Hardware-Acceleration Enforcement

To guarantee 60fps/120fps motion without main-thread jank, only properties that operate on the **Composite** rendering stage are animated:
- `transform` (scale, translate, rotate)
- `opacity`
- `filter` (sparingly)

Properties affecting Layout (`width`, `height`, `top`, `margin`, `padding`) or Paint (`background-color`, `border-color`) are strictly isolated or paired with discrete transitions.
