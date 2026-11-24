# React Router v7 Cheatsheet

> Quick-reference guide for React Router v7 configuration, loaders, actions, navigation hooks, and route parameters.

---

## 1. Route Configuration (Data API)

```tsx
import { createBrowserRouter, RouterProvider, Outlet, Link } from 'react-router-dom';

const router = createBrowserRouter([
  {
    path: '/',
    element: <RootLayout />,
    errorElement: <ErrorPage />,
    children: [
      { index: true, element: <HomePage /> },
      {
        path: 'projects/:projectId',
        loader: async ({ params }) => {
          const res = await fetch(`/api/projects/${params.projectId}`);
          if (!res.ok) throw new Response('Not Found', { status: 404 });
          return res.json();
        },
        action: async ({ request, params }) => {
          const formData = await request.formData();
          return updateProject(params.projectId, formData);
        },
        element: <ProjectDetails />,
      },
    ],
  },
]);

export function App() {
  return <RouterProvider router={router} />;
}
```

---

## 2. Essential Navigation Hooks

| Hook | Usage | Purpose |
| :--- | :--- | :--- |
| `useNavigate()` | `const navigate = useNavigate(); navigate('/dashboard');` | Imperative client-side redirection |
| `useParams()` | `const { projectId } = useParams();` | Extract URL path parameters |
| `useSearchParams()` | `const [params, setParams] = useSearchParams();` | Read/write query string (`?filter=active`) |
| `useLoaderData()` | `const data = useLoaderData<ProjectData>();` | Access data returned by route loader |
| `useActionData()` | `const errors = useActionData();` | Access validation data returned by route action |
| `useNavigation()` | `const { state } = useNavigation(); // 'idle' \| 'loading' \| 'submitting'` | Global pending/loading indicator |

---

## 3. Declarative Links & Active Styling

```tsx
import { NavLink } from 'react-router-dom';

<NavLink
  to="/dashboard"
  className={({ isActive, isPending }) =>
    isActive ? 'font-bold text-blue-600' : 'text-slate-600'
  }
>
  Dashboard
</NavLink>
```
