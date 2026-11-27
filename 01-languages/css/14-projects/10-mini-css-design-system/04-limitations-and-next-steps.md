# Limitations & Next Steps: Mini CSS Design System & UI Toolkit

## 1. Architectural Limitations & Production Edge Cases

1. **`!important` Cascade Layer Inversion**:
   In CSS Cascade Layers, marking a rule `!important` *inverts* layer priority: an `!important` rule in `@layer reset` will defeat an `!important` rule in `@layer utilities`. Because this inverse behavior can confuse developers, design systems should strictly enforce a zero-`!important` linting policy using Stylelint (`declaration-no-important`).
2. **Third-Party CSS Integration**:
   When importing third-party stylesheets (e.g. `@import "vendor.css" layer(vendor);`), ensuring the `vendor` layer is explicitly placed before `components` guarantees that internal design system rules override vendor defaults cleanly.

---

## 2. Progressive Enhancements & Next Steps

1. **LightningCSS Packaging & Compression**:
   Compiling the `@layer` stylesheets using LightningCSS to generate both human-readable unminified development builds and pre-compressed Brotli distribution bundles.
2. **Design Token Sync via Tokens Studio / Figma Tokens**:
   Setting up a GitHub Actions workflow to transform JSON design tokens into the `@layer tokens` stylesheet automatically on Figma design commits.
