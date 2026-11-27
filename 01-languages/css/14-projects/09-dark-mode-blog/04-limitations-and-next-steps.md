# Limitations & Next Steps: Technical Blog with Native Dark Mode

## 1. Architectural Limitations & Production Edge Cases

1. **Operating System Mode Override**:
   While `color-scheme: light dark` automatically tracks OS preferences without flash or JavaScript, users who wish to manually override their preference (e.g. force dark mode while the OS is in light mode) require a tiny inline `<script>` or CSS attribute toggle (`data-theme="dark"`).
2. **Code Syntax Highlighting in Dark Mode**:
   Syntax highlighting tokens must be designed with dual-contrast luminance to ensure that tokens like yellow numeric literals or dark blue function identifiers remain legible on both light (#f6f8fa) and dark (#161b22) code backgrounds.

---

## 2. Progressive Enhancements & Next Steps

1. **Sticky Table of Contents Rail**:
   Adding an aside with `position: sticky; top: 2rem;` on screens wider than 1200px.
2. **CSS Scroll-Driven Reading Progress Bar**:
   Using `animation-timeline: scroll()` to render an indicator bar across the top of the browser tracking article scroll completion in pure CSS.
