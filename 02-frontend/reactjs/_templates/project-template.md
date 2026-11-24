# Project Template — ReactJS Curriculum

> **Source of Truth** for all production projects in `02-frontend/reactjs/22-projects/`. Every project folder must contain a `README.md` conforming to this structure, plus corresponding modular `.md` files (`01-design.md`, `02-source-code.md`, `03-tests.md`, `04-build-and-run.md`).

```markdown
# Project [XX]: [Project Title] (`[project-slug]`)

> [Concise summary of project scope, core frontend system design principles implemented, and real-world capabilities demonstrated.]

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-[xx]-[slug]` |
| **React Version** | React 19.0.0 |
| **Tested With** | Vite 6.0, TypeScript 5.7, Vitest 3.0, Playwright 1.49 |
| **Bundler** | Vite 6 (ESM) |
| **State Architecture** | [TanStack Query v5 / Zustand v5 / React 19 Actions] |
| **Styling** | [Tailwind CSS v4 / Radix UI / shadcn/ui] |
| **Complexity Level** | [Intermediate / Advanced / Production-Grade] |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 System Architecture & Data Flow
[Detailed architectural rationale: separation of concerns, container vs presentational patterns, hooks-as-services, API client inversion.]

```
+-------------------------------------------------------------+
|                      React Application Flow                 |
|                                                             |
|  [ User Actions / DOM Events ]                              |
|               |                                             |
|               v                                             |
|  [ Feature UI Components / Radix Primitives ]               |
|               |                                             |
|               v                                             |
|  [ Custom Hooks / Feature Services / TanStack Query ]       |
|               |                                             |
|               v                                             |
|  [ Typed API Client / Axios or Fetch Layer ]                |
|               |                                             |
|               v                                             |
|  [ Backend API / MSW Mock Server ]                          |
+-------------------------------------------------------------+
```

### 1.2 Directory Layout
```text
src/
├── app/                  # App shell, providers, router configuration
├── assets/               # Static assets, fonts, icons
├── components/           # Shared generic UI components (buttons, dialogs, inputs)
├── features/             # Feature-sliced modules (e.g. auth, billing, feed)
│   └── [feature]/
│       ├── components/   # Feature-specific UI
│       ├── hooks/        # Feature hooks (business logic / data)
│       ├── services/     # API integration & data transformations
│       ├── types/        # TypeScript interfaces & Zod schemas
│       └── tests/        # Unit & integration tests
├── hooks/                # Global generic hooks (useDebounce, useMediaQuery)
├── lib/                  # Library configurations (queryClient, axios, utils)
└── main.tsx              # Application entry point
```

---

## 2. Complete Project Source Code

### Configuration Files
- `package.json`
- `vite.config.ts`
- `tsconfig.json`
- `src/index.css` (Tailwind v4)

### Core Components & Feature Code
- Domain Types & Zod Schemas (`src/features/.../types.ts`)
- API Client & Hooks Service (`src/features/.../hooks/use[Feature].ts`)
- UI Presentation Components (`src/features/.../components/...`)
- Application Shell & Main Router (`src/App.tsx`, `src/main.tsx`)

---

## 3. Testing & Verification Suite

- **Unit & Hook Tests**: Vitest + `@testing-library/react` tests verifying render states, user interactions, and error handling.
- **Mock Handlers**: Mock Service Worker (MSW) network interception handlers.
- **E2E / Browser Smoke Test**: Playwright test script testing critical user flows.

---

## 4. Build, Run, and Benchmark Output

### 4.1 Running the Project
```bash
npm install
npm run dev
npm run test
npm run build
```

### 4.2 Real Verification Log
```text
✓ tsc --noEmit (0 errors)
✓ vitest run (X tests passed)
✓ vite build (real bundle metrics)
```
```
