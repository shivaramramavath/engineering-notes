# Testing with Vitest & React Testing Library Cheatsheet

> Quick-reference guide for rendering components, user interactions with `@testing-library/user-event`, async assertions, and MSW network mocking.

---

## 1. Query Priority Cheat Sheet

| Query | When to Use | Accessible by Screen Readers? |
| :--- | :--- | :--- |
| `getByRole('button', { name: /save/i })` | Buttons, links, headings, checkboxes | Yes (Primary Choice) |
| `getByLabelText(/password/i)` | Form inputs with associated `<label>` | Yes |
| `getByPlaceholderText(/search/i)` | Inputs lacking accessible labels | Partially |
| `getByText(/paragraph text/i)` | Static non-interactive text | Yes |
| `getByTestId('custom-chart')` | Canvas, SVG, dynamic graphs | Fallback |

---

## 2. Testing Component Interactions

```tsx
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { describe, it, expect, vi } from 'vitest';
import { Counter } from './Counter';

describe('Counter Component', () => {
  it('increments count on button click', async () => {
    const user = userEvent.setup();
    render(<Counter initialCount={0} />);

    const button = screen.getByRole('button', { name: /increment/i });
    expect(screen.getByText('Count: 0')).toBeInTheDocument();

    await user.click(button);
    expect(screen.getByText('Count: 1')).toBeInTheDocument();
  });
});
```

---

## 3. Asynchronous Waiters

```tsx
// Wait for element to appear in DOM after async fetch
const successAlert = await screen.findByRole('alert');
expect(successAlert).toHaveTextContent(/saved successfully/i);

// Wait for element to disappear from DOM
await waitFor(() => {
  expect(screen.queryByTestId('loading-spinner')).not.toBeInTheDocument();
});
```

---

## 4. Mock Service Worker (MSW) Request Handler

```ts
import { http, HttpResponse } from 'msw';

export const handlers = [
  http.get('/api/user', () => {
    return HttpResponse.json({ id: 'u1', name: 'Dan Abramov' });
  }),
  http.post('/api/login', async ({ request }) => {
    const body = await request.json();
    return HttpResponse.json({ token: 'mock-jwt-token' });
  }),
];
```
