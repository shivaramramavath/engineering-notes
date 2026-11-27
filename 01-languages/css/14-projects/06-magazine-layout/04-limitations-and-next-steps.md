# Limitations & Next Steps: High-End Editorial Magazine Layout

## 1. Architectural Limitations & Production Edge Cases

1. **`column-span: all` Browser Reflow Cost**:
   When an element declares `column-span: all`, the browser layout engine splits the multi-column formatting context into pre-span and post-span blocks. While performant for static articles, dynamically resizing or inserting elements adjacent to spanning quotes forces a recalculation of all preceding column heights.
2. **Justified Text on Narrow Columns**:
   On viewports between 640px and 768px where columns are narrow (~300px), `text-align: justify` can produce awkward typographic "rivers" of empty space unless `hyphens: auto` is supported and enabled in the browser language dictionary.

---

## 2. Progressive Enhancements & Next Steps

1. **Native `initial-letter` Replacement**:
   As `initial-letter: 3 2` gains universal Baseline support, replacing floating `::first-letter` float hacks with native initial letter primitives ensures consistent vertical baseline alignment across all operating system font scalers.
2. **CSS Fragmentation Rules**:
   Adding explicit `orphans: 3; widows: 3;` rules to ensure that column breaks never isolate single lines of text at the top or bottom of a column.
