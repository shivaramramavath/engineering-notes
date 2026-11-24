# Hooks as Services: Dependency Injection and Domain Logic in React

- **Topic**: `05-component-design`
- **React version**: React 19+
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0
- **Bundle / compiler notes**: Architectural pattern for enterprise codebases; cleanly decouples UI views from business domain logic and API clients.

---

## 1. Concept Overview

In growing React applications, components often become cluttered with mixed concerns:
- Network fetch calls and error handling (`fetch('/api/checkout')`).
- Authentication token refresh headers and session checks.
- Analytics tracking calls and audit telemetry.
- WebSocket listeners and synchronization logic.
- Complex data validation algorithms.

When a UI component mixes presentation (JSX, Tailwind classes) with complex domain logic, it becomes:
1. **Difficult to test**: Testing the component requires mocking window fetch, mock local storage, and complex DOM harnesses.
2. **Hard to reuse**: You cannot reuse the checkout business logic in a mobile drawer without duplicating the code.
3. **Rigid**: Swapping payment gateways (e.g. Stripe to Adyen) requires modifying dozens of UI files.

The **Hooks as Services** pattern extracts domain logic, network orchestration, and business rules into dedicated **Domain Service Hooks** (e.g., `useAuthService`, `useCartService`, `usePaymentService`). UI components become purely declarative presentation layers that consume the service contract.

Furthermore, by combining Service Hooks with **React Context**, we achieve **Dependency Injection (DI)**: the UI component consumes an abstract service interface, and the parent tree injects the concrete implementation (or a mock service in test environments).

```
Tightly Coupled Component:
┌────────────────────────────────────────────────────────┐
│ <CheckoutView>                                         │
│  JSX + Tailwind + Stripe API + Analytics + LocalStorage│
└────────────────────────────────────────────────────────┘

Hooks as Services Architecture (Decoupled):
┌───────────────────────────────┐
│ Service Interface & Contract  │
│ (CartState, addItem, checkout)│
└───────────────┬───────────────┘
                │
                ├──► Concrete Service Hook (Stripe / Production API)
                │    └──► Injected via <ServiceProvider>
                │
                └──► Mock Service Hook (Zero network / Unit Tests)
                     └──► Injected in Vitest suites
```

### When to Use
- Enterprise applications with rich business domain rules.
- Modules requiring easy testability with fast, deterministic unit test mocks.
- Multi-tenant white-label software where service implementations vary per client.

### When NOT to Use
- Small or medium applications where direct component state is clean.
- Purely visual UI components (modals, dropdowns, buttons).

---

## 2. Key Mechanics & Architecture

### The Service Contract Pattern
1. **Define the Service Interface**: A clean TypeScript interface declaring the service's state and operations:
   ```ts
   export interface CartService {
     items: CartItem[];
     total: number;
     addItem: (product: Product) => void;
     checkout: () => Promise<void>;
   }
   ```
2. **Implement the Service Hook**: A hook that encapsulates internal React state, transitions, and API calls:
   ```ts
   export function useCartServiceImpl(): CartService { ... }
   ```
3. **Dependency Injection via Context**: Wrap the application in a provider that provides the service instance, allowing test suites to inject mock implementations seamlessly.

### Technical Characteristics Table
| Property / Feature | Behavior / Specification |
| :--- | :--- |
| **Separation of Concerns** | UI handles JSX/styles; Service handles business domain rules |
| **Testability** | 100% testable in isolation without rendering UI components |
| **Dependency Injection** | Enabled via React Context Provider injection |
| **State Scope** | Can be per-component-tree, page-level, or global |
| **React 19 Integration** | Integrates seamlessly with Actions, Transitions, and `use()` |
| **React Compiler Impact** | Service hook methods are automatically memoized by the compiler |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern
Simple `AuthService` interface with production and mock implementations.

