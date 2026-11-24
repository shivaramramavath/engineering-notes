# React Router v7: Library Mode vs Framework Mode & Remix Unification

- **Topic**: `10-routing`
- **React version**: React 19+ (React Router v7.0+)
- **Tested with**: React 19.0.0, Vite 6.0, TypeScript 5.7, Vitest 3.0, `react-router` 7.1.0
- **Bundle / compiler notes**: Covers the official merger of Remix into React Router v7; details the distinct architectures of Library Mode (SPA) vs Framework Mode (SSR/SSG).

---

## 1. Concept Overview

React Router v7 represents the most significant milestone in React navigation history: **the official unification of Remix and React Router**.

Previously, the ecosystem was divided:
- **React Router v6**: A client-side routing library (`react-router-dom`) used in SPAs (Vite, CRA).
- **Remix**: A full-stack web framework built by the same creators that layered SSR, loaders, actions, and Vite plugins over React Router.

In React Router v7, Remix has officially become **React Router Framework Mode**:
- All packages are unified under a single primary import: `import { ... } from 'react-router'`.
- Developers can start in **Library Mode** (a traditional client-side SPA) and seamlessly graduate to **Framework Mode** (full-stack SSR, static pre-rendering, and automated code-splitting) without rewriting their route definitions or application code.

```
React Router v7 Architecture:
┌────────────────────────────────────────────────────────┐
│                       react-router                     │
├───────────────────────────┬────────────────────────────┤
│ Library Mode (SPA)        │ Framework Mode (Full-Stack)│
│ • Client-side routing     │ • Powered by Remix engine  │
│ • Standard Vite SPA       │ • SSR, SSG, Prerendering   │
│ • ClientLoaders & Actions │ • Server Loaders & Actions │
│ • createBrowserRouter()   │ • File-based route typing  │
└───────────────────────────┴────────────────────────────┘
```

### When to Use
- **Library Mode**: Pure client-side Single Page Applications (SPAs) hosted on static CDNs (Cloudflare Pages, S3, Netlify, Vercel Static).
- **Framework Mode**: Applications requiring SEO, dynamic server-side rendering (SSR), automated route code-splitting, or static site pre-generation.

### When NOT to Use
- Next.js projects utilizing the Next.js App Router (Next.js has its own proprietary routing engine).
- Highly type-strict applications that demand 100% end-to-end type safety across search parameters and navigation links (consider TanStack Router).

---

## 2. Key Mechanics & Architecture

### The Two Modes Compared

| Feature | Library Mode | Framework Mode |
| :--- | :--- | :--- |
| **Vite Plugin** | `@vitejs/plugin-react` | `@react-router/dev/vite` |
| **Route Definition** | Code-based (`createBrowserRouter`) | Config or File-based (`routes.ts`) |
| **Rendering Strategy**| Client-side rendering (CSR) | SSR, Static (SSG), or SPA pre-rendering |
| **Data Loading** | `clientLoader` / `clientAction` | `loader` (runs on server) + `clientLoader` |
| **Package Import** | `import { ... } from 'react-router'` | `import { ... } from 'react-router'` |
| **Type Generation** | Manual types | Automatic Route Typegen (`./+types/route`) |

---

## 3. Real-World Code Examples

### Example 1: Basic / Foundational Pattern — Library Mode (Vite SPA)
Setting up React Router v7 in Library Mode using `createBrowserRouter` and `RouterProvider`.

```tsx
import React from 'react';
import { createBrowserRouter, RouterProvider, Link, Outlet } from 'react-router';

function RootLayout() {
  return (
    <div className="min-h-screen bg-zinc-50 text-zinc-900">
      <nav className="p-4 border-b bg-white flex gap-4 text-sm font-medium">
        <Link to="/" className="hover:text-blue-600">Home</Link>
        <Link to="/analytics" className="hover:text-blue-600">Analytics</Link>
      </nav>
      <main className="p-6">
        <Outlet />
      </main>
    </div>
  );
}

function HomePage() {
  return <h2 className="text-xl font-bold">Welcome to the Dashboard</h2>;
}

function AnalyticsPage() {
  return <h2 className="text-xl font-bold">System Analytics</h2>;
}

const router = createBrowserRouter([
  {
    path: '/',
    element: <RootLayout />,
    children: [
      { index: true, element: <HomePage /> },
      { path: 'analytics', element: <AnalyticsPage /> },
    ],
  },
]);

export function App() {
  return <RouterProvider router={router} />;
}
```
Output:
```
// Client-side SPA navigation between Home and Analytics with nested layout persistence.
```

### Example 2: Production / Real-World Integration — Client Loaders in Library Mode
Fetching data before rendering a route using React Router v7's `clientLoader`.

