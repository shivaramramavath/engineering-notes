# Mock Service Worker (MSW) v2 — Basic to Advanced

A comprehensive reference card for `msw` v2 in React 19 testing and development.

> **Target Version:** MSW v2.6+  
> **Philosophy:** Network-level mocking via Service Workers (browser) and class interceptors (Node.js)  
> **Advantage:** Mocks at the transport layer; zero monkey-patching of `fetch` or Axios  

---

## Level 0 — Concepts

- **Transport-Level Interception**: Unlike traditional mock tools (like `jest.mock('axios')`), MSW intercepts HTTP requests at the network layer. In the browser, it uses a real Service Worker (`mockServiceWorker.js`); in Node.js/Vitest, it intercepts low-level socket connections.
- **Identical Handlers**: The exact same mock handler definitions are shared across unit tests (Vitest), local development, and Storybook stories.
- **Core Primitives Table**:

| API | Type | Purpose |
| :--- | :--- | :--- |
| `http.get()`, `http.post()` | Function | Declarative request matcher functions |
| `HttpResponse.json()` | Function | Constructs standardized mocked JSON responses |
| `setupServer()` | Function | Configures Node.js / Vitest test interceptor |
| `setupWorker()` | Function | Configures browser Service Worker for local development |
| `server.use()` | Method | Overrides request handlers for a specific isolated test |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npm install -D msw
```

### 1.2 Defining Request Handlers
```ts
// src/mocks/handlers.ts
import { http, HttpResponse } from 'msw';

export const handlers = [
  http.get('/api/users', () => {
    return HttpResponse.json([
      { id: '1', name: 'Alice' },
      { id: '2', name: 'Bob' },
    ]);
  }),

  http.post('/api/users', async ({ request }) => {
    const newUser = (await request.json()) as { name: string };
    return HttpResponse.json({ id: '3', name: newUser.name }, { status: 201 });
  }),
];
```

### 1.3 Vitest Test Server Setup
```ts
// src/mocks/server.ts
import { setupServer } from 'msw/node';
import { handlers } from './handlers';

export const server = setupServer(...handlers);
```

```ts
// src/setupTests.ts
import { server } from './mocks/server';
import { beforeAll, afterEach, afterAll } from 'vitest';

beforeAll(() => server.listen({ onUnhandledRequest: 'error' }));
afterEach(() => server.resetHandlers());
afterAll(() => server.close());
```

---

## Level 2 — Intermediate: Simulating Errors in Individual Tests

```tsx
import { test, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import { http, HttpResponse } from 'msw';
import { server } from '../mocks/server';
import { UserProfile } from './UserProfile';

test('renders error alert when API returns 500 server error', async () => {
  // Override handler specifically for this test
  server.use(
    http.get('/api/users', () => {
      return new HttpResponse(null, { status: 500 });
    })
  );

  render(<UserProfile />);
  expect(await screen.findByText(/Failed to load user/i)).toBeInTheDocument();
});
```

---

## Level 3 — Advanced: Simulating Slow Networks and Delays

```ts
import { http, HttpResponse, delay } from 'msw';

export const handlers = [
  http.get('/api/analytics', async () => {
    // Simulates a realistic 1.5 second network latency
    await delay(1500);
    return HttpResponse.json({ metrics: [10, 20, 30] });
  }),
];
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Forgetting `server.resetHandlers()` in `afterEach`**| Mock overrides leak into subsequent tests, causing random intermittent test failures. | Always reset handlers in test lifecycle `afterEach()`. |
| **Using `onUnhandledRequest: 'bypass'` in CI** | Silent network calls escape to the real internet, causing brittle, flaky CI runs. | Set `server.listen({ onUnhandledRequest: 'error' })`. |
| **Mocking `fetch` Globally with `vi.fn()`** | Masks real HTTP status codes, header bugs, and JSON serialization flaws. | Use MSW at the transport layer for realistic integration testing. |
| **Using MSW v1 Syntax (`rest.get()`, `res()`, `ctx()`)**| Deprecated in MSW v2; causes migration build errors. | Use modern MSW v2 syntax (`http.get()`, `HttpResponse.json()`). |
