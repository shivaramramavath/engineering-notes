# Module 00: Setup & Development Workflow (`00-setup`)

> Development environment configuration, project scaffolding, modern toolchain integration, Turbopack, Biome vs ESLint 9, and Next.js upgrade guides.

---

## 1. Module Overview & Learning Objectives

This module establishes a production-grade development workflow for modern Next.js applications (Next.js 15.2+ / 16.0 with React 19). You will learn how to initialize robust TypeScript projects, configure high-performance bundlers (Turbopack), structure linting with ESLint 9 Flat Config and Biome, and safely migrate existing applications across major versions.

---

## 2. Directory Contents

| Note | Focus Area | Complexity | Key Concepts Covered |
| :--- | :--- | :---: | :--- |
| **[00-create-next-app.md](file:///E:/Learning/engineering-notes/02-frontend/nextjs/00-setup/00-create-next-app.md)** | Scaffolding | Beginner | `create-next-app` flags, template selection, package managers |
| **[01-typescript.md](file:///E:/Learning/engineering-notes/02-frontend/nextjs/00-setup/01-typescript.md)** | Type Safety | Beginner | `tsconfig.json` paths, strict mode, Next.js type generation |
| **[02-linting-and-formatting.md](file:///E:/Learning/engineering-notes/02-frontend/nextjs/00-setup/02-linting-and-formatting.md)** | Code Quality | Intermediate | ESLint 9 Flat Config (`eslint.config.mjs`), Biome integration, removal of `next lint` |
| **[03-environment-variables.md](file:///E:/Learning/engineering-notes/02-frontend/nextjs/00-setup/03-environment-variables.md)** | Configuration | Intermediate | `.env.local`, `NEXT_PUBLIC_` boundaries, runtime vs build envs |
| **[04-next-config.md](file:///E:/Learning/engineering-notes/02-frontend/nextjs/00-setup/04-next-config.md)** | Build Configuration | Intermediate | `next.config.ts`, rewrites, headers, image domains, standalone output |
| **[05-development-workflow.md](file:///E:/Learning/engineering-notes/02-frontend/nextjs/00-setup/05-development-workflow.md)** | Dev Productivity | Beginner | Dev scripts, debugging flags, VS Code launch configurations |
| **[06-nextjs-versions-and-upgrade-guide.md](file:///E:/Learning/engineering-notes/02-frontend/nextjs/00-setup/06-nextjs-versions-and-upgrade-guide.md)** | Upgrades & Codemods | Advanced | Next.js 13/14 to 15/16 migration, async request APIs, codemods |
| **[07-turbopack-and-bundlers.md](file:///E:/Learning/engineering-notes/02-frontend/nextjs/00-setup/07-turbopack-and-bundlers.md)** | Bundler Architecture | Advanced | Turbopack Rust engine, HMR performance, Webpack fallback, React Compiler |

---

## 3. Recommended Learning Path
1. Begin with **00-create-next-app** and **01-typescript** to set up a clean project.
2. Review **02-linting-and-formatting** to transition away from legacy `next lint` to modern ESLint 9 or Biome.
3. Study **06-nextjs-versions-and-upgrade-guide** and **07-turbopack-and-bundlers** for modern Next.js 15/16 breaking changes and compiler optimizations.
4. Next module: [Module 01: Fundamentals](file:///E:/Learning/engineering-notes/02-frontend/nextjs/01-fundamentals/README.md).
