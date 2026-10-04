# Project Structure

A fresh Vite + React + TypeScript project contains about a dozen files. Knowing what each one does removes the "magic" and tells you what is safe to delete, edit, or ignore.

## Prerequisites

[`02-vite.md`](./02-vite.md) — a project created with the `react-ts` template.

---

## What the template generates

```
my-app/
├── public/
│   └── vite.svg
├── src/
│   ├── assets/
│   │   └── react.svg
│   ├── App.css
│   ├── App.tsx
│   ├── index.css
│   ├── main.tsx
│   └── vite-env.d.ts
├── .gitignore
├── eslint.config.js
├── index.html
├── package.json
├── package-lock.json
├── tsconfig.json
├── tsconfig.app.json
├── tsconfig.node.json
└── vite.config.ts
```

Exact files vary slightly between template versions, but the roles stay the same.

---

## The entry path: `index.html` → `main.tsx` → `App.tsx`

### `index.html`

Unlike older toolchains, `index.html` lives at the **project root** and is the real entry point:

```html
<body>
  <div id="root"></div>
  <script type="module" src="/src/main.tsx"></script>
</body>
```

React will render into `#root`. Edit this file to change the page title, favicon, or meta tags.

### `src/main.tsx`

```tsx
import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import App from "./App";
import "./index.css";

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <App />
  </StrictMode>
);
```

- `createRoot` attaches React to the DOM element.
- `StrictMode` is a development-only wrapper that surfaces bugs (for example, it runs some code twice on purpose). It does nothing in production. See `../02-state-and-rendering/03-rendering.md`.
- The `!` after `getElementById(...)` is a TypeScript non-null assertion: you're telling the compiler the element exists.

### `src/App.tsx`

Your root component. Everything else in the UI is rendered from here.

---

## Other files

| File | Purpose |
|------|---------|
| `public/` | Static files served as-is at the site root (`/vite.svg`). Use for favicons, `robots.txt`, files referenced by absolute URL |
| `src/assets/` | Files you `import` in code (images, fonts). Vite hashes and optimizes them |
| `src/index.css` | Global styles |
| `src/vite-env.d.ts` | Type declarations for Vite features like `import.meta.env` |
| `vite.config.ts` | Vite configuration |
| `tsconfig.json` | Root TypeScript config that references the two below |
| `tsconfig.app.json` | Compiler options for your app code in `src/` |
| `tsconfig.node.json` | Compiler options for tooling files like `vite.config.ts` |
| `eslint.config.js` | Linting rules |

**`public/` vs `src/assets/`:** if you `import` it, it goes in `src/assets/` (bundled, cache-busted). If you reference it by fixed URL, it goes in `public/`.

---

## A sensible first cleanup

Start every new project by removing the demo content:

1. Replace the contents of `App.tsx` with a single heading.
2. Delete `App.css` and the demo logos.
3. Trim `index.css` to a minimal reset or your own base styles.

```tsx
export default function App() {
  return <h1>Hello, React</h1>;
}
```

---

## Growing the `src/` folder

The starter has no structure beyond `App.tsx`. As the app grows, a common first step is:

```
src/
├── components/   # reusable UI pieces
├── hooks/        # custom hooks
├── pages/        # route-level components
├── lib/          # helpers, API client
└── App.tsx
```

This works for small apps. Larger apps organize by feature instead — see `../20-frontend-architecture/00-feature-based-architecture.md`.

---

## Common mistakes

- **Editing `index.html` expecting JSX** — it's plain HTML; React renders into `#root`.
- **Putting imported images in `public/`** — they skip hashing and optimization.
- **Deleting `tsconfig.node.json`** — breaks type-checking for `vite.config.ts`.
- **Leaving demo CSS in place** — starter styles (centered `#root`, max-width) cause confusing layout.
- **Treating `StrictMode` double-logging as a bug** — it's intentional in development.

## Quick summary

- `index.html` at the root is the entry; it loads `src/main.tsx`
- `main.tsx` creates the React root and renders `<App />` inside `StrictMode`
- `public/` is for static files by URL; `src/assets/` is for imported files
- Three `tsconfig` files split app code from tooling code
- Clear out the demo content first, then add structure as the app grows

## Next

**[`04-react-devtools.md`](./04-react-devtools.md)** shows how to inspect the components this project renders.