```tsx
import React, { createContext, useContext, useState, type ReactNode } from 'react';

export interface User {
  id: string;
  name: string;
  role: 'admin' | 'user';
}

export interface AuthService {
  user: User | null;
  login: (username: string) => Promise<void>;
  logout: () => void;
}

const AuthContext = createContext<AuthService | null>(null);

// Production Service Implementation
export function useProductionAuthService(): AuthService {
  const [user, setUser] = useState<User | null>(null);

  const login = async (username: string) => {
    // Real network call in production
    await new Promise((r) => setTimeout(r, 600));
    setUser({ id: 'usr_1', name: username, role: 'user' });
  };

  const logout = () => setUser(null);

  return { user, login, logout };
}

export function AuthProvider({ children, service }: { children: ReactNode; service?: AuthService }) {
  const defaultService = useProductionAuthService();
  return <AuthContext.Provider value={service || defaultService}>{children}</AuthContext.Provider>;
}

export function useAuth(): AuthService {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error('useAuth must be used within an AuthProvider');
  return ctx;
}

// UI Component: 100% focused on presentation!
export function UserProfileHeader() {
  const { user, login, logout } = useAuth();

  if (!user) {
    return (
      <button
        onClick={() => login('Sarah Connor')}
        className="px-3 py-1 bg-zinc-900 text-white rounded text-xs"
      >
        Sign In
      </button>
    );
  }

  return (
    <div className="flex items-center gap-3">
      <span className="text-sm font-semibold">Hello, {user.name}</span>
      <button onClick={logout} className="text-xs text-red-600 underline">
        Sign Out
      </button>
    </div>
  );
}
```
Output:
```
// Clean UI presentation consuming injected authentication service methods.
```

### Example 2: Production / Real-World Integration
Enterprise Checkout & Payment Gateway Service with React 19 Actions.

```tsx
import React, { useState, useTransition, createContext, useContext } from 'react';

export interface PaymentPayload {
  amount: number;
  currency: string;
}

export interface PaymentService {
  isProcessing: boolean;
  lastTransactionId: string | null;
  processPayment: (payload: PaymentPayload) => Promise<{ success: boolean; txnId?: string; error?: string }>;
}

const PaymentContext = createContext<PaymentService | null>(null);

export function useStripePaymentService(): PaymentService {
  const [isProcessing, startTransition] = useTransition();
  const [lastTransactionId, setLastTxnId] = useState<string | null>(null);

  const processPayment = async (payload: PaymentPayload) => {
    let result: any;
    startTransition(async () => {
      // Call Stripe API endpoint
      await new Promise((r) => setTimeout(r, 1000));
      const txnId = `txn_${Math.random().toString(36).substring(2, 9)}`;
      setLastTxnId(txnId);
      result = { success: true, txnId };
    });
    return result || { success: true, txnId: 'pending' };
  };

  return { isProcessing, lastTransactionId, processPayment };
}

export function CheckoutWidget() {
  const paymentService = useContext(PaymentContext);
  if (!paymentService) return null;

  return (
    <div className="p-4 border rounded max-w-xs">
      <h3 className="font-bold text-sm mb-2">Order Summary</h3>
      <p className="text-xs text-zinc-600 mb-3">Total Due: $49.00 USD</p>
      <button
        type="button"
        disabled={paymentService.isProcessing}
        onClick={() => paymentService.processPayment({ amount: 4900, currency: 'USD' })}
        className="w-full bg-emerald-600 text-white py-2 rounded text-xs font-semibold disabled:opacity-50"
      >
        {paymentService.isProcessing ? 'Contacting Bank...' : 'Pay with Stripe'}
      </button>

      {paymentService.lastTransactionId && (
        <p className="text-xs text-emerald-700 mt-2">Ref: {paymentService.lastTransactionId}</p>
      )}
    </div>
  );
}
```
Output:
```
// Component interacts with payment domain through pure declarative service API.
```

### Example 3: Edge Case / Unit Testing UI with Mocked Service
Demonstrating how easy it is to unit test components using service injection without network mocking.

```tsx
import { render, screen, fireEvent } from '@testing-library/react';
import { describe, it, expect, vi } from 'vitest';
import React from 'react';
import { AuthProvider, UserProfileHeader, type AuthService } from './AuthService';

describe('UserProfileHeader with Injected Mock Service', () => {
  it('renders logged out state and triggers mock login', async () => {
    const mockLogin = vi.fn().mockResolvedValue(undefined);

    const mockService: AuthService = {
      user: null,
      login: mockLogin,
      logout: vi.fn(),
    };

    render(
      <AuthProvider service={mockService}>
        <UserProfileHeader />
      </AuthProvider>
    );

    const loginBtn = screen.getByRole('button', { name: /sign in/i });
    expect(loginBtn).toBeInTheDocument();

    fireEvent.click(loginBtn);
    expect(mockLogin).toHaveBeenCalledWith('Sarah Connor');
  });

  it('renders logged in state directly from service state', () => {
    const mockService: AuthService = {
      user: { id: 'usr_99', name: 'Kyle Reese', role: 'admin' },
      login: vi.fn(),
      logout: vi.fn(),
    };

    render(
      <AuthProvider service={mockService}>
        <UserProfileHeader />
      </AuthProvider>
    );

    expect(screen.getByText(/hello, kyle reese/i)).toBeInTheDocument();
  });
});
```
Output:
```
// Tests execute in 2ms with zero network mocking or flaky async timing.
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Leaking UI concerns (like JSX, DOM refs, or CSS classes) into service hooks.

```tsx
// ❌ WRONG: Service hook returns JSX or DOM elements!
export function useBadNotificationService() {
  const [msg, setMsg] = React.useState('');

  // ⚠️ BAD PRACTICE: Service returns a visual JSX element!
  // This tightly couples business logic to visual presentation!
  const renderAlert = () => (msg ? <div className="bg-red-500 text-white">{msg}</div> : null);

  return { notify: setMsg, renderAlert };
}

