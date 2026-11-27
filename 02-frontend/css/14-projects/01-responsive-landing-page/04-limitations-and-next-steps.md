# Limitations & Next Steps: Responsive SaaS Landing Page

## 1. Architectural Limitations & Production Edge Cases

1. **Terminal Overflow on Ultra-Narrow Viewports (<320px)**:
   The preformatted `<pre><code>` block inside `.terminal-mockup` requires `overflow-x: auto` to prevent blowing out parent grid tracks. On devices under 320px width (e.g., Galaxy Fold outer screen), horizontal code scrolling is necessary.
2. **Infinite Marquee Duplication**:
   The pure CSS marquee relies on duplicating DOM spans in markup (`.marquee-group` rendered twice) to achieve continuous visual loops without gaps. While performant (Composited GPU translation), dynamic CMS data streams should generate these duplicates automatically via template iteration.
3. **Glassmorphism Backdrop Filter Fallback**:
   In legacy browsers lacking `backdrop-filter` support, `.site-header` degrades to a solid `rgba(255, 255, 255, 0.95)` fill without blurring background layers.

---

## 2. Progressive Enhancements & Next Steps

1. **Subgrid Baseline Alignment on Feature Cards**:
   Upgrade the feature cards to CSS Subgrid:
   ```css
   .features-grid {
     grid-template-rows: repeat(auto-fit, minmax(200px, auto));
   }
   .feature-card {
     display: grid;
     grid-row: span 4;
     grid-template-rows: subgrid;
   }
   ```
   This synchronizes card titles, paragraphs, and links across the horizontal row.
2. **Dark Mode Integration via `light-dark()`**:
   Convert the OKLCH tokens to native `light-dark()` CSS primitives for zero-JS operating system theme synchronization.
3. **Interactive Mobile Off-Canvas Drawer**:
   Pair the mobile header with the native HTML Popover API (`popover="auto"`) and `@starting-style` for pure-declarative drawer sliding.
