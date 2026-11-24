# Testing Async Server Components & Actions (`testing-server-components-and-async-components`)

> Testing strategies in Next.js App Router: Unit testing asynchronous React Server Components, testing Server Actions, API mocking with MSW v2, and Playwright end-to-end integration.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Testing** | Vitest 3.0, React Testing Library, MSW v2, Playwright |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The Challenge of Testing App Router Primitives
Traditional React testing tools (`render(<Component />)`) expect synchronous function components.
In modern Next.js:
- **Server Components are `async` functions**: `export default async function Page() { ... }`. React Testing Library cannot render a raw async function directly without resolving the Promise.
- **Server Actions are decoupled RPC functions**: They can be tested as standard TypeScript asynchronous functions without mounting a browser DOM.
- **Network Boundaries**: Outgoing requests inside Server Components run in Node.js, requiring Mock Service Worker (MSW) Node server interception.

```
+----------------------------------------------------------------------+
|                     Next.js Testing Architecture                     |
|                                                                      |
|  [ Unit Tests (Vitest) ] ──> Direct invocation of Server Actions    |
|                                                                      |
|  [ RSC Integration Tests ] ──> await Component(props) ──> RTL render |
|                                       │                              |
|                                       ▼                              |
|                       [ MSW v2 Node Interceptor ]                   |
|                                                                      |
|  [ E2E Tests (Playwright) ] ──> Real Chromium Browser vs next start  |
+----------------------------------------------------------------------+
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Attempting to `render(<AsyncComponent />)`
```tsx
// test.tsx (Naive - Fails with TypeScript and React errors!)
import { render } from '@testing-library/react';
import UserPage from './page';

it('renders user', () => {
  // ERROR: AsyncComponent returns a Promise, not a valid ReactElement in standard RTL!
  render(<UserPage params={Promise.resolve({ id: '1' })} />);
});
```

### Idiomatic: Awaiting the Async Component First
```tsx
// test.tsx (Idiomatic RSC Unit Testing)
import { render, screen } from '@testing-library/react';
import UserPage from './page';

it('renders user correctly', async () => {
  // 1. Await the server component function directly to resolve JSX element
  const resolvedJsx = await UserPage({
    params: Promise.resolve({ id: 'user-123' }),
    searchParams: Promise.resolve({}),
  });

  // 2. Render the resolved JSX into the testing DOM
  render(resolvedJsx);

  expect(screen.getByText(/user-123/i)).toBeInTheDocument();
});
```

---

## 3. Five Progressive Examples

### Example 1: Unit Testing a Server Action Directly
```ts
// actions/userActions.test.ts
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { updateUsernameAction } from './userActions';
import { db } from '@/db';
import { verifySession } from '@/dal/session';

vi.mock('@/dal/session', () => ({ verifySession: vi.fn() }));
vi.mock('@/db', () => ({ db: { user: { update: vi.fn() } } }));

describe('updateUsernameAction', () => {
  it('updates username when authenticated', async () => {
    vi.mocked(verifySession).mockResolvedValueOnce({ user: { id: 'u1' } } as any);
    vi.mocked(db.user.update).mockResolvedValueOnce({ id: 'u1', name: 'Alice' } as any);

    const result = await updateUsernameAction({ name: 'Alice' });
    expect(result.success).toBe(true);
  });
});
```

### Example 2: Mocking Next.js Navigation Hooks
```ts
// vitest.setup.ts
import { vi } from 'vitest';

vi.mock('next/navigation', () => ({
  useRouter: () => ({ push: vi.fn(), replace: vi.fn() }),
  useSearchParams: () => new URLSearchParams(),
  usePathname: () => '/dashboard',
  unauthorized: vi.fn(() => { throw new Error('NEXT_UNAUTHORIZED'); }),
  forbidden: vi.fn(() => { throw new Error('NEXT_FORBIDDEN'); }),
}));
```

### Example 3: Mocking Outgoing HTTP Requests with MSW v2
```ts
// tests/msw/server.ts
import { setupServer } from 'msw/node';
import { http, HttpResponse } from 'msw';

export const server = setupServer(
  http.get('https://api.external.com/products', () => {
    return HttpResponse.json([
      { id: 'p1', name: 'Mechanical Keyboard', price: 120 },
    ]);
  })
);
```

### Example 4: Testing Route Handlers Directly
```ts
// app/api/health/route.test.ts
import { describe, it, expect } from 'vitest';
import { GET } from './route';

describe('Health Route Handler', () => {
  it('returns status 200 with healthy flag', async () => {
    const res = await GET();
    const json = await res.json();
    expect(res.status).toBe(200);
    expect(json.status).toBe('healthy');
  });
});
```

### Example 5: Playwright End-to-End Test for Streaming UI
```ts
// tests/e2e/catalog.spec.ts
import { test, expect } from '@playwright/test';

