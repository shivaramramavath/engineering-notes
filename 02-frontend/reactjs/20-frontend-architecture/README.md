# 20 — Frontend Architecture

**Architecture** is the set of decisions that are **expensive to change later**: how code is organized, which parts may depend on which, where data and errors flow, and what conventions a team shares. Individual components are easy to rewrite. A tangled dependency graph, a missing boundary, or a data layer scattered across 200 files is not.

Good architecture isn't about diagrams or patterns. It answers practical questions:

- **Where does this new code go?** (Everyone should agree without a meeting.)
- **What can I change without breaking something far away?**
- **How long until a new teammate can ship safely?**
- **Can two teams work in parallel without stepping on each other?**

```text
Small app        →  a few folders, almost no rules
Growing app      →  features, clear layers, enforced boundaries      ← this folder
Large org        →  ownership, packages, tooling, maybe separate deploys
```

## The core principles

1. **Organize by what the code is *for* (features), not what it *is* (components/hooks/utils).** Things that change together live together.
2. **Dependencies point one way.** Lower-level code never imports higher-level code, and features don't reach into each other's internals.
3. **Keep logic out of the UI.** Components render. Business rules, data mapping, and orchestration live in testable, UI-free code.
4. **Isolate the edges.** The API, third-party libraries, and the browser are *boundaries*. Wrap them once so the rest of the app doesn't depend on their shape.
5. **Make the right thing the easy thing.** Conventions, templates, and lint rules beat documentation nobody reads.
6. **Don't build for scale you don't have.** Architecture should grow with the app. Premature structure is its own debt.

## A reference shape

This is where the folder converges (each part is explained in the notes):

```text
src/
├── app/                  # app shell: providers, router, global styles, error boundary
├── features/             # one folder per product capability
│   ├── projects/
│   │   ├── api/          # data access for this feature
│   │   ├── components/   # UI owned by this feature
│   │   ├── hooks/        # feature-level hooks (queries, mutations, state)
│   │   ├── domain/       # pure business logic, types, mappers
│   │   └── index.ts      # the feature's PUBLIC API
│   └── billing/ …
├── shared/               # feature-agnostic building blocks
│   ├── ui/               # design-system primitives (shadcn/ui lives here)
│   ├── lib/              # api client, utils, env, logger
│   └── hooks/            # generic hooks (useDebounce, useMediaQuery)
└── routes/               # route tree composing features into pages
```

## Prerequisites

- [Component design](../05-component-design/README.md): composition and API design
- [State management](../13-state-management/README.md) and [server state](../12-server-state/README.md): where each kind of state lives
- [API integration](../11-api-integration/README.md): the client this folder organizes
- [Production](../19-production/README.md): the constraints architecture has to serve (deploys, monitoring, security)

## Contents

| # | File | What you'll learn |
|---|------|-------------------|
| 00 | [Feature-based architecture](./00-feature-based-architecture.md) | Folder structure, public APIs, dependency rules, where code goes |
| 01 | [Layered architecture](./01-layered-architecture.md) | UI / hooks / domain / data layers inside a feature, keeping logic out of components |
| 02 | [API architecture](./02-api-architecture.md) | Organizing client, endpoints, queries, DTOs, and contracts |
| 03 | [Error handling architecture](./03-error-handling-architecture.md) | One strategy from API failure to boundary to monitoring |
| 04 | [Scaling large applications](./04-scaling-large-applications.md) | Ownership, monorepos, tooling, micro-frontends, migrations |

## Suggested order

00 → 01 → 02 builds the structure. 03 cuts across all of it. 04 is for when team or codebase size forces new answers. If you're building a small app, read 00 and 01 and stop there.

## How to use this folder

Treat each note as a **menu, not a mandate**. For a 5-screen app, `features/` with three folders and one lint rule is plenty. Adopt more structure when you feel a specific pain: merge conflicts in shared folders, "where does this go?" debates, changes that ripple unexpectedly, slow onboarding.