```tsx
import React from 'react';
import { useLoaderData, type ClientLoaderFunctionArgs } from 'react-router';

interface Project {
  id: string;
  name: string;
  stars: number;
}

// clientLoader executes BEFORE the component renders!
export async function clientLoader({ params }: ClientLoaderFunctionArgs): Promise<{ projects: Project[] }> {
  // Simulate network fetch
  await new Promise((r) => setTimeout(r, 400));

  return {
    projects: [
      { id: 'p1', name: 'Antigravity Core', stars: 1240 },
      { id: 'p2', name: 'Oxide Engine', stars: 980 },
    ],
  };
}

export function ProjectsView() {
  // Strongly typed loader data!
  const { projects } = useLoaderData<typeof clientLoader>();

  return (
    <div className="p-4 border rounded-xl max-w-sm">
      <h3 className="font-bold mb-3 text-sm">Active Projects</h3>
      <ul className="divide-y text-xs">
        {projects.map((p) => (
          <li key={p.id} className="py-2 flex justify-between">
            <span className="font-medium">{p.name}</span>
            <span className="text-zinc-500">★ {p.stars}</span>
          </li>
        ))}
      </ul>
    </div>
  );
}
```
Output:
```
// Route renders instantly with pre-fetched data; zero flash of empty state.
```

### Example 3: Edge Case / Framework Mode File-Based Route Configuration
Configuring routes in React Router v7 Framework Mode with automatic Route Typegen.

```ts
// app/routes.ts (Framework Mode route tree configuration)
import { type RouteConfig, index, route, layout } from '@react-router/dev/routes';

export default [
  layout('routes/layout.tsx', [
    index('routes/home.tsx'),
    route('dashboard', 'routes/dashboard.tsx'),
    route('projects/:id', 'routes/project-details.tsx'),
  ]),
] satisfies RouteConfig;
```

```tsx
// app/routes/project-details.tsx (Framework Mode with auto-generated Route types!)
import React from 'react';
import type { Route } from './+types/project-details';

// Server-side loader in Framework Mode:
export async function loader({ params }: Route.LoaderArgs) {
  return {
    projectId: params.id,
    timestamp: new Date().toISOString(),
  };
}

export default function ProjectDetails({ loaderData }: Route.ComponentProps) {
  return (
    <div>
      <h3 className="text-lg font-bold">Project: {loaderData.projectId}</h3>
      <p className="text-xs text-zinc-400">Loaded at: {loaderData.timestamp}</p>
    </div>
  );
}
```
Output:
```
// Server renders HTML on server with loaded data; client hydrates with 100% type safety.
```

### Example 4: Failure Mode / Pitfall & Remediation
**Pitfall**: Importing from deprecated `react-router-dom` in v7.

```tsx
// ❌ WRONG: In React Router v7, importing from `react-router-dom` is deprecated!
import { useNavigate, useLocation } from 'react-router-dom';

// ✅ REMEDIATION: All APIs are unified in core `react-router`!
import { useNavigate, useLocation, Link, NavLink, Outlet } from 'react-router';
```

### Example 5: Idiomatic Modern React 19 Pattern
Route Actions with React 19 native Form Actions in React Router v7.

```tsx
import React from 'react';
import { Form, useActionData, useNavigation, type ClientActionFunctionArgs } from 'react-router';

export async function clientAction({ request }: ClientActionFunctionArgs) {
  const formData = await request.formData();
  const title = formData.get('title') as string;

  if (!title || title.length < 3) {
    return { error: 'Title must be at least 3 characters' };
  }

  return { success: true, title };
}

export function NewTicketScreen() {
  const actionData = useActionData<typeof clientAction>();
  const navigation = useNavigation();
  const isSubmitting = navigation.state === 'submitting';

  return (
    <Form method="post" className="p-4 border rounded max-w-sm space-y-3">
      <h4 className="font-bold text-sm">Create Support Ticket</h4>
      <input name="title" placeholder="Ticket Summary" className="border p-1.5 text-xs w-full rounded" />
      {actionData?.error && <p className="text-red-500 text-xs">{actionData.error}</p>}
      <button type="submit" disabled={isSubmitting} className="w-full bg-zinc-900 text-white py-1.5 rounded text-xs">
        {isSubmitting ? 'Creating...' : 'Create Ticket'}
      </button>
    </Form>
  );
}
```
Output:
```
// Client action intercepts form submission, runs validation, and updates UI with navigation state.
```

---

## 4. Common Anti-Patterns & Gotchas