test('verifies streaming Suspense boundary and cart update', async ({ page }) => {
  await page.goto('/catalog');

  // Verify skeleton appears first
  const skeleton = page.locator('.cart-skeleton');
  await expect(skeleton).toBeVisible();

  // Verify streamed content arrives
  const cartBadge = page.locator('#cart-badge');
  await expect(cartBadge).toHaveText('3 items', { timeout: 5000 });
});
```

---

## 4. Under the Hood & Internals
Server Components return a Promise resolving to a React virtual DOM element. Because Vitest runs in Node.js, calling `await Component(props)` invokes the function with Node runtime privileges, executing asynchronous database calls or MSW-intercepted fetches. Passing the resulting JSX object to React Testing Library's `render()` mounts the resolved tree into jsdom.

---

## 5. Security & Isolation in Tests
- **Test Database Isolation**: When running integration tests against real databases, use **Testcontainers** (spinning up temporary Dockerized Postgres containers) or execute tests within transactional rollbacks (`BEGIN ... ROLLBACK`) to prevent cross-test data pollution.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Testing async component with `render()` | Not awaiting Promise | Objects are not valid as a React child | Call `render(await Component(props))` |
| Mocking `cookies()` manually | Complex cookie jar interface | TypeError: `cookie.get is not a function` | Mock with full `ReadonlyRequestCookies` |
| Mocking window in RSC | Server Components have no window | `ReferenceError: window is not defined` | Run RSC tests in Node / happy-dom |
| Hardcoding localhost in E2E | Inflexible CI URLs | Playwright fails on staging environments | Use `process.env.BASE_URL` |

---

## 7. Performance & Optimization Guide
Run unit and Server Action tests with Vitest in watch mode using in-memory mocks for instant sub-second feedback. Reserve slow Playwright E2E browser tests for pull-request CI workflows.

---

## 8. TypeScript & Strict Typing Recipes
```ts
import type { JSX } from 'react';

export async function renderServerComponent<TProps>(
  Component: (props: TProps) => Promise<JSX.Element>,
  props: TProps
): Promise<JSX.Element> {
  return await Component(props);
}
```

---

## 9. Testing & Quality Verification

```bash
# Run Vitest unit & integration test suite
npm run test:unit

# Run Playwright E2E tests against built production server
npm run test:e2e
```

---

## 10. When NOT to Use This Pattern
- Do not use unit tests to verify complex browser-native behaviors (e.g. cookie synchronization across domain redirects). Use Playwright E2E tests for browser behaviors.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. How do you test an asynchronous React Server Component in Vitest?</summary>
By invoking the Server Component function as a standard async function (`const jsx = await MyServerComponent(props)`) and then passing the resolved JSX to React Testing Library's `render(jsx)`.
</details>

<details>
<summary>2. How do you test Server Actions in isolation?</summary>
Server Actions are plain asynchronous TypeScript functions. They can be imported and executed directly in Vitest unit tests, with mocked database and session layers.
</details>

<details>
<summary>3. What is the role of Mock Service Worker (MSW) in Next.js testing?</summary>
MSW intercepts outgoing HTTP requests made by Server Components or Route Handlers at the network socket layer in Node.js, returning mock API responses without modifying application source code.
</details>

<details>
<summary>4. How do you test `unauthorized()` and `forbidden()` interrupts?</summary>
Mock `next/navigation` to throw an identifiable error when `unauthorized()` or `forbidden()` is called, and assert that the component rejects with that error.
</details>

<details>
<summary>5. Why should E2E tests run against `next start` (production build) instead of `next dev`?</summary>
Because caching behaviors, Partial Prerendering (PPR), static optimization, and route tables behave differently in development mode. Testing against production builds guarantees real-world fidelity.
</details>

<details>
<summary>6. How do you handle database seeding in Playwright E2E tests?</summary>
Use Playwright's `globalSetup` script or dedicated test API routes to seed clean fixture data before running specs.
</details>

<details>
<summary>7. What is Testcontainers?</summary>
Testcontainers is a library that spins up lightweight, disposable Docker containers (like Postgres and Redis) programmatically during test runs, guaranteeing a clean database environment.
</details>

<details>
<summary>8. How do you test optimistic UI updates?</summary>
In Playwright, simulate slow network responses (or delayed Server Actions) and assert that the UI reflects the optimistic change before the server responds.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Unit test a Server Component</summary>
Write a Vitest test resolving an async greeting component.
</details>

<details>
<summary>Exercise 2 (Medium): Test a Server Action with mocked session</summary>
Test a newsletter subscription action with valid and invalid email addresses.
</details>

<details>
<summary>Exercise 3 (Hard): Configure Playwright for streaming Suspense</summary>
Write a Playwright test verifying that a fallback skeleton appears before the dynamic data stream finishes.
</details>

---

## 13. Cross-References & Further Reading
- [Components: Server & Client Composition](file:///E:/Learning/engineering-notes/02-frontend/nextjs/03-components/README.md)
- [Official Next.js Testing Guide](https://nextjs.org/docs/app/building-your-application/testing)
