# 08 — Accessibility

Accessibility (often abbreviated **a11y**) means building interfaces that people can use regardless of ability: people who navigate by keyboard, use screen readers or voice control, have low vision or color blindness, are deaf or hard of hearing, have motor or cognitive differences, or are simply using a phone in sunlight. It's also a legal requirement in many places, and nearly every accessible practice improves the experience for everyone.

React doesn't make interfaces accessible or inaccessible; your markup and behavior do. This chapter teaches the practices that matter most in React apps and how to verify them.

## Prerequisites

- [`../01-fundamentals/01-jsx.md`](../01-fundamentals/01-jsx.md) — JSX attribute names (`htmlFor`, `aria-*`)
- [`../01-fundamentals/06-events.md`](../01-fundamentals/06-events.md) and [`../03-hooks/04-useRef.md`](../03-hooks/04-useRef.md) — for focus management
- [`../07-styling/00-css-essentials.md`](../07-styling/00-css-essentials.md) — focus styles, contrast, reduced motion

## What you'll be able to do after this chapter

- Choose the right HTML element so the browser provides accessibility for free
- Use ARIA correctly, and recognize when not to use it
- Make every interaction work with a keyboard, with visible and well-managed focus
- Announce dynamic changes to screen reader users, and test with a screen reader
- Apply the WCAG principles with a practical checklist and automate what can be automated

## Reading order

| # | File | What it covers | Prerequisite |
|---|------|----------------|--------------|
| 00 | [00-semantic-html.md](./00-semantic-html.md) | Landmarks, headings, buttons vs links, forms, images, tables | `../01-fundamentals/` |
| 01 | [01-aria.md](./01-aria.md) | Roles, states, properties, accessible names; when (not) to use ARIA | 00 |
| 02 | [02-keyboard-and-focus-management.md](./02-keyboard-and-focus-management.md) | Tab order, focus styles, roving tabindex, focus traps, route changes | 00, 01 |
| 03 | [03-screen-readers.md](./03-screen-readers.md) | How screen readers work, live regions, testing with them | 01 |
| 04 | [04-accessibility-checklist.md](./04-accessibility-checklist.md) | WCAG in practice, a PR checklist, automated and manual testing | 00–03 |

Read `00` before anything else: most accessibility problems are solved by choosing the right element.

## Boundary with other chapters

- **Focus rings, contrast, and motion in CSS:** [`../07-styling/`](../07-styling/README.md)
- **Accessible form patterns (errors, labels, submission):** [`../06-forms/01-form-validation.md`](../06-forms/01-form-validation.md), [`../06-forms/05-form-submission-and-errors.md`](../06-forms/05-form-submission-and-errors.md)
- **Pre-built accessible components (dialogs, menus, tabs):** [`../09-ui-components/`](../09-ui-components/README.md)
- **Accessibility tests:** [`../18-testing-and-debugging/`](../18-testing-and-debugging/README.md)
- **Interview questions:** [`../23-interview/09-accessibility.md`](../23-interview/09-accessibility.md)

## Exercises

1. **Audit a page.** Take one of your own pages and list every non-semantic element doing a semantic job (clickable `div`s, fake headings, unlabeled inputs). Fix them.
2. **Keyboard only.** Unplug your mouse and complete a core task on your app. Note every place focus is lost, invisible, or trapped.
3. **Name everything.** Find every icon-only button and give it an accessible name without adding visible text.
4. **Build a disclosure.** Make a show/hide widget with correct `aria-expanded` and `aria-controls`, first with a native `<details>` and then with a `<button>`.
5. **Roving tabindex.** Implement a toolbar with arrow-key navigation and a single tab stop.
6. **Announce it.** Add a live region that announces "3 results found" after a search, then verify it with VoiceOver or NVDA.
7. **Automate.** Add `eslint-plugin-jsx-a11y` and an `axe` check to one component test; fix what it reports; then list three issues it could *not* catch.

## Next

**[`../09-ui-components/README.md`](../09-ui-components/README.md)** covers building on accessible primitives, so you don't hand-roll complex widgets like dialogs and menus.