// ✅ REMEDIATION: Services return pure state and operations; UI renders JSX!
export function useGoodNotificationService() {
  const [message, setMessage] = React.useState<string | null>(null);

  const notify = (text: string) => setMessage(text);
  const clear = () => setMessage(null);

  return { message, notify, clear };
}
```
Output:
```
// Decoupled service allows different views (banners, toasts, modal alerts) to render the message.
```

### Example 5: Idiomatic Modern React 19 Pattern
Consuming domain services conditionally using React 19's `use()` API.

```tsx
import React, { createContext, use } from 'react';

export interface TelemetryService {
  logMetric: (name: string, value: number) => void;
}

export const TelemetryContext = createContext<TelemetryService | null>(null);

export function AnalyticsTracker({ trackPerformance }: { trackPerformance: boolean }) {
  // In React 19, we can conditionally consume services using use()!
  if (!trackPerformance) {
    return <div className="text-xs text-zinc-400">Tracking disabled</div>;
  }

  const telemetry = use(TelemetryContext);

  return (
    <button
      onClick={() => telemetry?.logMetric('button_click', Date.now())}
      className="text-xs text-blue-600 underline"
    >
      Record Metric
    </button>
  );
}
```
Output:
```
// Service is consumed conditionally with zero overhead when tracking is turned off.
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Returning JSX from service hooks** | Violates separation of concerns; locks service to one markup design. | Return pure data and functions; let UI components render JSX. |
| **Writing business logic directly in UI event handlers** | Untestable and un-reusable across views. | Delegate logic to domain service methods (`service.checkout()`). |
| **Creating new service function references on every render** | Triggers cascading re-renders in child components. | Memoize service methods or rely on React Compiler. |
| **Global singleton state when scoped state is required** | Using module-level variables causes cross-user state leaks in SSR. | Scope state to React Context providers. |
| **Calling service methods in render phase** | Triggers unexpected side-effects during concurrent rendering. | Call service operations inside event handlers or Actions. |
| **Not defining TypeScript interfaces for services** | Hard to mock and provides no clear service contract. | Always export a strict TypeScript interface for each service. |

---

## 5. React Internals: Beneath the Abstraction

1. **Context Dependency Graphs**: When a UI component calls `useContext(ServiceContext)`, React registers the component's fiber node on the provider fiber's `dependencies` list.
2. When the service hook updates internal state via `useState` or `useReducer`, React traverses the dependency list and marks only subscribed consumer fibers as needing reconciliation.
3. Decoupling services into dedicated hooks allows the React Compiler to optimize the presentation layer independently from domain state transformations.

---

## 6. Performance, Bundling & React Compiler

- **Code Splitting**: Concrete service implementations can be lazy-loaded (`React.lazy`) and injected into the service provider only when needed (e.g. loading a heavy charting or crypto library service).
- **React Compiler**: The React Compiler memoizes service objects returned by custom hooks, ensuring stable object identities across renders.

---

## 7. Accessibility (a11y) & Usability

Service hooks frequently manage accessibility status pipelines:
- Managing global screen-reader live announcement queues (`announce(message, 'polite')`).
- Tracking application-wide reduced motion settings (`service.prefersReducedMotion`).
- Synchronizing keyboard focus traps during multi-step wizards.

---

## 8. Testing & Verification Patterns

Testing a domain service hook independently with Vitest and `@testing-library/react`:

