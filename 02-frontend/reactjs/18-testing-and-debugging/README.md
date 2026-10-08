# 18 — Testing and Debugging

Tests are how you change code without fear. Debugging is how you find out why it's broken anyway. This folder covers both, with one guiding principle:

> **Test your app the way users use it.** Click what they click, read what they read, and avoid asserting on implementation details they never see.

That principle shapes the whole stack:

```text
        ┌────────────────────────────────────────┐
        │  E2E (Playwright)          few, slow      │  real browser, real-ish backend: critical flows
        ├────────────────────────────────────────┤
        │  Integration (Vitest + RTL + MSW)  many   │  pages/features with real providers, mocked network
        ├────────────────────────────────────────┤
        │  Unit (Vitest)              many, fast     │  pure logic: utils, reducers, hooks
        ├────────────────────────────────────────┤
        │  Static (TypeScript, ESLint)   always on   │  catches typos and type errors for free
        └────────────────────────────────────────┘
```

## The stack in this folder

| Tool | Role |
|---|---|
| **Vitest** | Test runner and assertions (Vite-native, Jest-compatible API) |
| **React Testing Library (RTL)** + **user-event** | Render components and interact like a user |
| **MSW** (Mock Service Worker) | Mock the network at the request level |
| **Playwright** | End-to-end tests in real browsers |
| **Browser DevTools, React DevTools, Query/Redux devtools** | Debugging |

## Prerequisites

- [Components and props](../01-fundamentals/02-components-and-props.md), [hooks](../03-hooks/README.md)
- [API client](../11-api-integration/02-api-client.md) and [TanStack Query](../12-server-state/03-tanstack-query.md): you'll test code that uses them

## Contents

| # | File | What you'll learn |
|---|------|-------------------|
| 00 | [Testing fundamentals](./00-testing-fundamentals.md) | What to test, test types, the trophy, good vs brittle tests |
| 01 | [Vitest](./01-vitest.md) | Setup, API, mocks, fake timers, coverage |
| 02 | [Component testing with RTL](./02-component-testing-with-rtl.md) | Queries, user-event, async UI, providers |
| 03 | [Hook testing](./03-hook-testing.md) | `renderHook`, `act`, timers, wrappers |
| 04 | [Mocking and MSW](./04-mocking-and-msw.md) | What to mock, MSW handlers, test setup |
| 05 | [Integration testing](./05-integration-testing.md) | Whole features: router + query cache + network |
| 06 | [E2E testing with Playwright](./06-e2e-testing-playwright.md) | Locators, auto-waiting, auth, CI, traces |
| 07 | [Debugging](./07-debugging.md) | DevTools, common React errors, a repeatable method |

## Suggested order

00 → 01 → 02 is the core loop. 03 and 04 fill in hooks and the network. 05 and 06 zoom out. 07 stands alone and is useful from day one.

## Quick reference

A condensed version lives in the [testing cheatsheet](../24-cheatsheets/09-testing.md), and common interview questions in [testing interview prep](../23-interview/08-testing.md).
