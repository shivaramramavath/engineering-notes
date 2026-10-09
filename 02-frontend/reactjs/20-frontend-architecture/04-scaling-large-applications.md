# Scaling Large Applications

An app that three people built in six months behaves nothing like one that **forty people** have changed for five years. Scaling a frontend isn't mainly about performance (that's [its own folder](../14-performance/README.md)). It's about **keeping change safe and fast as the codebase, the team, and the organization grow**.

The failure modes of scale are familiar:

- Every change risks breaking something far away.
- The build and CI take 25 minutes.
- Merge conflicts and "who owns this?" arguments are constant.
- Nobody dares upgrade React, the router, or a core library.
- New hires take months to become productive.
- Three different ways of doing the same thing coexist.

> **Don't scale before you need to.** Most apps never need micro-frontends, a monorepo with eight packages, or a custom build system. Add structure in response to a **specific, felt problem**, not because large companies do it.

## Dimensions of scale

| Dimension | Pain you feel | Direction |
|---|---|---|
| **Codebase size** | Slow IDE/typecheck/tests, tangled dependencies | Boundaries, packages, incremental tooling |
| **Team size** | Merge conflicts, inconsistent code, review bottlenecks | Ownership, conventions, automation |
| **Number of teams** | Coordination overhead, blocked releases | Clear interfaces, independent deploys (sometimes) |
| **Product complexity** | Features interacting in surprising ways | Domain-oriented structure, contracts |
| **Runtime size** | Heavy bundles, slow pages | Splitting, budgets ([performance](../14-performance/README.md)) |
| **Time** | Upgrades stall, debt accumulates | Migration discipline, regular maintenance |

## Step 1: strengthen boundaries (the biggest lever)

Everything starts with the [feature structure](./00-feature-based-architecture.md) and its dependency rules, **enforced by tooling**:

