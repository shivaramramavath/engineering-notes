# React Testing Library & user-event — Basic to Advanced

A comprehensive reference card for `@testing-library/react` and `@testing-library/user-event` in React 19.

> **Target Version:** React Testing Library v16+, `@testing-library/user-event` v14+  
> **Philosophy:** Test components the way users use them (Semantic accessibility roles, zero implementation details)  
> **Guiding Motto:** "The more your tests resemble the way your software is used, the more confidence they can give you."  

---

## Level 0 — Concepts

- **Query Priority**: Query elements using accessibility roles that real users perceive:
  1. `getByRole` (Buttons, Headings, Textboxes, Dialogs) — **Always preferred**
  2. `getByLabelText` (Form inputs paired with `<label>`)
  3. `getByText` (Non-interactive text paragraphs)
  4. `getByTestId` — **Last resort only**
- **Query Variants**:
  - `getBy*`: Returns element or throws synchronously.
  - `queryBy*`: Returns element or `null` (used to assert non-existence).
  - `findBy*`: Returns a Promise that polls until element appears (used for async UI).
- **Core Primitives Table**:

| API | Type | Purpose |
| :--- | :--- | :--- |
| `render()` | Function | Mounts a component tree into simulated DOM |
| `screen` | Object | Global query surface with pre-bound query methods |
| `userEvent.setup()` | Function | Initializes user interaction simulator with realistic browser events |
| `waitFor()` | Function | Retries an assertion until a timeout is reached |
| `act()` | Function | Wraps asynchronous mutations to flush React 19 Fiber tasks |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npm install -D @testing-library/react @testing-library/user-event @testing-library/jest-dom
```

### 1.2 Interactive Component Test
```tsx
import { test, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import React, { useState } from 'react';

function Counter() {
  const [count, setCount] = useState(0);
  return (
    <div>
      <p>Current Count: {count}</p>
      <button onClick={() => setCount((c) => c + 1)}>Increment</button>
    </div>
  );
}

test('increments counter on button click', async () => {
  const user = userEvent.setup();
  render(<Counter />);

  // 1. Verify initial render
  expect(screen.getByText(/Current Count: 0/i)).toBeInTheDocument();

  // 2. Click button using real user event simulation
  const button = screen.getByRole('button', { name: /increment/i });
  await user.click(button);

  // 3. Assert updated state
  expect(screen.getByText(/Current Count: 1/i)).toBeInTheDocument();
});
```

---

## Level 2 — Intermediate: Testing Asynchronous UI with `findByRole`

```tsx
test('displays asynchronous user profile once loaded', async () => {
  render(<UserProfile userId="101" />);

  // findByRole polls automatically until element appears or 1000ms times out
  const heading = await screen.findByRole('heading', { name: /Alice Smith/i });
  expect(heading).toBeInTheDocument();
});
```

---

## Level 3 — Advanced: Custom Wrapper Provider for Testing

Components requiring Context, Router, or QueryClient must be rendered with wrappers:
```tsx
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';

export function renderWithProviders(ui: React.ReactElement) {
  const testQueryClient = new QueryClient({
    defaultOptions: { queries: { retry: false } },
  });

  return render(
    <QueryClientProvider client={testQueryClient}>
      {ui}
    </QueryClientProvider>
  );
}
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Using `fireEvent.click()` Instead of `userEvent`**| `fireEvent` dispatches raw synthetic DOM events; skips focus, hover, and real keyboard event sequences. | Always use `const user = userEvent.setup(); await user.click()`. |
| **Asserting on Component State or Instance Variables**| Makes tests fragile: refactoring internal state breaks tests even if user UI remains identical. | Assert on user-observable text, ARIA roles, and DOM outputs. |
| **Using `waitFor()` around `findBy*` queries** | Redundant; `findBy*` already includes built-in `waitFor()` polling. | Call `await screen.findByRole(...)` directly. |
| **Overusing `data-testid` Everywhere** | Hides real accessibility flaws where buttons lack accessible names or inputs lack labels. | Use `getByRole('button', { name: '...' })`. |
