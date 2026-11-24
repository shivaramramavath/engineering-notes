# 18 — Testing & Debugging

Tests are how you change code without fear. Debugging is how you find out why it's broken anyway. This module covers both, with one guiding principle:

> **Test your app the way users use it.** Click what they click, read what they read, and avoid asserting on implementation details they never see.

```text
        ┌────────────────────────────────────────┐
        │  E2E (Playwright)          few, slow   │  real browser, real-ish backend: critical flows
        ├────────────────────────────────────────┤
        │  Real Browser (Vitest Browser Mode)    │  real layout dimensions, real CSS, visual snapshots
        ├────────────────────────────────────────┤
        │  Integration (Vitest + RTL + MSW)      │  pages/features with real providers, mocked network
        ├────────────────────────────────────────┤
        │  Unit (Vitest)             many, fast  │  pure logic: utils, reducers, custom hooks
        ├────────────────────────────────────────┤
        │  Static (TypeScript, ESLint)  always on│  catches typos and type errors for free
        └────────────────────────────────────────┘
```

---

## What you'll be able to do after this module

- **Design a High-ROI Test Strategy**: Focus on user-observable behavior, avoiding fragile assertions on internal state.
- **Master Vitest Core**: Configure test suites, mock timers, spy on functions, and measure code coverage with Vite-native execution speed.
- **Render with React Testing Library**: Query semantic accessibility roles (`getByRole`), interact with `@testing-library/user-event`, and handle async mutations.
- **Test Custom Hooks**: Use `renderHook` and `act()` to verify complex asynchronous state machines and timers.
- **Mock Network with MSW**: Intercept network requests at the transport level with Mock Service Worker v2, ensuring network mocks closely mimic production backends.
- **Build Full E2E Suites with Playwright**: Write robust, auto-waiting end-to-end tests covering login, checkout, and critical user funnels.
- **Run Real Browser Tests in Vitest 3**: Test real layout dimensions (`getBoundingClientRect`), computed CSS, and visual snapshots inside real Chromium/WebKit instances.
- **Systematic Debugging**: Diagnose React render loops, hydration mismatches, memory leaks, and performance issues using React DevTools and Chrome DevTools.

---

## Module Syllabus

| # | Note | Description | Key Tool |
|---|------|-------------|:---:|
| 00 | [00-testing-fundamentals.md](./00-testing-fundamentals.md) | Testing philosophy, testing trophy vs pyramid, brittle vs resilient tests | Methodology |
| 01 | [01-vitest.md](./01-vitest.md) | Vitest runner setup, assertion APIs, vi.fn, vi.spyOn, fake timers | Vitest 3 |
| 02 | [02-component-testing-with-rtl.md](./02-component-testing-with-rtl.md) | RTL queries, user-event interactions, testing async state | RTL / user-event |
| 03 | [03-hook-testing.md](./03-hook-testing.md) | `renderHook`, testing custom hooks, `act` mechanics, wrappers | RTL Hooks |
| 04 | [04-mocking-and-msw.md](./04-mocking-and-msw.md) | Mock Service Worker (MSW v2), network mocking, test handlers | MSW v2 |
| 05 | [05-integration-testing.md](./05-integration-testing.md) | Feature-level integration: Router + TanStack Query + MSW | Integration Testing |
| 06 | [06-e2e-testing-playwright.md](./06-e2e-testing-playwright.md) | End-to-end testing with Playwright, locators, auto-waiting, CI | Playwright |
| 07 | [07-debugging.md](./07-debugging.md) | Systematic debugging, React DevTools, hydration error diagnosis | DevTools |
| 08 | [08-vitest-browser-mode.md](./08-vitest-browser-mode.md) | Vitest 3 Browser Mode, real browser engine, layout dimensions, visual tests | Vitest Browser Mode |

---

## Boundary with Other Modules

- **Accessibility Verification**: Read [`08-accessibility/06-testing-accessibility-playwright.md`](../08-accessibility/06-testing-accessibility-playwright.md) for automated axe-core accessibility testing.
- **Performance Profiling**: Read [`14-performance/00-profiling-and-measuring.md`](../14-performance/00-profiling-and-measuring.md) for measuring INP and render bottlenecks.
- **Quick Reference**: See the [Testing Cheatsheet](../24-cheatsheets/09-testing.md) and [Testing Interview Questions](../23-interview/08-testing.md).

---
