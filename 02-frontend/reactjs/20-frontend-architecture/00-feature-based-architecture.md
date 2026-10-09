# Feature-Based Architecture

The first structure most React apps adopt is **grouping by file type**:

```text
src/
├── components/    # 150 components from every part of the app
├── hooks/         # 60 hooks
├── utils/         # 90 helpers
├── api/
├── types/
└── pages/
```

It works for the first ten screens. Then a single change ("add a coupon to checkout") touches five folders, `components/` becomes an unsearchable pile, nobody knows which hooks are safe to change, and every PR collides in the same directories.

**Feature-based architecture** groups by **what the code is for**: one folder per product capability, containing everything that capability needs. Code that changes together lives together.

## The structure

```text
src/
├── app/
│   ├── App.tsx                # providers (query client, router, theme), error boundary
│   ├── router.tsx             # route tree
│   └── styles/
├── features/
│   ├── auth/
│   ├── projects/
│   │   ├── api/
│   │   │   ├── projects.api.ts        # endpoints
│   │   │   └── projects.queries.ts    # query keys + queryOptions
│   │   ├── components/
│   │   │   ├── ProjectTable.tsx
│   │   │   ├── ProjectTable.test.tsx
│   │   │   └── NewProjectDialog.tsx
│   │   ├── hooks/
│   │   │   └── useProjectFilters.ts
│   │   ├── domain/
│   │   │   ├── project.types.ts
│   │   │   └── project-status.ts      # pure logic
│   │   └── index.ts                   # PUBLIC API of the feature
│   └── billing/
├── shared/
│   ├── ui/                    # Button, Dialog, … (the design-system primitives)
│   ├── lib/                   # api client, env, logger, utils
│   ├── hooks/                 # generic hooks
│   └── config/
└── routes/                    # page components that compose features
    ├── ProjectsPage.tsx
    └── ProjectDetailPage.tsx
```

Everything about "projects" (its API calls, UI, state, types, tests) is in `features/projects/`. Delete the folder and the capability is gone. Onboard someone to the projects area and you hand them one directory.

### What goes where

| Code | Home |
|---|---|
| UI specific to one capability (`ProjectTable`) | `features/<name>/components` |
| Generic UI primitives (`Button`, `Dialog`, `Tabs`) | `shared/ui` |
| API calls and query definitions for a capability | `features/<name>/api` |
| The HTTP client, `ApiError`, token store | `shared/lib/api` |
| Hooks tied to a capability's data or rules | `features/<name>/hooks` |
| Generic hooks (`useDebouncedValue`, `useMediaQuery`) | `shared/hooks` |
| Business rules and types for a capability | `features/<name>/domain` |
| Providers, router, global error boundary | `app/` |
| A screen that composes several features | `routes/` (or `pages/`) |
| Env parsing, logger, formatting helpers | `shared/lib` |

The question isn't "what *kind* of file is this?" but **"who owns it?"** If one feature owns it, it lives there. If nothing in particular owns it (it's generic), it's `shared`.

## The public API: `index.ts`

Each feature **exposes only what others may use** through its `index.ts`, and keeps everything else private:

```ts
// features/projects/index.ts
export { ProjectTable } from "./components/ProjectTable"
export { NewProjectDialog } from "./components/NewProjectDialog"
export { useProjects } from "./hooks/useProjects"
export type { Project } from "./domain/project.types"
// NOT exported: api functions, internal hooks, helpers, query keys
```

```tsx
// routes/ProjectsPage.tsx
import { ProjectTable, NewProjectDialog } from "@/features/projects"      // ✓ through the public API
import { projectKeys } from "@/features/projects/api/projects.queries"    // ✗ reaches into internals
```

Why bother:

- **You can refactor a feature's internals freely.** As long as the public API holds, nothing outside breaks.
- It makes **dependencies visible**: the exports are exactly what other code relies on.
- It keeps features **decoupled**, so they can be owned by different people or teams and, if ever needed, lazy-loaded or extracted.

## The dependency rules

The folder structure only helps if imports respect it. The rules:

```text
            app  ─────────────► features, shared
         routes  ─────────────► features, shared
       features  ─────────────► shared            (and ONLY public APIs of other features, sparingly)
         shared  ─────────────► (nothing above it)
```

