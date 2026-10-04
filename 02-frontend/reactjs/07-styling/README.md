# 07 — Styling

How to make React apps look right and keep the styles maintainable: the CSS you actually need, the main ways to organize styles in a React project (CSS Modules, Tailwind), responsive layouts, theming with dark mode, and a decision guide for choosing an approach.

## Prerequisites

- [`../01-fundamentals/01-jsx.md`](../01-fundamentals/01-jsx.md) — `className`, the `style` prop
- [`../05-component-design/00-component-api-design.md`](../05-component-design/00-component-api-design.md) — variants and forwarding `className`
- Basic HTML and CSS (selectors, properties). `00-css-essentials.md` refreshes the parts that matter most for component work.

## What you'll be able to do after this chapter

- Predict which CSS rule wins (the cascade and specificity)
- Lay out components with flexbox and grid
- Scope styles to components with CSS Modules
- Build interfaces quickly with Tailwind's utility classes and avoid its common pitfalls
- Make layouts that work from phones to desktops
- Implement design tokens and a light/dark/system theme without a flash of the wrong theme
- Choose a styling approach for a project and justify it

## Reading order

| # | File | What it covers | Prerequisite |
|---|------|----------------|--------------|
| 00 | [00-css-essentials.md](./00-css-essentials.md) | The cascade, box model, flexbox, grid, custom properties | Basic CSS |
| 01 | [01-css-modules.md](./01-css-modules.md) | Locally scoped CSS files | 00 |
| 02 | [02-tailwindcss.md](./02-tailwindcss.md) | Utility-first styling | 00 |
| 03 | [03-responsive-design.md](./03-responsive-design.md) | Mobile-first layouts, media and container queries | 00 |
| 04 | [04-theming-and-dark-mode.md](./04-theming-and-dark-mode.md) | Design tokens, theme switching, no-flash dark mode | 00, 02 |
| 05 | [05-styling-approaches-compared.md](./05-styling-approaches-compared.md) | Comparison, decision guide, `cn`, class merging | 01, 02 |

You don't need both `01` and `02` to start building: pick the approach your project uses and read the other later. `05` helps you decide.

## Boundary with other chapters

- **Component variants (`cva`) and a component kit:** [`../09-ui-components/01-component-variants.md`](../09-ui-components/01-component-variants.md), [`../09-ui-components/09-design-system.md`](../09-ui-components/09-design-system.md)
- **Animation and transitions:** [`../21-specializations/animation/`](../21-specializations/animation/README.md)
- **Accessibility of styling (focus, contrast, motion):** [`../08-accessibility/`](../08-accessibility/README.md)
- **Bundle size and CSS performance:** [`../14-performance/05-bundle-optimization.md`](../14-performance/05-bundle-optimization.md)
- **JavaScript media queries (`useMediaQuery`):** [`../03-hooks/10-hook-recipes.md`](../03-hooks/10-hook-recipes.md)

## Exercises

1. **Specificity.** Given three conflicting rules for the same element (a class, an ID, and an inline style), predict the winner, then verify in DevTools.
2. **Layout.** Build a card grid with CSS Grid (`auto-fit`/`minmax`) and a header with Flexbox, with no media queries.
3. **CSS Modules.** Style a `Button` with a `.module.css` file, including a `variant` prop that selects a class.
4. **Tailwind.** Rebuild the same button and card with Tailwind utilities; extract a React component instead of using `@apply`.
5. **Responsive.** Make a navigation that is a stacked menu on phones and a horizontal bar on wide screens, mobile-first.
6. **Dark mode.** Add a theme toggle with three options (light, dark, system), persisted in `localStorage`, with no flash on page load.
7. **Decide.** Write a half-page recommendation for a styling approach for a hypothetical project (team size, design system needs, server components) using the criteria in `05`.

## Next

**[`../08-accessibility/README.md`](../08-accessibility/README.md)** covers making what you've built usable by everyone, including focus styles, contrast, and motion preferences touched on here.
