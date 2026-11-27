# Limitations & Next Steps: Micro-Interaction & Animated Components Set

## 1. Architectural Limitations & Production Edge Cases

1. **`@starting-style` Browser Support Horizon**:
   `@starting-style` and `transition-behavior: allow-discrete` are Widely Available in modern platform versions (Chrome 117+, Firefox 125+, Safari 17.4+). In legacy engines, the dialog opens instantly with standard display toggle mechanics without visual interpolation.
2. **Font-Size Transitions on Floating Labels**:
   Transitioning `font-size` can occasionally trigger minor glyph rasterization repaints. Using `transform: scale(0.8)` instead of transitioning `font-size` directly isolates the transition completely to the GPU compositor thread.

---

## 2. Progressive Enhancements & Next Steps

1. **Spring Dynamics via CSS `linear()` Function**:
   Implementing physics-based spring curves using modern multi-point `linear()` timing functions for organic bounce back on button releases.
2. **View Transitions API Integration**:
   Elevating the modal opening sequence to the cross-document or single-page Document View Transitions API (`::view-transition`).