1. **`shared` never imports from `features` or `app`.** It must stay generic and reusable.
2. **Features never import from `app` or `routes`.**
3. **A feature doesn't import another feature's internals.** Anything it needs from another feature comes through that feature's `index.ts`, and better still, is composed at a higher level (below).
4. **No circular dependencies** between features. If A needs B and B needs A, they're one feature, or the shared piece belongs in `shared`.

### Enforce it, don't just document it

Rules that aren't checked decay within months. ESLint can enforce import restrictions:

```js
// eslint.config.js (flat config; syntax abbreviated)
export default [
  {
    files: ["src/features/**/*.{ts,tsx}"],
    rules: {
      "no-restricted-imports": ["error", {
        patterns: [
          { group: ["@/features/*/*"], message: "Import another feature through its public API (@/features/<name>), not its internals." },
          { group: ["@/app/*", "@/routes/*"], message: "Features must not depend on the app shell or routes." },
        ],
      }],
    },
  },
  {
    files: ["src/shared/**/*.{ts,tsx}"],
    rules: {
      "no-restricted-imports": ["error", {
        patterns: [{ group: ["@/features/*", "@/app/*", "@/routes/*"], message: "shared must stay feature-agnostic." }],
      }],
    },
  },
]
```

Notes:

- Inside a feature, use **relative imports** for its own files (`../hooks/useProjects`), so the alias pattern above only fires on cross-feature imports.
- Dedicated tools (for example `eslint-plugin-boundaries`, `dependency-cruiser`, or monorepo constraint features) give richer rules and dependency graphs. Check their current docs for configuration.
- Run it in CI ([CI/CD](../19-production/04-ci-cd.md)), so violations block merges.

## Cross-feature interaction

Features inevitably need each other: a project page shows the project's billing plan. Prefer these, roughly in this order:

1. **Compose at a higher level.** The page in `routes/` imports from both features and wires them together. Neither feature knows about the other.

```tsx
// routes/ProjectDetailPage.tsx
import { ProjectHeader } from "@/features/projects"
import { PlanBadge } from "@/features/billing"

export function ProjectDetailPage() {
  const { projectId } = useParams()
  return (
    <>
      <ProjectHeader id={projectId!} actions={<PlanBadge projectId={projectId!} />} />   {/* composition via slots/children */}
    </>
  )
}
```

2. **Pass data and callbacks via props or slots** ([composition patterns](../05-component-design/01-composition-patterns.md)).
3. **Share through the URL** (the selected project ID, filters) ([URL state](../10-routing/06-search-filter-and-url-state.md)).
4. **Depend on another feature's public API** for a genuinely shared concept (the `auth` feature's `useCurrentUser`). Keep these dependencies few, one-directional, and deliberate.
5. **Share server data through the query cache**: both features read `["users", id]` via a shared query definition, no direct imports of each other's state.
6. If two features keep needing each other's insides, **they're really one feature**, or the common part belongs in `shared`.

## When does code move to `shared`?

The temptation is to put everything reusable in `shared` "just in case". That makes `shared` a dumping ground that every feature depends on, which is the old type-based structure again.

A practical rule: **keep code in the feature that owns it until a second feature genuinely needs it** (and ideally a third; the "rule of three"). Then promote it, and make it generic: no feature-specific names, types, or assumptions.

Signs something *shouldn't* be in `shared`:

- Its name or props mention a feature concept (`ProjectStatusBadge`).
- It imports feature types or calls a feature's API.
- Only one feature uses it.

Duplicating a few lines between two features is often cheaper than inventing a premature abstraction that both bend around ([component API design](../05-component-design/00-component-api-design.md)).

## Colocation

Keep things **next to what uses them**:

- Tests beside the code (`ProjectTable.test.tsx`).
- Styles, stories, and types beside the component.
- A hook used by one component can live in the same file or folder until it's reused.
- Small features don't need all the subfolders. A feature with three files is fine as a flat folder. Add `api/`, `components/`, and `hooks/` when the folder gets crowded.

The structure is a **guideline for growth**, not a template to stamp on every feature on day one.