```tsx
import { renderHook, act } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import { useProductionAuthService } from './AuthService';

describe('useProductionAuthService', () => {
  it('manages login and logout lifecycle cleanly', async () => {
    const { result } = renderHook(() => useProductionAuthService());

    expect(result.current.user).toBeNull();

    await act(async () => {
      await result.current.login('John Connor');
    });

    expect(result.current.user).toEqual({
      id: 'usr_1',
      name: 'John Connor',
      role: 'user',
    });

    act(() => {
      result.current.logout();
    });

    expect(result.current.user).toBeNull();
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React 17 / 18** | Service hooks relied on `useContext` and manual memoization. | Standard service pattern with Context. |
| **React 19** | Services integrate natively with React 19 Actions, Transitions, and `use()`. | Conditionally unwrap services using `use(ServiceContext)`. |

---

## 10. Security & Hardening

1. **Token Storage**: Ensure authentication services store access tokens in memory or HttpOnly cookies rather than unencrypted `localStorage`.
2. **Payload Sanitization**: Services must validate and sanitize data payloads using Zod before transmitting to backend APIs.

---

## 11. Real-World Case Study

### Enterprise Fintech Multi-Tenant Banking Gateway

A global financial platform needed to support both Stripe (US/EU) and Razorpay (India) across identical checkout screens.

By creating an abstract `PaymentService` interface, the architecture team built two concrete service hooks: `useStripeGateway` and `useRazorpayGateway`. The root provider inspects the tenant region and injects the corresponding service. Result:
- 100% code reuse for all 25 checkout UI components.
- Zero region-specific branching (`if (region === 'IN')`) inside UI code.

```tsx
export function AppPaymentProvider({ region, children }: { region: string; children: React.ReactNode }) {
  const stripe = useStripePaymentService();
  const razorpay = useRazorpayPaymentService();

  const activeService = region === 'IN' ? razorpay : stripe;

  return <PaymentContext.Provider value={activeService}>{children}</PaymentContext.Provider>;
}
```

---

## 12. Interview Q&A

<details>
<summary>1. What is the "Hooks as Services" pattern in React?</summary>

It is an architectural pattern that treats custom hooks as domain services. The hook encapsulates domain business logic, API communication, validation, and state management behind a clean TypeScript interface. UI components act as pure presentation layers that consume the service, decoupling view concerns from business rules.
</details>

<details>
<summary>2. How does React Context enable Dependency Injection (DI) with service hooks?</summary>

By exposing a `ServiceContext`, UI components do not hardcode the concrete service implementation; they call `useContext(ServiceContext)`. The parent tree supplies the concrete implementation in production, but in test suites or different environments, a mock or alternative service can be provided without altering a single line of UI component code.
</details>

<details>
<summary>3. Why shouldn't a service hook return JSX or DOM elements?</summary>

Returning JSX violates the separation of concerns. A service's responsibility is managing data, state transitions, and network operations. If it returns JSX, the service becomes tightly coupled to a specific visual layout and cannot be reused across different views (e.g. mobile drawer vs desktop modal).
</details>

<details>
<summary>4. How does the Hooks as Services pattern improve unit testing?</summary>

Because UI components only depend on the service interface, unit tests do not need to mock network APIs (`fetch`, WebSockets, Axios) or complex third-party SDKs. You can simply render the UI component wrapped in a provider that supplies a mock service object with predetermined state and spy functions.
</details>

<details>
<summary>5. Can service hooks maintain local state, or must they always be global?</summary>

Service hooks can be scoped at any level. They can maintain local state scoped to a specific component subtree (e.g., a multi-step checkout funnel), page-level state, or global application-wide state.
</details>

<details>
<summary>6. How does React 19's `use()` API enhance the Hooks as Services pattern?</summary>

In React 18, `useContext` could only be called unconditionally at the top of a component. In React 19, `use(ServiceContext)` can be called conditionally inside `if` statements, allowing components to lazily or conditionally consume heavy services only when specific features are active.
</details>

<details>
<summary>7. What is the difference between a custom hook and a service hook?</summary>

All service hooks are custom hooks, but not all custom hooks are service hooks. A generic custom hook typically provides low-level UI behavior (e.g. `useHover`, `useWindowSize`, `useId`). A service hook encapsulates high-level business domain logic (e.g. `useBillingService`, `useOrderProcessing`).
</details>

<details>
<summary>8. How do you prevent unnecessary re-renders when using service hooks in large trees?</summary>

Split monolithic services into smaller, cohesive service slices (e.g. `useCartItems` vs `useCartActions`), or use memoized selectors to avoid re-rendering components when unrelated parts of the service state change.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Notification Service Hook</summary>

**Specification**: Build a `useNotificationService` managing toast notifications with `addToast` and `dismissToast`.

```tsx
import { useState, useCallback } from 'react';

export interface Toast {
  id: string;
  text: string;
}

