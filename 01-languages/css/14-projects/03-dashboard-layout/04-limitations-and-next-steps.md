# Limitations & Next Steps: Cloud Operations Dashboard Shell

## 1. Architectural Limitations & Production Edge Cases

1. **Large Dataset DOM Virtualization**:
   While `table-layout: fixed` and CSS sticky freeze-panes render hundreds of rows efficiently, rendering 10,000+ cluster nodes simultaneously causes memory pressure and scroll stutter. In enterprise deployments, pairing the CSS sticky layout with DOM virtualization (e.g. TanStack Virtual) or CSS `content-visibility: auto` is required.
2. **Popover Sidebar Backdrop Blurs on Low-End Mobile**:
   Applying `backdrop-filter: blur(8px)` on mobile off-canvas drawer backdrops can drop animation frame rates below 60fps on low-tier mobile GPUs. A solid `rgba(0, 0, 0, 0.6)` fallback provides maximum performance.

---

## 2. Progressive Enhancements & Next Steps

1. **CSS Subgrid for Aligned Metrics Rows**:
   Integrating `grid-template-rows: subgrid` on `.metric-card` so multi-line titles, values, and trend indicators synchronize baselines across all 4 cards in the ribbon.
2. **Dynamic Compact Sidebar Transition**:
   Transitioning `grid-template-columns: 250px 1fr` to `72px 1fr` on desktop when the user toggles a compact mode, combined with text label fade-outs.