| Bad Practice | Root Cause | Clean Solution |
| :--- | :--- | :--- |
| **Importing from `react-router-dom` in v7** | v7 unifies all router packages into `react-router`. | Import from `'react-router'` directly. |
| **Using `useEffect` for data fetching on route change** | Causes network waterfall delays and flash of blank content. | Use `loader` or `clientLoader` in route definitions. |
| **Hardcoding navigation paths without route parameters** | Broken routes when paths change. | Use typed routes or central route path constants. |
| **Not handling route error boundaries** | An error in one route crashes the entire application tree. | Export an `ErrorBoundary` component from each route. |
| **Mixing Framework Mode Vite plugin in pure Library Mode** | `@react-router/dev/vite` expects a full-stack directory structure. | Use `@vitejs/plugin-react` for Library Mode SPAs. |
| **Navigating during render phase** | Calling `navigate()` inside component body causes infinite re-render loops. | Navigate inside event handlers or `useEffect`. |

---

## 5. React Internals: Beneath the Abstraction

1. **History Stack Synchronization**: React Router v7 binds to the browser's `window.history` API via custom navigation controllers.
2. **Data Pipeline Execution**: When a navigation event occurs (e.g. user clicks `<Link to="/project/12">`):
   - The router matches the URL path against the registered route tree.
   - It initiates parallel execution of all matched route `loaders` before rendering the new route.
   - If an action was dispatched, it re-validates all active loaders automatically to ensure data freshness.
3. React executes the transition in the Transition Lane, preventing the existing page from vanishing while the new route data loads.

---

## 6. Performance, Bundling & React Compiler

- **Automatic Code Splitting**: In Framework Mode, every route file is automatically split into an independent lazy-loaded JavaScript chunk.
- **Prefetching**: Hovering a `<Link prefetch="intent">` pre-fetches the route's JS chunk and loader data, making navigation feel instantaneous.
- **React Compiler**: The React Compiler memoizes route layouts and component subtrees cleanly.

---

## 7. Accessibility (a11y) & Usability

- **Focus Restoration**: React Router v7 automatically resets focus to the top of the page on route transitions, preventing screen reader users from remaining trapped in the footer.
- **Accessible Current Page**: `<NavLink>` automatically injects `aria-current="page"` when its target URL matches the current browser location.

---

## 8. Testing & Verification Patterns

Testing React Router v7 in Vitest using `createMemoryRouter`:

```tsx
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { describe, it, expect } from 'vitest';
import { createMemoryRouter, RouterProvider } from 'react-router';
import React from 'react';

const routes = [
  { path: '/', element: <div>Home Screen</div> },
  { path: '/settings', element: <div>Settings Screen</div> },
];

describe('React Router v7 Navigation', () => {
  it('renders initial route and navigates cleanly', async () => {
    const router = createMemoryRouter(routes, { initialEntries: ['/'] });
    render(<RouterProvider router={router} />);

    expect(screen.getByText('Home Screen')).toBeInTheDocument();

    await router.navigate('/settings');
    expect(screen.getByText('Settings Screen')).toBeInTheDocument();
  });
});
```

---

## 9. Version History & Migration (React 17 -> 18 -> 19)

| React Version | Behavior / API Status | Migration Notes |
| :--- | :--- | :--- |
| **React Router v6** | Library-only; Remix was a separate framework. | Required manual loader integration. |
| **React Router v7** | Official unification of Remix and React Router. | Two modes: Library Mode and Framework Mode. |
| **React 19** | Full compatibility with React 19 Actions and native transitions. | Recommended router for production React 19 apps. |

---

## 10. Security & Hardening

1. **Open Redirects**: Validate destination URLs when parsing `?redirect=` search parameters to prevent redirecting users to phishing domains.
2. **Route Authorization Guards**: Never rely solely on client-side route redirects to protect sensitive data; always enforce permission checks on server APIs and data loaders.

---

## 11. Real-World Case Study

### Shopify Hydrogen Storefronts

Shopify's headless commerce platform Hydrogen is built directly on the Remix / React Router Framework Mode architecture:
- Sub-50ms SSR response times globally on edge networks.
- Automatic route pre-fetching on product cards ensures 0ms perceived navigation when users click to view item details.

---

## 12. Interview Q&A

<details>
<summary>1. What is the relationship between Remix and React Router v7?</summary>

Remix has been officially merged into React Router v7. React Router v7 unifies both projects: Library Mode provides classic client-side routing for SPAs, while Framework Mode provides the full-stack Remix architecture (SSR, SSG, route loaders, actions, and automatic route code-splitting) under the single `react-router` package.
</details>

<details>
<summary>2. What is the difference between Library Mode and Framework Mode in React Router v7?</summary>

- **Library Mode**: A client-side routing library used inside a standard Vite SPA (`createBrowserRouter`). Runs exclusively in the browser.
- **Framework Mode**: A full-stack web framework configured via `@react-router/dev/vite` that handles server-side rendering (SSR), static site generation (SSG), file-based route typegen, and server data loaders.
</details>

<details>
<summary>3. Why are route `loaders` superior to fetching data in `useEffect`?</summary>

