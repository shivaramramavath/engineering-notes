# React Router — Senior Interview Questions & Deep Dives

> React Router v7 architecture, Framework vs Library mode, loaders/actions data flow, error boundaries, and nested layouts.

---

## Question 1: What is the architectural difference between React Router v7 in "Library Mode" versus "Framework Mode" (formerly Remix)?

### The Fundamental Distinction

| Feature | Library Mode | Framework Mode |
| :--- | :--- | :--- |
| **Setup** | Standard Vite/SPA React app (`<RouterProvider />`) | Vite plugin with SSR, prerendering, file-based routing |
| **Bundling** | Client-only bundle; routing runs in browser DOM | Server runtime + client hydration bundles |
| **Loaders Execution** | Run client-side via `fetch` before component renders | Run on the Node/Edge server before HTML is sent |
| **Hydration Strategy** | SPA bootstrapping | Server-rendered streaming HTML with selective hydration |

---

## Question 2: How do Route Loaders and Actions eliminate "Fetch-on-Render" request waterfalls?

### The Fetch-on-Render Waterfall (Anti-pattern)
In traditional SPAs with `useEffect`:
1. `App` renders ➔ fetches user profile (waits 200ms).
2. `Dashboard` renders ➔ fetches project list (waits 300ms).
3. `ProjectCard` renders ➔ fetches team avatars (waits 150ms).
**Total Waterfall: 650ms** of sequential blocking network requests.

### The React Router Loader Solution (Render-as-You-Fetch)
Route loaders are declared alongside route definitions. React Router matches all nested route branches **in parallel** before any component renders:
```ts
export const projectRoute = {
  path: 'projects/:id',
  loader: async ({ params }) => {
    // Both run concurrently before rendering!
    const [project, team] = await Promise.all([
      fetchProject(params.id),
      fetchTeam(params.id),
    ]);
    return { project, team };
  },
  Component: ProjectPage,
};
```
Data dependencies are fetched in a single round-trip, completely eliminating network waterfalls.
