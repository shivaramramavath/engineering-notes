# Module 00: Setup and Modern Tooling

## Syllabus & Topic Overview

Module 00 covers the essential foundation for building high-performance modern React applications. Moving decisively away from legacy architectures (Create React App, Webpack-first setups, and manual Babel configs), this module establishes modern production environments using React 19, Vite 6, TypeScript 5.7 strict mode, monorepo architectures, and StrictMode debugging behaviors.

### Concept Notes Index

1. [00-javascript-for-react.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/00-setup/00-javascript-for-react.md) - Essential modern ESNext features for React: destructuring, rest/spread, closures, async/await, optional chaining, and nullish coalescing.
2. [01-node-and-npm.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/00-setup/01-node-and-npm.md) - Node.js LTS runtime, npm/pnpm package resolution, lockfiles, semantic versioning, and scripts.
3. [02-vite.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/00-setup/02-vite.md) - Deep dive into Vite 6: native ESM dev server, esbuild pre-bundling, Rollup production builds, and plugins.
4. [03-project-structure.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/00-setup/03-project-structure.md) - Feature-sliced design, component folder anatomy, path aliases (`@/`), and asset organization.
5. [04-react-devtools.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/00-setup/04-react-devtools.md) - Mastering the React DevTools: inspecting Fiber trees, props/state live mutation, and flamegraph profiling.
6. [05-typescript-and-linting-setup.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/00-setup/05-typescript-and-linting-setup.md) - Strict TypeScript compiler options, ESLint flat config with `eslint-plugin-react-hooks` and React Compiler rules.
7. [06-react-versions-and-upgrade-guide.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/00-setup/06-react-versions-and-upgrade-guide.md) - Complete upgrade roadmap from React 17 -> 18 -> 19: Actions, `useActionState`, `ref` as prop, and removals.
8. [07-bundlers-compared-vite-rspack-turbopack.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/00-setup/07-bundlers-compared-vite-rspack-turbopack.md) - Architectural comparison of Vite 6, Rspack, Turbopack, and legacy Webpack.
9. [08-strict-mode-and-dev-behaviours.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/00-setup/08-strict-mode-and-dev-behaviours.md) - StrictMode double invocation, effect unmount/remount simulation, component purity, and memory leak detection.
10. [09-monorepo-setup-for-react.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/00-setup/09-monorepo-setup-for-react.md) - pnpm Workspaces, Turborepo 2 pipelines, shared UI component libraries, and single React instance invariants.

---

## Learning Outcomes & Core Takeaways

- **Vite 6 Mastery**: Configure ultra-fast unbundled ESM development workflows and optimize production Rollup chunk splitting.
- **React 19 Readiness**: Understand all breaking changes in React 19 (removal of `ReactDOM.render`, `forwardRef`, `defaultProps`) and transition to native Actions and `ref` props.
- **StrictMode Fluency**: Emphasize component purity and ensure all `useEffect` hooks contain robust cleanup logic to thrive under double-invocation checks.
- **Enterprise Monorepos**: Set up pnpm workspaces and Turborepo to share design systems without dual-React hook collision bugs.