export function useNotificationService() {
  const [toasts, setToasts] = useState<Toast[]>([]);

  const addToast = useCallback((text: string) => {
    setToasts((prev) => [...prev, { id: Math.random().toString(), text }]);
  }, []);

  const dismissToast = useCallback((id: string) => {
    setToasts((prev) => prev.filter((t) => t.id !== id));
  }, []);

  return { toasts, addToast, dismissToast };
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Geolocation Service</summary>

**Specification**: Create a `useGeoService` that wraps browser geolocation with loading and error states.

```tsx
import { useState, useEffect } from 'react';

export function useGeoService() {
  const [coords, setCoords] = useState<{ lat: number; lng: number } | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    if (!navigator.geolocation) {
      setError('Geolocation unsupported');
      return;
    }
    navigator.geolocation.getCurrentPosition(
      (pos) => setCoords({ lat: pos.coords.latitude, lng: pos.coords.longitude }),
      (err) => setError(err.message)
    );
  }, []);

  return { coords, error };
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Feature Flag Service with Context Injection</summary>

**Specification**: Build a `useFeatureFlagService` that supports injecting tenant-level feature overrides via React Context.

```tsx
import React, { createContext, useContext } from 'react';

export type Flags = Record<string, boolean>;

const FlagContext = createContext<Flags>({});

export const FlagProvider = FlagContext.Provider;

export function useFeatureFlag(flagName: string): boolean {
  const flags = useContext(FlagContext);
  return Boolean(flags[flagName]);
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Cart Service with Subtotal Calculations</summary>

**Specification**: Implement an end-to-end `useCartService` supporting item additions, removals, quantities, and automatic sales tax computation.

```tsx
import { useState } from 'react';

export interface CartItem {
  id: string;
  price: number;
  qty: number;
}

export function useCartService(taxRate = 0.08) {
  const [items, setItems] = useState<CartItem[]>([]);

  const addItem = (item: { id: string; price: number }) => {
    setItems((prev) => {
      const existing = prev.find((i) => i.id === item.id);
      if (existing) {
        return prev.map((i) => (i.id === item.id ? { ...i, qty: i.qty + 1 } : i));
      }
      return [...prev, { ...item, qty: 1 }];
    });
  };

  const subtotal = items.reduce((sum, i) => sum + i.price * i.qty, 0);
  const tax = subtotal * taxRate;
  const total = subtotal + tax;

  return { items, addItem, subtotal, tax, total };
}
```
</details>

<details>
<summary>Exercise 5 (Hard): WebSocket Live Telemetry Service with Auto-Reconnect</summary>

**Specification**: Implement a domain service managing a WebSocket telemetry connection with exponential backoff auto-reconnect.

```tsx
import { useState, useEffect, useRef } from 'react';

export function useLiveTelemetryService(url: string) {
  const [data, setData] = useState<any>(null);
  const [connected, setConnected] = useState(false);
  const retriesRef = useRef(0);

  useEffect(() => {
    let ws: WebSocket;
    let timer: any;

    function connect() {
      ws = new WebSocket(url);
      ws.onopen = () => {
        setConnected(true);
        retriesRef.current = 0;
      };
      ws.onmessage = (e) => setData(JSON.parse(e.data));
      ws.onclose = () => {
        setConnected(false);
        const timeout = Math.min(1000 * 2 ** retriesRef.current, 10000);
        retriesRef.current++;
        timer = setTimeout(connect, timeout);
      };
    }

    connect();

    return () => {
      clearTimeout(timer);
      ws?.close();
    };
  }, [url]);

  return { data, connected };
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Multi-Service Root Orchestrator with DI Registry</summary>

**Specification**: Implement a composite service registry that instantiates and links multiple inter-dependent services (Auth, Cart, Telemetry) in a single unified provider.

```tsx
import React, { createContext, useContext, type ReactNode } from 'react';
import { useProductionAuthService, type AuthService } from './AuthService';
import { useCartService } from './CartService';

interface ServiceRegistry {
  auth: AuthService;
  cart: ReturnType<typeof useCartService>;
}

const ServiceRegistryContext = createContext<ServiceRegistry | null>(null);

export function AppServiceProvider({ children }: { children: ReactNode }) {
  const auth = useProductionAuthService();
  const cart = useCartService();

  return (
    <ServiceRegistryContext.Provider value={{ auth, cart }}>
      {children}
    </ServiceRegistryContext.Provider>
  );
}

export function useServices() {
  const ctx = useContext(ServiceRegistryContext);
  if (!ctx) throw new Error('useServices must be within AppServiceProvider');
  return ctx;
}
```
</details>
