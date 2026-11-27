# Limitations & Next Steps: Native Standards Accordion & Tabs Suite

## 1. Architectural Limitations & Production Edge Cases

1. **Native `name` Mutex Baseline Horizon**:
   The `name` attribute on `<details>` is part of Baseline 2024 (Chrome 120+, Firefox 125+, Safari 17.2+). For legacy browsers released prior to 2024, multiple `<details>` items can open simultaneously without closing siblings (degrading gracefully to non-exclusive accordions).
2. **Tablist ARIA Completeness**:
   While radio-driven tabs are completely functional and keyboard navigable via arrow keys, strict WCAG APG tablist standards recommend explicit `role="tablist"`, `role="tab"`, and `role="tabpanel"` attributes. For public enterprise web apps, augmenting the markup with light ARIA bindings provides maximum screen reader fidelity.

---

## 2. Progressive Enhancements & Next Steps

1. **`interpolate-size: allow-keywords` Direct Transitions**:
   As `interpolate-size: allow-keywords` becomes widely adopted, replacing the `grid-template-rows: 0fr -> 1fr` wrapper with a direct `height: 0 -> auto` transition simplifies DOM structure.
2. **Search Keyword Highlighting with CSS Custom Highlight API**:
   Pairing native find-in-page auto-expansion with `::highlight()` to dynamically colorize searched terms inside collapsed disclosures.
