# 20 — Frontend Architecture & Scalable Codebases

Architecture is what makes a codebase easy to understand, easy to change, and resilient to organizational scale. This module covers structural patterns and software engineering principles for large-scale enterprise React applications: feature-based folder organization, layered clean architecture, centralized API design, global error handling strategies, monorepo management with Turborepo, and modernizing legacy codebases from Create React App to Vite 6.

---

## What you'll be able to do after this module

- **Organize by Feature Domains**: Break monolithic folder structures into autonomous, co-located feature modules (`features/billing`, `features/auth`) with strict public APIs.
- **Implement Layered Architecture**: Decouple presentation components (UI) from domain business logic (services/state) and data infrastructure (HTTP clients).
- **Architect Scalable API Layers**: Centralize query endpoints, request adapters, response types, and mock services into a maintainable client layer.
- **Engineer Error Resilience**: Design multi-tier error boundaries, error classification models, and automated user recovery funnels.
- **Scale Across Monorepos**: Configure Turborepo and workspace package boundaries to share design systems, utilities, and types across multiple apps.
- **Modernize Legacy Codebases**: Execute seamless, step-by-step migrations from deprecated Create React App (CRA) setups to modern Vite 6 + React 19.

---

## Module Syllabus

| # | Note | Description | Key Architectural Pattern |
|---|------|-------------|:---:|
| 00 | [00-feature-based-architecture.md](./00-feature-based-architecture.md) | Feature-based colocation, domain isolation, public API barrels | Feature-Sliced Design |
| 01 | [01-layered-architecture.md](./01-layered-architecture.md) | Clean architecture: Presentation vs Domain vs Infrastructure layers | Layered Separation |
| 02 | [02-api-architecture.md](./02-api-architecture.md) | Centralized client layer, contract types, error mapping, mocks | API Layering |
| 03 | [03-error-handling-architecture.md](./03-error-handling-architecture.md) | Multi-tier error boundaries, recovery strategies, error logging | Error Resilience |
| 04 | [04-scaling-large-applications.md](./04-scaling-large-applications.md) | Monorepos, Turborepo pipelines, code sharing across teams | Enterprise Scaling |
| 05 | [05-cra-to-vite-migration.md](./05-cra-to-vite-migration.md) | Complete step-by-step CRA to Vite 6 + React 19 migration guide | Tooling Modernization |

---

## Architecture Evolution Matrix

| Paradigm / Milestone | Folder Organization | Build Tooling | State & Data Fetching |
| :--- | :--- | :--- | :--- |
| **Legacy (2018-2021)** | Technical folders (`components/`, `reducers/`) | Create React App / Webpack | Redux monolith + `useEffect` |
| **Intermediate (2022-2023)**| Domain features (`features/*`) | Webpack 5 / Vite 3 | Zustand / Redux Toolkit + RTK Query |
| **Modern Enterprise (2024+)** | Feature-Sliced / Monorepo (Turborepo) | Vite 6 / Rspack | TanStack Query v5 + React Compiler |

---

## Boundary with Other Modules

- **Component Design Patterns**: Read [`05-component-design/`](../05-component-design/README.md) for headless UI and compound component patterns.
- **Server State Integration**: Read [`12-server-state/`](../12-server-state/README.md) for TanStack Query caching architecture.
- **Production & Security**: Read [`19-production/`](../19-production/README.md) for CI/CD pipelines, Dockerization, and CSP hardening.

---