`useEffect` data fetching causes "network waterfalls": the browser must download the JS bundle, render the component, mount it to the DOM, and *then* start fetching data, resulting in layout shifts and loading spinners. Route loaders execute *before* the component renders, allowing parallel data fetching and zero-layout-shift transitions.
</details>

<details>
<summary>4. What is `clientLoader` in React Router v7?</summary>

`clientLoader` is a loader function that runs in the browser in Library Mode (or in client transitions in Framework Mode). It fetches client-side data (e.g. querying indexedDB, localStorage, or client fetch APIs) before the route component mounts.
</details>

<details>
<summary>5. How does `NavLink` support accessibility?</summary>

`<NavLink>` automatically adds `aria-current="page"` to the rendered HTML anchor tag when its `to` destination matches the active route, allowing screen readers to announce which link is currently active.
</details>

<details>
<summary>6. How does React Router v7 handle automatic re-validation?</summary>

When an `action` or `clientAction` completes successfully, React Router automatically re-runs all active route loaders on the page to guarantee that the UI reflects the latest server/client mutations without manual query invalidation.
</details>

<details>
<summary>7. What package do you import in React Router v7?</summary>

Everything is imported directly from `'react-router'` (e.g., `import { createBrowserRouter, Link, Outlet, useLoaderData } from 'react-router'`). The legacy `react-router-dom` package is deprecated.
</details>

<details>
<summary>8. How do you test routes in isolation without a real browser window?</summary>

Using `createMemoryRouter(routes, { initialEntries: ['/test-url'] })` from `react-router`, which creates an in-memory history stack compatible with Vitest and JSDOM.
</details>

---

## 13. Practical Challenges & Exercises

<details>
<summary>Exercise 1 (Easy): Basic Layout Route with Active Navigation</summary>

**Specification**: Build a layout component with `<NavLink>` items highlighting active links.

```tsx
import { NavLink, Outlet } from 'react-router';

export function NavLayout() {
  return (
    <div>
      <nav className="flex gap-4 p-3 border-b text-xs">
        <NavLink to="/" className={({ isActive }) => isActive ? 'font-bold underline' : ''}>Home</NavLink>
        <NavLink to="/about" className={({ isActive }) => isActive ? 'font-bold underline' : ''}>About</NavLink>
      </nav>
      <Outlet />
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Dynamic Route Parameter Display</summary>

**Specification**: Route component reading dynamic `:invoiceId` via `useParams()`.

```tsx
import { useParams } from 'react-router';

export function InvoiceView() {
  const { invoiceId } = useParams();
  return <div className="p-4 text-sm font-mono">Viewing Invoice: {invoiceId}</div>;
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Client Loader with Error Handling</summary>

**Specification**: Implement a `clientLoader` that throws a 404 response if an entity is not found.

```tsx
import { useLoaderData, type ClientLoaderFunctionArgs } from 'react-router';

export async function clientLoader({ params }: ClientLoaderFunctionArgs) {
  const res = await fetch(`/api/items/${params.id}`);
  if (!res.ok) throw new Response('Not Found', { status: 404 });
  return res.json();
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Search Parameter Synchronization Hook</summary>

**Specification**: Custom hook syncing search queries with URL search params via `useSearchParams()`.

```tsx
import { useSearchParams } from 'react-router';

export function useSearchParamSync(key = 'q') {
  const [params, setParams] = useSearchParams();
  const query = params.get(key) || '';
  const setQuery = (val: string) => {
    setParams(prev => {
      val ? prev.set(key, val) : prev.delete(key);
      return prev;
    });
  };
  return [query, setQuery] as const;
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Route Error Boundary with Retry</summary>

**Specification**: Build an `ErrorBoundary` that catches route loader failures and displays a retry button.

```tsx
import { useRouteError, isRouteErrorResponse, useNavigate } from 'react-router';

export function RouteErrorDisplay() {
  const error = useRouteError();
  const navigate = useNavigate();

  return (
    <div className="p-6 border border-red-200 bg-red-50 rounded text-red-800 text-xs">
      <h3 className="font-bold">Error Loading Route</h3>
      <p>{isRouteErrorResponse(error) ? `${error.status} ${error.statusText}` : 'An unexpected error occurred.'}</p>
      <button onClick={() => navigate(0)} className="mt-2 bg-red-600 text-white px-3 py-1 rounded">Retry</button>
    </div>
  );
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Protected Route Guard with Redirect Target Memory</summary>

**Specification**: Build an authentication route guard that preserves intended destination URL and redirects on login.

```tsx
import { Navigate, useLocation, Outlet } from 'react-router';

export function AuthGuard({ isAuthenticated }: { isAuthenticated: boolean }) {
  const location = useLocation();
  if (!isAuthenticated) {
    return <Navigate to={`/login?redirect=${encodeURIComponent(location.pathname)}`} replace />;
  }
  return <Outlet />;
}
```
</details>
