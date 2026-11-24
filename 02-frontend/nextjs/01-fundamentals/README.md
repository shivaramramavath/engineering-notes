# Module 01: Next.js Fundamentals (`01-fundamentals`)

> Architectural foundations of the Next.js framework, App Router design principles, build output anatomy, route tables, and Pages-to-App Router migration.

---

## 1. Module Overview & Learning Objectives

This module explores the core mechanics governing Next.js execution. You will understand how the App Router processes incoming HTTP requests, how React Server Components (RSC) serialize UI across the wire, how to decode production route tables (`○`, `ƒ`, `◐`), and how to migrate legacy Pages Router codebases to modern idiomatic architectures.

---

## 2. Directory Contents

| Note | Focus Area | Complexity | Key Concepts Covered |
| :--- | :--- | :---: | :--- |
| **[00-overview.md](file:///E:/Learning/engineering-notes/02-frontend/nextjs/01-fundamentals/00-overview.md)** | Core Philosophy | Beginner | Server-centric architecture, hybrid rendering, full-stack primitives |
| **[01-app-router.md](file:///E:/Learning/engineering-notes/02-frontend/nextjs/01-fundamentals/01-app-router.md)** | App Directory | Beginner | The `app/` folder, shared layouts, templates, error boundaries |
| **[02-project-structure.md](file:///E:/Learning/engineering-notes/02-frontend/nextjs/01-fundamentals/02-project-structure.md)** | Code Organization | Intermediate | Colocation, private folders `_components`, route groups `(marketing)` |
| **[03-pages-and-layouts.md](file:///E:/Learning/engineering-notes/02-frontend/nextjs/01-fundamentals/03-pages-and-layouts.md)** | Layout Hierarchy | Intermediate | Nested layout nesting, state persistence across navigation, template resets |
| **[04-pages-router.md](file:///E:/Learning/engineering-notes/02-frontend/nextjs/01-fundamentals/04-pages-router.md)** | Legacy Comparison | Intermediate | `pages/` directory, `getServerSideProps`, `getStaticProps`, legacy data fetching |
| **[05-build-output-anatomy-and-route-table.md](file:///E:/Learning/engineering-notes/02-frontend/nextjs/01-fundamentals/05-build-output-anatomy-and-route-table.md)** | Build Artifacts | Advanced | Route symbols (`○`, `ƒ`, `◐` PPR), `.next/standalone/`, chunk manifests |

---

## 3. Recommended Learning Path
1. Review **00-overview** and **01-app-router** to build a solid mental model of Server-First UI rendering.
2. Master **02-project-structure** and **03-pages-and-layouts** to design clean nested application shells.
3. Study **05-build-output-anatomy-and-route-table** to understand production compilation artifacts and Partial Prerendering.
4. Next module: [Module 02: Routing](file:///E:/Learning/engineering-notes/02-frontend/nextjs/02-routing/README.md).
