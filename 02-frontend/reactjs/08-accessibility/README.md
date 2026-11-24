# 08 — Accessibility (a11y)

This module provides an exhaustive engineering guide to building inclusive, legally compliant React applications according to **WCAG 2.2 Level AA** standards. Covering semantic HTML, ARIA 1.2 roles and properties, keyboard focus management, screen reader compatibility, accessible primitives (Radix UI, React Aria), and automated CI/CD testing with Playwright and Axe-Core.

---

## What you'll be able to do after this module

- **Write Bulletproof Semantic HTML**: Choose the correct HTML elements (`<button>`, `<dialog>`, `<nav>`, `<main>`) to establish accessible DOM trees with zero custom hacks.
- **Master ARIA 1.2 Roles, States & Properties**: Understand the First Rule of ARIA, manage `aria-expanded`, `aria-controls`, and `aria-live` regions.
- **Implement Flawless Focus Management**: Build focus traps for modals, restore focus to triggers on unmount, and implement roving `tabIndex` for composite toolbars.
- **Support Screen Readers**: Verify compatibility across NVDA (Windows), JAWS (Windows), VoiceOver (macOS / iOS), and TalkBack (Android).
- **Leverage Headless Accessible Primitives**: Use Radix UI and Adobe React Aria to eliminate manual focus trapping, pointer normalization, and modal layout shifts.
- **Automate Accessibility in CI/CD**: Run `@axe-core/playwright` and `vitest-axe` scans on Pull Requests to block accessibility regressions before release.
- **Comply with WCAG 2.2 Standards**: Satisfy color contrast ratios (4.5:1), target size minimums (24x24px), and keyboard trap prevention.

---

## Module Syllabus

| # | Note | Description | Focus Area |
|---|------|-------------|:---:|
| 00 | [00-semantic-html.md](./00-semantic-html.md) | Document outline, native buttons vs anchors, form landmarks | HTML Semantics |
| 01 | [01-aria.md](./01-aria.md) | First rule of ARIA, states, properties, `aria-live` polite vs assertive | WAI-ARIA 1.2 |
| 02 | [02-keyboard-and-focus-management.md](./02-keyboard-and-focus-management.md) | Focus trapping, focus restoration, roving `tabIndex`, skip links | Keyboard A11y |
| 03 | [03-screen-readers.md](./03-screen-readers.md) | Testing matrix (NVDA, VoiceOver, JAWS), virtual cursor, AccName algorithm | Screen Readers |
| 04 | [04-accessibility-checklist.md](./04-accessibility-checklist.md) | WCAG 2.2 AA audit checklist, contrast, target sizes, motion preferences | Compliance |
| 05 | [05-accessible-radix-and-react-aria.md](./05-accessible-radix-and-react-aria.md) | Headless primitives: Radix UI, Adobe React Aria, Ark UI, compound a11y | Headless Primitives |
| 06 | [06-testing-accessibility-playwright.md](./06-testing-accessibility-playwright.md) | Automated E2E testing with `@axe-core/playwright` and `vitest-axe` | Automated Testing |

---

## Accessibility Testing Pyramid

| Layer | Tools | Scope | Catches |
| :--- | :--- | :--- | :--- |
| **Linting** | `eslint-plugin-jsx-a11y` | Static Code | Missing alt tags, invalid roles, bad tabIndex |
| **Unit** | `vitest` + `vitest-axe` | Virtual DOM | Missing form labels, invalid ARIA attributes |
| **End-to-End**| `playwright` + `@axe-core/playwright` | Real Browser | True CSS contrast, keyboard focus trap, Tab flow |
| **Manual** | NVDA / VoiceOver | Human User | Logical reading flow, pronunciation, UX clarity |

---

## Boundary with Other Modules

- **Component Design & Slots**: Read [`05-component-design/05-slots-and-asChild-pattern.md`](../05-component-design/05-slots-and-asChild-pattern.md) for slot-based accessible triggers.
- **Forms & Validation**: Read [`06-forms/`](../06-forms/README.md) for accessible error linking via `aria-describedby` and `aria-invalid`.
- **Styling & Focus Visible**: Read [`07-styling/06-tailwind-v4-changes-and-theming.md`](../07-styling/06-tailwind-v4-changes-and-theming.md) for OKLCH color contrast and focus rings.
- **Automated Testing Suite**: Read [`18-testing-and-debugging/`](../18-testing-and-debugging/README.md) for comprehensive Vitest and Playwright test architecture.

---

## Practical Exercises

1. **Focus Trap Modal**: Build a modal dialog that traps `Tab` navigation strictly within its boundaries and closes on `Escape`.
2. **Roving TabIndex Toolbar**: Implement a rich text toolbar where `ArrowLeft` and `ArrowRight` move active focus across buttons.
3. **Radix Popover Verification**: Use `@radix-ui/react-popover` and assert with `vitest-axe` that the rendered DOM has zero violations.
4. **Playwright Contrast Audit**: Write an automated Playwright test that verifies color contrast across both light mode and dark mode.
5. **Live Status Announcer**: Implement a polite `aria-live` status container that announces order completion to screen readers without stealing visual focus.

---

## Next Steps

Advance to **[09-ui-components/README.md](../09-ui-components/README.md)** to build high-performance UI components: TanStack Table, Drag-and-Drop, Floating UI, and Storybook design systems.
