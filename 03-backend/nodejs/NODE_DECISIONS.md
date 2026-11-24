# Node.js Architecture Decision Records (ADRs)

> Engineering and architectural decisions made during the audit, completion, and standardization of the Node.js track.

---

## ADR 001: Baseline Version Standardization (Node.js 22 LTS / Node.js 24)
- **Status**: Accepted
- **Context**: Node.js 18 reached End of Life in April 2025. Node.js 20 is in maintenance LTS (EOL April 2026). Node.js 22 is the active LTS line, and Node.js 24 is the current generation line. Modern language capabilities such as native TypeScript type stripping (`--experimental-strip-types` / native `node app.ts`), built-in SQLite (`node:sqlite`), `require(esm)` by default, the permission model (`--permission`), and built-in `--env-file` are standard in Node 22+.
- **Decision**: Standardize on **Node.js 22 LTS / Node.js 24** as the primary execution baseline across all notes and examples, tagging older syntax differences where applicable.

---

## ADR 002: Module Consolidation & Naming Conventions
- **Status**: Accepted
- **Context**: Several naming and structural inconsistencies existed:
  - `01-fundamentals/03-module-system.md` and `02-core-modules/11-module-system.md` duplicated identical content.
  - `05-http-web/06_status-codes.md` and `06-express/08_http-errors.md` had underscores and duplicate status code coverage.
  - `13-testing/` mixed two prefixes (`00_README.md`, `01-unit-and-integration-testing.md` vs `01_testing-fundamentals.md`).
  - `22-project-tooling/` had files with underscores (`01_project-structure.md`) and an unnumbered camelCase file (`InversifyJS.md`).
- **Decision**:
  - Resolve the module system duplication by keeping `01-fundamentals/03-esm-vs-commonjs-in-node.md` as the definitive module system guide, and repurposing `02-core-modules/11` into `02-core-modules/11-node-sqlite-and-test-runner.md`.
  - Consolidate `13-testing/` into standard `NN-kebab-case.md` notes without losing existing depth.
  - Standardize `22-project-tooling/` files to kebab-case: `01-project-structure.md` through `06-inversifyjs.md`.
  - Harmonize `05-http-web/` and `06-express/` to standard `NN-kebab-case.md`.

---

## ADR 003: Database Structure Preservation & PostgreSQL Expansion
- **Status**: Accepted
- **Context**: `07-databases/` contains deep nested modules for `mongodb` (92 files) and `redis` (113 files) with their own root files (`README`, `ROADMAP`, `CONTRIBUTING`, `LICENSE`). Meanwhile, `postgresql` had only two 0-byte placeholder files.
- **Decision**: Keep nested `mongodb` and `redis` tracks intact, populating their missing root files for consistency. Expand `07-databases/postgresql/` into a complete production guide covering `pg` connection pooling, parameterized queries, Drizzle & Prisma migrations, Kysely, transaction isolation, JSON/full-text search, and real database testing. Add SQLite (`node:sqlite`), MySQL, DynamoDB, and an overarching database selection guide.

---

## ADR 004: Fills Numbering Gap with Module 21 (Projects) and Modules 23/24
- **Status**: Accepted
- **Context**: The track skipped from `20-aws-sdk` to `22-project-tooling`, leaving a gap at `21-`. Furthermore, top-level enterprise reference projects, interview guides, and cheatsheets were absent.
- **Decision**:
  - Fill `21-projects/` with 16 comprehensive, production-grade reference projects (Express+Postgres REST API, Auth service, Redis URL shortener, BullMQ job queue, Realtime chat, File upload, etc.).
  - Add `23-interview/` with 11 deep-dive interview preparation guides covering event loop mechanics, streams, databases, and output prediction.
  - Add `24-cheatsheets/` with 14 high-density quick reference cards.
  - Build out all 3 project skeletons in `20-aws-sdk/05-projects/` (`01-s3-upload-api`, `02-serverless-api`, `03-node-app-on-ecs`) per project template with infrastructure, application code, tests, and Docker.

---

## ADR 005: Framework Landscape Expansion (06b, 06c, 06d)
- **Status**: Accepted
- **Context**: Modern Node.js server architectures frequently evaluate Fastify, Hono, and Koa alongside Express 5.
- **Decision**: Add sibling modules `06b-fastify/`, `06c-hono-and-edge-runtimes/`, and `06d-koa-and-framework-landscape/` providing complete production guides for schema validation, hooks, performance benchmarks, and edge interop.