## Routing and code splitting

Features map naturally onto **lazy-loaded route chunks**, giving you [code splitting](../14-performance/03-code-splitting-and-lazy-loading.md) almost for free:

```tsx
// app/router.tsx
export const routes = [
  { path: "/", element: <AppLayout />, children: [
    { path: "projects/*", lazy: () => import("@/routes/projects") },
    { path: "billing/*", lazy: () => import("@/routes/billing") },
  ]},
]
```

Each feature's route module becomes its own chunk. A well-bounded feature (explicit public API, no tangled imports) splits cleanly; a feature wired into everything pulls the world into one chunk. Check the [bundle analyzer](../14-performance/05-bundle-optimization.md#analyze-first) to see whether your boundaries hold at build time.

## Barrel files: use with care

`index.ts` files that re-export things are the mechanism for a public API, but they have costs:

- A **large barrel** can make bundlers and dev tooling load more modules than needed, since importing one thing may evaluate the whole barrel (tree-shaking usually removes unused exports in production, but dev server and test startup can slow down).
- They can hide **circular imports**.
- **Don't nest barrels** (a barrel re-exporting barrels) and **don't barrel everything**. Export a small, intentional surface.

If a feature's barrel grows large, that's a signal the feature has too much exposed surface, or too many responsibilities.

## Alternatives and relatives

- **Feature-Sliced Design (FSD)** is a more formal methodology with named layers (`app`, `pages`, `widgets`, `features`, `entities`, `shared`) and strict import direction rules. It's the same ideas with more ceremony, which can help big teams and hurt small ones. Read its docs if you want a shared vocabulary.
- **Domain-driven design (DDD)** vocabulary (bounded contexts, ubiquitous language) often maps onto feature folders: each feature owns its own model of a concept.
- **Monorepos** take the same boundary idea further, making features or shared libraries separate packages ([04](./04-scaling-large-applications.md#monorepos-and-packages)).

## Migrating from a type-based structure

Don't stop the world. Move **incrementally**:

1. Create `features/`, `shared/`, and `app/`, and add the lint rules in *warning* mode.
2. Pick **one feature** and move its components, hooks, and API into `features/<name>`. Leave re-exports at the old paths if needed.
3. Extract genuinely generic pieces to `shared/` as you touch them.
4. Move on as you work on each area (the "boy scout" rule), switching lint rules to *errors* for finished features.
5. Delete the old folders when empty.

Use your editor's move-with-refactor support and path aliases to keep diffs mechanical, and ship in small PRs.

## Common mistakes

- **Organizing by file type forever**, and letting `components/` become a junkyard.
- **No public API**: features import each other's internals, so nothing can change safely.
- **Everything promoted to `shared` immediately**, creating a god folder every feature depends on.
- **Features depending on each other in a cycle** (A imports B, B imports A).
- **`shared` importing from `features`**, which inverts the whole structure.
- **Rules documented but not enforced**, so they erode.
- **Over-structuring tiny features** with empty `api/`, `hooks/`, and `domain/` folders.
- **Giant barrel files** that re-export everything (slow tooling, hidden cycles, accidental coupling).
- **Cross-feature coupling through shared mutable state** (a global store everything writes to) instead of explicit composition.
- **A feature defined too broadly** ("dashboard" containing ten unrelated capabilities). Split by user-facing capability.
- **Big-bang restructures** that freeze development for weeks.

## Quick summary

- Group code **by feature** (what it's for), not by type, so changes stay local and ownership is clear.
- Shape: **`app/`** (shell), **`features/`** (capabilities), **`shared/`** (generic building blocks), **`routes/`** (pages composing features).
- Each feature exposes a small **public API** (`index.ts`); everything else is private.
- Dependencies flow **one way**: `app/routes → features → shared`; features don't reach into each other's internals; no cycles.
- **Enforce** the rules with lint and CI, and prefer **composition at the page level** over cross-feature imports.
- Promote code to `shared` only when truly shared and generic (rule of three); colocate tests and helpers.
- Feature boundaries double as **code-splitting** boundaries.
- Migrate incrementally, and add structure only when you feel the pain.

## Next

[01 — Layered architecture](./01-layered-architecture.md)