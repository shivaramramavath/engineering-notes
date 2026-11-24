# React Router v7 — Basic to Advanced

A comprehensive reference card for `react-router` and `react-router-dom` v7 in React 19.

> **Target Version:** React Router v7.1+  
> **Evolution:** Unification of React Router v6 and Remix  
> **Modes:** Library Mode (SPA) vs Framework Mode (Fullstack SSR/SSG)  

---

## Level 0 — Concepts

- **Declarative Route Tree**: Maps URLs to nested component hierarchies.
- **Nested Outlets**: `<Outlet />` renders child route components while keeping parent layouts mounted.
- **Data Routers**: Route `loader` and `action` functions eliminate waterfall requests by fetching data in parallel with route chunk downloads.
- **Core Primitives Table**:

| API | Type | Purpose |
| :--- | :--- | :--- |
| `createBrowserRouter()` | Function | Configures standard data router instance |
| `RouterProvider` | Component | Mounts router instance into React tree |
| `<Outlet />` | Component | Viewport placeholder rendering matched child route |
| `<Link>` / `<NavLink>` | Component | Declarative client-side navigation links |
| `useNavigate()` | Hook | Programmatic navigation handler |
| `useParams()` | Hook | Extracts dynamic URL path parameters |
| `useSearchParams()` | Hook | Reads and updates URL query string parameters |
| `useLoaderData()` | Hook | Accesses data resolved by the route's `loader` function |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npm install react-router-dom
```

### 1.2 Router Configuration with Nested Layout
```tsx
// src/router.tsx
import { createBrowserRouter, RouterProvider, Outlet, Link } from 'react-router-dom';
import React from 'react';

function RootLayout() {
  return (
    <div>
      <nav className="p-4 border-b flex gap-4">
        <Link to="/" className="text-blue-600 font-bold">Home</Link>
        <Link to="/dashboard" className="text-blue-600">Dashboard</Link>
      </nav>
      <main className="p-4">
        <Outlet />
      </main>
    </div>
  );
}

export const router = createBrowserRouter([
  {
    path: '/',
    element: <RootLayout />,
    children: [
      { index: true, element: <h2>Home Page</h2> },
      { path: 'dashboard', element: <h2>Dashboard Overview</h2> },
    ],
  },
]);

export function App() {
  return <RouterProvider router={router} />;
}
```

---

## Level 2 — Intermediate: Dynamic Routes & Loaders

```tsx
import { useLoaderData, useParams, type LoaderFunctionArgs } from 'react-router-dom';

export async function userLoader({ params }: LoaderFunctionArgs) {
  const res = await fetch(`/api/users/${params.userId}`);
  if (!res.ok) throw new Response('User Not Found', { status: 404 });
  return res.json();
}

export function UserProfileView() {
  const user = useLoaderData() as { id: string; name: string };
  const { userId } = useParams();

  return (
    <div>
      <h3>User #{userId}</h3>
      <p>Name: {user.name}</p>
    </div>
  );
}
```

---

## Level 3 — Advanced: View Transitions & Route Protection

```tsx
import { NavLink, Navigate, Outlet } from 'react-router-dom';

// 1. Native View Transitions support
export function Nav() {
  return <NavLink to="/profile" viewTransition>Profile</NavLink>;
}

// 2. Auth Guard
export function ProtectedRoute({ isAuthenticated }: { isAuthenticated: boolean }) {
  if (!isAuthenticated) {
    return <Navigate to="/login" replace />;
  }
  return <Outlet />;
}
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Using `window.location.href` for Navigation** | Triggers full browser document reload, wiping React memory state. | Always use `<Link>` or `useNavigate()`. |
| **Data Fetching Inside `useEffect` on Route Change**| Introduces sequential waterfall: download JS -> mount -> fetch data. | Use route `loader` functions to parallelize fetching. |
| **Missing Catch-All 404 Route (`path: '*'`)** | Unknown URLs render blank screens without error feedback. | Provide a root error element and `path: '*'` fallback. |
| **Directly Storing URL State in `useState`** | Breaks browser back button, sharing, and deep linking. | Sync query filters using `useSearchParams`. |
