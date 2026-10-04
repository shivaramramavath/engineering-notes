# Accessibility Checklist

The previous files cover techniques. This one turns them into something you can run: a short orientation to WCAG (the standard most accessibility requirements are measured against), a practical checklist to use in pull requests and design reviews, and the tools — linters, automated checks, browser tools, and manual tests — that catch problems at each stage.

## Prerequisites

[`00-semantic-html.md`](./00-semantic-html.md) through [`03-screen-readers.md`](./03-screen-readers.md)

---

## 1. WCAG in one page

The **Web Content Accessibility Guidelines (WCAG)**, published by the W3C, are the standard behind most accessibility laws and procurement requirements. Current versions are **2.1 and 2.2** (2.2 adds criteria and is backward compatible; a newer major version is in development, so check w3.org for the latest). They're organized around four principles, **POUR**:

| Principle | Question | Examples |
|-----------|----------|----------|
| **Perceivable** | Can users perceive the content? | Text alternatives, captions, contrast, resizable text, not relying on color |
| **Operable** | Can users operate the interface? | Keyboard access, enough time, no seizure triggers, navigable, large-enough targets |
| **Understandable** | Can users understand content and interface? | Readable language, predictable behavior, error prevention and help |
| **Robust** | Does it work with assistive tech and across browsers? | Valid names, roles, and states; status messages |

Each **success criterion** has a conformance level: **A** (essential), **AA**, and **AAA**. **Target AA** — it's what regulations and contracts generally require. AAA isn't expected for whole sites.