- Lint rules (or dedicated tools) that forbid cross-feature internals, upward imports, and cycles ([enforcement](./00-feature-based-architecture.md#enforce-it-dont-just-document-it)).
- **Circular dependency detection** in CI (for example with `madge` or `dependency-cruiser`).
- A visible **dependency graph** you can inspect to see what's actually coupled.
- **TypeScript strictness** (`strict`, no implicit `any`) so interfaces between modules mean something.

If boundaries hold, you can scale almost any direction afterwards (packages, teams, even separate deploys), because the seams already exist. If they don't, every scaling step amplifies the tangle.

## Ownership

Code with no owner rots. Make ownership explicit:

```text
# .github/CODEOWNERS
/src/features/billing/      @acme/payments-team
/src/features/projects/     @acme/core-team
/src/shared/ui/             @acme/design-system-team
/src/shared/lib/api/        @acme/platform-team
/.github/workflows/         @acme/platform-team
```

- **CODEOWNERS** auto-requests reviews from the people who know the area, and (with branch protection) can require their approval for sensitive paths ([branch protection](../19-production/04-ci-cd.md#branch-protection-and-required-checks)).
- Give **shared code** (design system, API client, build config) a named owning team with a way to request changes.
- Ownership isn't gatekeeping. Pair it with **documented contribution paths** and fast response expectations so shared code doesn't become a bottleneck.

## Conventions and automation

At scale, consistency can't depend on everyone remembering a wiki page.

- **Automate formatting and linting** (Prettier, ESLint), run in editors, pre-commit hooks, and CI. Debates about style end at the config file.
- **Generators and templates** create new features with the right structure: a scaffolding script (Plop, Hygen, or a simple Node script) that makes `features/<name>/` with the standard folders, a sample test, and the `index.ts`.
- **Lint rules for your own architecture** (restricted imports, naming, required patterns) turn conventions into feedback within seconds.
- **Pull request templates and review checklists** (accessibility, tests, error states, performance impact).
- **Codemods** (jscodeshift, ts-morph) for large mechanical changes (renaming an API, migrating a component) so you change 400 files reliably instead of by hand.
- **Written decisions** ([ADRs](#architecture-decision-records)) so the *why* survives people leaving.

## Monorepos and packages

A **monorepo** keeps multiple apps and shared packages in one repository, with a workspace tool to manage them:

```text
repo/
├── apps/
│   ├── web/                  # main customer app
│   ├── admin/                # internal admin app
│   └── marketing/            # public site
├── packages/
│   ├── ui/                   # design system components
│   ├── api-client/           # typed client + generated types
│   ├── config-eslint/        # shared lint config
│   ├── config-typescript/    # shared tsconfig
│   └── utils/
├── package.json              # workspaces
└── pnpm-workspace.yaml / turbo.json / nx.json
```

Tools: package-manager **workspaces** (pnpm, npm, Yarn), plus a **task runner/build orchestrator** such as Turborepo or Nx for caching and dependency-aware task execution. Their configuration formats change, so follow current docs.

### What you gain

- **Shared code as real packages** with explicit public APIs and version-free local linking: one `ui` package consumed by three apps.
- **Atomic changes**: update the design system and every consumer in one PR.
- **Consistent tooling and dependency versions** across apps.
- **Affected-only CI**: build and test only the packages impacted by a change, using the dependency graph and **remote build caching**, so CI time stays flat as the repo grows.
- **Enforced boundaries**: a package can only import what it declares as a dependency, which is much stronger than a lint rule.

### What it costs

- **Tooling complexity** (workspace config, task pipelines, caching, TypeScript project references).
- **Repository-wide effects**: a broken main blocks everyone, the repo gets large, and permissions are repo-wide.
- Requires **discipline about package boundaries** (don't create ten tiny packages that always change together).

**When to adopt:** you have **multiple apps** sharing code, or a shared library (design system) with several consumers, or CI time is hurting. A single app can scale a long way as a single package with strong internal boundaries.

## Build, CI, and developer speed

Developer speed is a scaling concern: slow feedback loops tax every change.

- **TypeScript**: use `isolatedModules`, `skipLibCheck`, **project references** in monorepos, and incremental builds. Run the type check in CI separately from the build ([build](../19-production/01-build.md#type-checking-is-separate)).
- **Tests**: run in parallel, **shard across machines**, and run only affected tests on PRs where tooling supports it ([speed](../19-production/04-ci-cd.md#speed)).
- **CI caching**: dependencies, build outputs, and test results ([CI/CD](../19-production/04-ci-cd.md)).
- **Dev server**: Vite's on-demand model scales well, but huge dependency graphs or barrel-file-heavy imports can slow cold start. Watch for it ([barrel files](./00-feature-based-architecture.md#barrel-files-use-with-care)).
- **Measure it**: track CI duration, flaky-test rate, and time-to-first-PR for new hires. These are your developer experience metrics.
- **Fix flaky tests aggressively**: at scale, a 2% flake rate across hundreds of tests means CI fails constantly.

## Performance at scale

- **Per-route code splitting** keyed to feature boundaries ([code splitting](../14-performance/03-code-splitting-and-lazy-loading.md)).
- **Performance budgets enforced in CI** (bundle size, Lighthouse) so no team can silently regress shared load times ([budgets](../14-performance/00-profiling-and-measuring.md#performance-budgets-and-regression)).
- **Real-user monitoring segmented by route and release** so each team sees its own numbers ([RUM](../19-production/07-performance-monitoring.md)).
- **Dependency discipline**: every new library has a cost for the whole app. Review additions, and prefer one library per job (one date library, one form library).

## Shared design system

With many teams, UI consistency comes from a shared, owned **design system** ([design system](../09-ui-components/09-design-system.md)), whether that's a folder in a single app or an internal package:

- **Tokens and primitives** versioned and documented, with **accessibility built in** (so fixing a focus bug fixes it everywhere).
- A **visible catalog** (Storybook or a docs route) so people reuse instead of rebuilding.
- **A contribution path**, so teams can propose components without waiting indefinitely.
- **Deprecation policy**: mark, migrate with codemods, remove.

## State and data at scale

- **Keep server state in the query cache** and avoid giant global stores. The less global mutable state, the less accidental coupling ([state management](../13-state-management/00-choosing-state-management.md)).
- **Feature-owned state**: each feature owns its stores and providers, exposed only through its public API.
- **Typed contracts with the backend** (generated clients, schemas) and a contract-change process across teams ([API architecture](./02-api-architecture.md#contracts-and-change-management)).
- **Feature flags** to decouple deploy from release, enabling **trunk-based development**: small, frequent merges to `main` behind flags rather than long-lived branches that diverge and conflict.

## Micro-frontends

**Micro-frontends** split the frontend into **independently built and deployed** pieces, owned by different teams, composed in the browser (or at the edge). Techniques include **Module Federation**, iframes, web components, route-level composition at the CDN/server, and framework-specific single-spa-style orchestration.

**The real reason to use them is organizational, not technical:** many autonomous teams that must **deploy independently** without coordinating releases.

What you get:

- Independent deploy cadence and ownership per team.
- Incremental migration (new tech in one slice while the legacy app continues).
- Fault and blast-radius isolation.

What it costs, and it's a lot:

- **Consistency**: design, navigation, and UX drift unless a shared design system and shell are strictly maintained.
- **Duplicate dependencies** and larger bundles (React loaded twice, or complex sharing configuration with version-compatibility constraints).
- **Cross-app communication and shared state** get awkward (events, URL, shared storage).
- **Performance and complexity**: more network hops, runtime composition, and harder end-to-end testing and debugging.
- **Operational overhead**: multiple pipelines, versions, and integration points to monitor.

**Default answer: don't.** A well-structured modular monolith (feature boundaries, monorepo packages, CODEOWNERS, affected-only CI) delivers most of the benefits with a fraction of the cost. Consider micro-frontends only when team autonomy and independent releases are genuinely blocked by a single deployable, and you have the platform capacity to support the extra machinery.

A common middle ground: **separate apps for separate audiences** (customer app, admin, marketing) in a monorepo, sharing a design-system package, each deployed independently. That's multiple applications, not micro-frontends within one.

## Migrations and evolving the codebase

At scale, change is mostly **migration**: upgrading React, swapping the router, replacing a state library, moving to TypeScript, adopting a design system. Do it safely:

- **Strangler fig**: build the new way next to the old, route traffic or usages gradually, delete the old when nothing uses it. Never "stop the world and rewrite".
- **Incremental adoption with guard rails**: new code must use the new pattern (lint rule on the old one in *warn* mode for existing files, *error* for new ones).
- **Codemods** for mechanical transformations, with a PR per area.
- **Compatibility shims** (re-exports at old import paths) to move code without breaking consumers immediately.
- **Track progress visibly**: a script that counts remaining legacy usages, plotted over time or posted in a channel. What's measured gets finished.
- **Timebox and own it**: migrations with no owner and no deadline become permanent half-states, the worst of both worlds.
- **Keep dependencies current** in small increments (automated update PRs, regular upgrade days) rather than every two years in a painful jump ([dependency updates](../19-production/04-ci-cd.md#dependency-updates)).
- **Rewrite rarely.** Prefer targeted refactors behind tests. Full rewrites routinely take longer than planned and re-introduce bugs the old code had already fixed.

## Managing technical debt

- **Make debt visible**: keep a list (issues labeled `tech-debt`) with impact and rough cost.
- **Pay it as you go**: leave touched code better (small refactors in the same PR as feature work, where safe).
- **Reserve capacity** (a fixed share of each sprint) for maintenance, or debt only grows.
- **Distinguish deliberate from accidental debt.** Deliberate debt (a shortcut taken knowingly to hit a deadline) should have a ticket and a plan.
- **Use metrics** to find hotspots: files with the highest churn plus complexity plus bug frequency are where refactoring pays back most.

## Architecture Decision Records

An **ADR** is a short document capturing **a decision, its context, and its consequences**, kept in the repo (`docs/adr/0007-use-tanstack-query.md`). Months later, "why do we do it this way?" has an answer, and changing course is a conscious, documented choice instead of an accident.

```md
# 7. Use TanStack Query for server state

Status: Accepted (2026-03-14)

## Context
We were hand-rolling fetch + useEffect + global store caching. Bugs: stale data,
duplicate requests, inconsistent loading states. Four screens needed pagination.

## Decision
Use TanStack Query for all server state. Query keys via per-feature factories.
No server data in global stores.

## Consequences
+ Caching, dedup, retries, and invalidation handled consistently.
- New dependency; team must learn its caching model.
- Legacy screens migrate incrementally (tracked in #482).

## Alternatives considered
SWR (less mutation support), RTK Query (we don't use Redux), keep hand-rolled.
```

Keep them short, number them, never delete (supersede with a new ADR). Write one for decisions that are **costly to reverse** or that people keep re-debating.

## Onboarding and documentation

- A **README that gets a new developer running in under 30 minutes**: install, env setup ([`.env.example`](../19-production/00-environment-variables.md#env-files-and-precedence)), dev server, tests.
- **Architecture overview** (a diagram of folders and dependency rules; this folder is a template), plus the ADR log.
- **Living examples**: a well-built reference feature people copy from, and the scaffolding generator.
- **Docs near the code** (feature READMEs), not a wiki that decays.
- Track **time-to-first-merged-PR** for new hires: a direct measure of how approachable your architecture is.

## A pragmatic growth path

```text
Stage 1: one app, a few folders                    → just keep components small, no fetch in UI
Stage 2: features/, shared/, lint boundaries      → 00, 01
Stage 3: layered features, API conventions, errors → 01, 02, 03
Stage 4: CODEOWNERS, generators, CI affected-only → ownership and speed
Stage 5: monorepo with apps + packages             → multiple apps sharing code
Stage 6: independent deploys / micro-frontends     → only when org structure demands it
```

Move to the next stage when a **specific pain** appears, not when the calendar says so. Each stage should make something concrete better, and you should be able to say what.

## Common mistakes

- **Scaling prematurely**: micro-frontends or a ten-package monorepo for a team of five.
- **No enforced boundaries**, so structure exists only on paper and decays.
- **Shared code without an owner**, or an owner who's a bottleneck with no contribution path.
- **A shared `utils`/`common` dumping ground** that everything depends on.
- **Micro-frontends adopted for technical reasons** (to "decouple the code") when the real fix is module boundaries.
- **Big-bang rewrites** instead of incremental, measured migrations.
- **Half-finished migrations** with no owner, so two ways of doing everything live on forever.
- **Ignoring CI and dev-loop speed** until nobody runs the tests.
- **Tolerating flaky tests** at scale.
- **Style and architecture rules enforced by review comments** instead of tooling.
- **Undocumented decisions**, so the same debates recur and context is lost.
- **Never upgrading dependencies**, then facing a multi-month leap.
- **Duplicated design-system components** because the shared one is hard to find or change.
- **Treating architecture as a one-time design** instead of something revisited as the product and team change.

## Quick summary

- Scaling a frontend means keeping **change safe and fast** as code, team, and org grow. Start by strengthening **enforced boundaries**; most other scaling steps depend on them.
- Give code **owners** (CODEOWNERS), automate **conventions** (lint rules, generators, codemods, templates), and write down decisions as **ADRs**.
- A **monorepo** (workspaces + a task runner with caching and affected-only CI) pays off with multiple apps or a shared package consumed by many; it adds tooling complexity.
- Protect **developer speed** (typecheck, tests, CI caching, flake control) and **runtime performance** (per-route splitting, budgets, RUM per route).
- Keep UI consistent with an owned **design system**, and keep state **feature-owned** with typed backend contracts and feature flags for trunk-based flow.
- **Micro-frontends** solve an *organizational* problem (independent deploys by many teams) at high technical cost. The default answer is a modular monolith.
- Evolve via **incremental migrations** (strangler fig, codemods, progress tracking, ownership), manage debt visibly, and grow structure only in response to real pain.

## Next

Continue to [21 — Specializations](../21-specializations/README.md).