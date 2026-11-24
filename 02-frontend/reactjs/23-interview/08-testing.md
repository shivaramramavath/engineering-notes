# Testing & Debugging — Senior Interview Questions & Deep Dives

> Testing pyramid strategies, Vitest Browser Mode, MSW API mocking, testing concurrent transitions, and avoiding implementation detail pitfalls.

---

## Question 1: Why does React Testing Library emphasize testing "user behavior" rather than "implementation details"?

### The Guiding Principle
> *"The more your tests resemble the way your software is used, the more confidence they can give you."* — Kent C. Dodds

### Pitfalls of Testing Implementation Details
1. **Refactoring Fragility**: If you assert `wrapper.state('count') === 1` or test that a private method `handleIncrement()` was called, changing internal component state to a reducer or Zustand store will break the test, even though the user-facing functionality remains 100% intact.
2. **False Positives & Negatives**: An implementation test can pass when the UI is completely unusable (e.g. button hidden by CSS or unclickable), or fail when the user experience is flawless.

### Resilient Query Priority
1. `getByRole('button', { name: /submit/i })` (Accessible by all users and screen readers)
2. `getByLabelText(/email/i)` (Accessible form controls)
3. `getByText(/welcome/i)`
4. `getByTestId('custom-slot')` (Last resort)

---

## Question 2: How does Vitest 3 Browser Mode differ fundamentally from jsdom tests?

### Architectural Comparison
- **jsdom**: A pure Node.js JavaScript re-implementation of the DOM and HTML standards.
  - *Limitation*: No layout engine, no CSS calculation (`getComputedStyle` is largely stubbed), `getBoundingClientRect()` returns `{0, 0, 0, 0}`, cannot test visual overflow, z-index layering, or browser-specific rendering engines.
- **Vitest Browser Mode**: Spawns a real browser instance (Chromium, Firefox, or WebKit via Playwright) and executes tests inside actual browser tabs.
  - *Advantage*: True CSS styling, accurate pixel measurements, native keyboard focus traversal, and visual regression snapshot diffing.