Notable WCAG 2.2 AA additions relevant to React apps: **focus not obscured** by sticky UI (2.4.11), **dragging movements need an alternative** (2.5.7), **minimum target size** of 24×24 CSS px (2.5.8), and **accessible authentication** (no cognitive tests like remembering passwords with no alternative, 3.3.8). Criteria also cover **redundant entry** (don't make users retype information) and **consistent help** placement.

> **Legal context.** Accessibility is required by law or contract in many jurisdictions (for example, the ADA and Section 508 in the US, and the European Accessibility Act in the EU, which began applying in 2025 to many consumer products and services). Requirements and enforcement vary and change, so confirm what applies to your product with legal counsel rather than relying on this file.

---

## 2. The checklist

Use it as a pull request template, a design review aid, and a pre-release gate. Items link to the file that explains them.

### Structure and semantics

- [ ] The page has a single `<h1>`, headings are in order, and chosen for structure ([`00`](./00-semantic-html.md))
- [ ] Landmarks present: `header`, `nav`, `main` (exactly one), `footer`; repeated landmarks have distinct names
- [ ] Links navigate and buttons act; no clickable `div`/`span`; non-submit buttons have `type="button"`
- [ ] Lists, tables (with `caption`/`th scope`), `fieldset`/`legend` used where they apply
- [ ] `<html lang>` is set and correct; each route has a unique, descriptive `<title>`
- [ ] Link text makes sense out of context ("Read the pricing guide", not "click here")

### Keyboard and focus

- [ ] Every interactive element is reachable and operable with the keyboard alone ([`02`](./02-keyboard-and-focus-management.md))
- [ ] Tab order matches the visual and logical order; no positive `tabindex`
- [ ] Focus is always visible (`:focus-visible`), with sufficient contrast, and not hidden behind sticky UI
- [ ] A skip link is present and works
- [ ] No keyboard traps; modals trap intentionally, close on Escape, and return focus
- [ ] Composite widgets (tabs, menus, toolbars) use a roving tabindex and arrow keys
- [ ] After route changes, deletions, and closing overlays, focus lands somewhere sensible
- [ ] Hover-only content is also available on focus; custom shortcuts can be disabled or remapped
- [ ] Drag-and-drop and gestures have a keyboard / single-pointer alternative

### Forms

- [ ] Every input has a visible, programmatically associated `<label>` — not just a placeholder ([`00`](./00-semantic-html.md))
- [ ] Appropriate `type` and `autoComplete` values for common fields
- [ ] Required fields are indicated in text (not color alone)
- [ ] Errors are text, specific, and next to the field; fields use `aria-invalid` and `aria-describedby` ([`../06-forms/01-form-validation.md`](../06-forms/01-form-validation.md))
- [ ] On failed submit, focus moves to the first error or an error summary, and errors are announced ([`../06-forms/05-form-submission-and-errors.md`](../06-forms/05-form-submission-and-errors.md))
- [ ] Groups of radios/checkboxes are in a `fieldset` with a `legend`
- [ ] No cognitive tests or "paste-blocking" in authentication; password managers work

### Color, contrast, and visual design

- [ ] Normal text contrast ≥ **4.5:1**; large text (≥ 24px, or ≥ 18.66px bold) ≥ **3:1** ([`../07-styling/04-theming-and-dark-mode.md`](../07-styling/04-theming-and-dark-mode.md))
- [ ] UI components and meaningful graphics (borders of inputs, icons, focus indicators) ≥ **3:1**
- [ ] Information is never conveyed by **color alone** (add text, icons, patterns)
- [ ] Contrast checked in **every theme** (light, dark, high contrast), including muted, disabled, and placeholder text
- [ ] Content reflows at **320 CSS px width** (400% zoom) without two-dimensional scrolling; text resizes to 200% without loss
- [ ] Layout survives user text-spacing overrides; no fixed heights that clip text
- [ ] Touch targets ≥ 24×24 CSS px (aim for ~44×44 on touch UIs) ([`../07-styling/03-responsive-design.md`](../07-styling/03-responsive-design.md))
- [ ] Zoom and orientation are not locked

### Images, media, and motion

- [ ] Informative images have meaningful `alt`; decorative ones use `alt=""` ([`00`](./00-semantic-html.md))
- [ ] Icon-only controls have accessible names; decorative icons are `aria-hidden`
- [ ] Charts and complex images have a text or table alternative
- [ ] Video has captions (and audio description where needed); audio has a transcript; no autoplaying sound
- [ ] Animations respect `prefers-reduced-motion`; nothing flashes more than three times per second; moving or auto-updating content can be paused ([`../21-specializations/animation/04-animation-performance-and-accessibility.md`](../21-specializations/animation/04-animation-performance-and-accessibility.md))

### Names, roles, and states (ARIA)

- [ ] Native elements are preferred; ARIA is used only where needed ([`01`](./01-aria.md))
- [ ] Every control has an accessible name; the name includes its visible label
- [ ] States (`aria-expanded`, `aria-pressed`, `aria-selected`, `aria-current`) are accurate and kept in sync
- [ ] No `aria-hidden` on focusable elements; hidden regions use `hidden` or `inert`
- [ ] All `aria-controls`/`labelledby`/`describedby` ids exist and are unique
- [ ] Custom widgets follow the APG pattern, or use a tested primitive library

### Dynamic content and screen readers

- [ ] Status messages, results counts, and non-focus changes are announced via live regions ([`03`](./03-screen-readers.md))
- [ ] Live region containers exist before content changes; announcements are short and not repeated
- [ ] Loading states have a text equivalent and completion is announced
- [ ] Route changes update the title and move focus or announce
- [ ] Time limits and auto-dismissing messages can be extended or reviewed

### Process

- [ ] `eslint-plugin-jsx-a11y` passes; automated axe checks pass
- [ ] Keyboard-only walkthrough done for new or changed flows
- [ ] Screen reader spot-check done for new or complex widgets
- [ ] Reviewed at 200% zoom, narrow viewport, and with reduced motion

---

## 3. Tools by stage

No single tool covers everything. Layer them.

### While writing code: lint

**`eslint-plugin-jsx-a11y`** catches missing `alt`, unlabeled controls, invalid `aria-*`, click handlers on non-interactive elements, autoFocus, and more as you type ([`../00-setup/05-typescript-and-linting-setup.md`](../00-setup/05-typescript-and-linting-setup.md)):

```bash
npm install -D eslint-plugin-jsx-a11y
```

Enable its recommended rules in your flat ESLint config (see the plugin docs for the current config key). Treat violations as errors, not warnings.

### In component tests: axe

**axe-core** is the leading rules engine. Wrappers run it against rendered components in tests:

```tsx
import { render } from "@testing-library/react";
import { axe } from "jest-axe";            // or vitest-axe for Vitest projects

it("has no detectable accessibility violations", async () => {
  const { container } = render(<SignupForm />);
  expect(await axe(container)).toHaveNoViolations();
});
```

(Package names and matcher setup differ between `jest-axe` and Vitest-compatible forks such as `vitest-axe`; follow the README of the one you install, including registering the matcher.) Also use **Testing Library role queries** (`getByRole("button", { name: "Save" })`), which fail if roles or names are wrong ([`../18-testing-and-debugging/02-component-testing-with-rtl.md`](../18-testing-and-debugging/02-component-testing-with-rtl.md)). Note that jsdom doesn't compute layout or colors, so contrast checks aren't meaningful there.

### In the browser

- **axe DevTools** (extension) or **Lighthouse** (Chrome DevTools) audits the rendered page, including color contrast.
- **Browser DevTools → Accessibility panel / tree** shows each element's computed role, name, and state.
- **WAVE**, **Accessibility Insights** (guided assessments), and **Storybook's a11y addon** for component-level checks.
- **Contrast checkers** and DevTools color pickers show contrast ratios; simulators for color blindness are built into Chrome's rendering panel.

### In end-to-end tests

Run axe against full pages and key states (open modal, error state, expanded menu) with **Playwright** (`@axe-core/playwright`) or similar ([`../18-testing-and-debugging/06-e2e-testing-playwright.md`](../18-testing-and-debugging/06-e2e-testing-playwright.md)). Integrate into CI so regressions fail the build ([`../19-production/04-ci-cd.md`](../19-production/04-ci-cd.md)).

### Manual (essential)

- **Keyboard walkthrough**
- **Screen reader spot-checks** (NVDA + Firefox/Chrome; VoiceOver + Safari; one mobile)
- **Zoom to 200–400%**, narrow viewports, text-spacing overrides, high-contrast / forced-colors mode, reduced motion
- **Testing with disabled users**, which finds issues no checklist predicts

### What automation can and can't do

Automated tools reliably find missing names and `alt`, invalid ARIA, contrast failures on static text, and structural errors — but they catch only a **minority to roughly half** of real issues (estimates vary by study). They can't judge whether alt text is *meaningful*, whether the focus order is *logical*, whether a custom widget is *usable*, or whether announcements are *sensible*. **A passing automated scan is the floor, not the finish line.**

---

## 4. Building accessibility into your process

Retrofitting is expensive; building it in is cheap.

| Stage | Practice |
|-------|----------|
| **Design** | Check contrast and target sizes in the design tool; specify focus states, error states, and keyboard behavior; annotate heading levels and landmarks; design with real content and large text |
| **Component library** | Build on accessible primitives ([`../09-ui-components/00-shadcn-ui.md`](../09-ui-components/00-shadcn-ui.md)); bake semantics, names, and keyboard behavior into base components so product code gets them by default ([`../05-component-design/00-component-api-design.md`](../05-component-design/00-component-api-design.md)) |
| **Development** | Lint rules on; semantic HTML first; test with the keyboard as you build |
| **Code review** | Use the checklist above; ask "how does this work with a keyboard? with a screen reader?" |
| **CI** | axe in component and E2E tests; Lighthouse budgets |
| **QA** | Manual keyboard and screen reader passes on new flows; periodic audits |
| **Release** | Publish an **accessibility statement** with known issues and a way to report them |
| **Ongoing** | Track accessibility bugs like any others; retest after redesigns and library upgrades |

Make it a team habit, not one person's job. Include the **definition of done**: "meets the accessibility checklist".

---

## 5. Common patterns, quick fixes

| Symptom | Likely fix |
|---------|-----------|
| axe: "Buttons must have discernible text" | Add visible text, `aria-label`, or visually hidden text ([`01`](./01-aria.md)) |
| axe: "Form elements must have labels" | Associate a `<label htmlFor>` / wrap the input ([`00`](./00-semantic-html.md)) |
| axe: "Images must have alternate text" | `alt="…"` or `alt=""` if decorative |
| axe: "Elements must have sufficient color contrast" | Darken text or lighten background; check in every theme |
| axe: "Page must contain a level-one heading" / "landmark" | Add `<h1>`, `<main>` |
| axe: "ARIA attributes must conform to valid values / IDs must exist" | Fix typos; verify referenced ids |
| "Focus disappears" after an action | Move focus deliberately ([`02`](./02-keyboard-and-focus-management.md)) |
| Screen reader silent after update | Add a live region that exists beforehand ([`03`](./03-screen-readers.md)) |
| Div-as-button | Replace with `<button type="button">` |
| Modal behind-content still reachable | Use `<dialog>`/library, or `inert` on the background |

---

## 6. Interview angle

Common questions: *How do you make a modal accessible? What's the difference between `aria-label` and `aria-labelledby`? When shouldn't you use ARIA? How do you announce dynamic content? How do you test accessibility?* Concise answers in [`../23-interview/09-accessibility.md`](../23-interview/09-accessibility.md).

---

## Common mistakes

- **Treating accessibility as a final polish step** — retrofits are costly and incomplete.
- **Believing a clean axe/Lighthouse score means "accessible"** — those find only part of the issues.
- **Never testing with a keyboard or a screen reader.**
- **Checking contrast only in the default theme** (and ignoring dark, hover, disabled, and focus states).
- **Fixing accessibility per page** instead of fixing the shared component that causes it.
- **Adding ARIA to silence a linter** without fixing the underlying semantics.
- **Disabling lint rules** (`jsx-a11y`) to ship.
- **Making accessibility one person's responsibility** rather than part of the team's definition of done.
- **Overlay or "accessibility widget" products promising instant compliance** — they don't fix underlying problems and often make the experience worse; fix the code.
- **No accessibility statement or feedback channel** for users to report barriers.
- **Assuming WCAG compliance means usability for everyone** — involve disabled users.

## Quick summary

- WCAG is organized as POUR (perceivable, operable, understandable, robust); target **level AA**, including the WCAG 2.2 additions
- Use the checklist: structure, keyboard and focus, forms, color and contrast, media and motion, ARIA, and dynamic content
- Layer tooling: `jsx-a11y` while coding, axe in component and E2E tests, Lighthouse and DevTools in the browser, then manual keyboard, screen reader, zoom, and reduced-motion checks
- Automated tools find only part of the problems; manual testing and real users find the rest
- Build accessibility into design, shared components, review, CI, and release; fix causes in shared components, not page by page
- Avoid quick-fix overlays; publish an accessibility statement and a way to give feedback

## Next

You've finished the accessibility chapter. Continue to **[`../09-ui-components/README.md`](../09-ui-components/README.md)**, where accessible primitives (Radix, shadcn/ui) put these practices into reusable components. For test automation, see [`../18-testing-and-debugging/README.md`](../18-testing-and-debugging/README.md).
