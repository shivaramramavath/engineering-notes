# Nest CLI Workspaces and Monorepos

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 01-getting-started
- **Prerequisites**: [03-cli-generators](./03-cli-generators.md), [04-project-structure](./04-project-structure.md), [00-prerequisites/05-nodejs-modules-esm-and-cjs](../00-prerequisites/05-nodejs-modules-esm-and-cjs.md)

---

## 1. Context & Purpose

In enterprise environments, backend engineering teams frequently manage multiple coordinated applications: API gateways, domain microservices, worker processes, and shared business libraries. Managing these within isolated repositories causes code duplication, version drift, and cumbersome local developer workflows.

The Nest CLI provides native monorepo workspace management. It organizes multiple applications (`apps/`) and reusable libraries (`libs/`) under a single root `nest-cli.json` and TypeScript configuration, offering unified build pipelines, shared dependencies, and atomic cross-service refactoring.

---

## 2. Theoretical Foundations

Monorepo architecture centralizes multiple projects within a single version-controlled repository:
- **Code Sharing**: Internal domain models, DTOs, utilities, and database schemas are published as internal libraries without publishing to an external npm registry.
- **Atomic Commits**: Breaking API changes and their client-service updates occur synchronously in a single Git commit.
- **Consistent Tooling**: Linting, formatting, compilation, testing, and continuous integration pipelines run under a single unified configuration.

Nest CLI monorepos operate on the concept of **Applications** (runnable entry points producing executables) and **Libraries** (non-runnable shared modules imported via TypeScript path aliases).

---

## 3. Architecture & Internal Mechanics

```
root/
├── nest-cli.json                   <-- Root CLI workspace descriptor
├── package.json                    <-- Unified dependencies
├── tsconfig.json                   <-- Root path aliases (@app/*, @lib/*)
├── tsconfig.build.json
├── apps/
│   ├── gateway/                    <-- HTTP API Gateway Application
│   │   ├── src/
│   │   │   ├── main.ts
│   │   │   └── gateway.module.ts
│   │   └── tsconfig.app.json
│   └── order-worker/               <-- Background Queue Worker Application
│       ├── src/
│       │   ├── main.ts
│       │   └── order-worker.module.ts
│       └── tsconfig.app.json
└── libs/
    ├── common/                     <-- Shared Library (Pipes, Filters, Guards)
    │   ├── src/
    │   │   ├── index.ts
    │   │   └── common.service.ts
    │   └── tsconfig.lib.json
    └── database/                   <-- Shared Database Models & Migrations
        ├── src/
        │   ├── index.ts
        │   └── database.module.ts
        └── tsconfig.lib.json
```

### 3.1 Path Aliases and Compilation Mechanics
The root `tsconfig.json` maps library names directly to their source entry points:
```json
{
  "compilerOptions": {
    "baseUrl": "./",
    "paths": {
      "@app/common": ["libs/common/src"],
      "@app/common/*": ["libs/common/src/*"],
      "@app/database": ["libs/database/src"],
      "@app/database/*": ["libs/database/src/*"]
    }
  }
}
```
During development (`nest start <app> --watch`), the Nest compiler reads source files directly from `libs/` via TypeScript path mapping, eliminating compilation overhead.

---

## 4. Concrete Implementation

### 4.1 Converting a Standard Project to a Monorepo
To transform a standard single-application NestJS project into a monorepo workspace:

```bash
# Generate a new app within an existing standard workspace
nest generate app order-worker
```
The Nest CLI automatically converts the workspace:
1. Moves the original application to `apps/<existing-name>/`.
2. Creates `apps/order-worker/`.
3. Rewrites `nest-cli.json` to monorepo mode.

### 4.2 Generating Shared Libraries

```bash
# Generate a reusable library
nest generate library common
```
The CLI prompts for a prefix (default `@app`) and sets up `libs/common/src/index.ts` with export bindings.

### 4.3 Workspace Configuration (`nest-cli.json`)

```json
{
  "$schema": "https://json.schemastore.org/nest-cli",
  "collection": "@nestjs/schematics",
  "sourceRoot": "apps/gateway/src",
  "monorepo": true,
  "root": "apps/gateway",
  "compilerOptions": {
    "webpack": false,
    "tsConfigPath": "apps/gateway/tsconfig.app.json"
  },
  "projects": {
    "gateway": {
      "type": "application",
      "root": "apps/gateway",
      "entryFile": "main",
      "sourceRoot": "apps/gateway/src",
      "compilerOptions": {
        "tsConfigPath": "apps/gateway/tsconfig.app.json"
      }
    },
    "order-worker": {
      "type": "application",
      "root": "apps/order-worker",
      "entryFile": "main",
      "sourceRoot": "apps/order-worker/src",
      "compilerOptions": {
        "tsConfigPath": "apps/order-worker/tsconfig.app.json"
      }
    },
    "common": {
      "type": "library",
      "root": "libs/common",
      "entryFile": "index",
      "sourceRoot": "libs/common/src",
      "compilerOptions": {
        "tsConfigPath": "libs/common/tsconfig.lib.json"
      }
    }
  }
}
```

### 4.4 Consuming Shared Libraries in Applications

```typescript
// apps/gateway/src/gateway.module.ts
import { Module } from '@nestjs/common';
import { CommonModule, CommonService } from '@app/common';
import { GatewayController } from './gateway.controller';

@Module({
  imports: [CommonModule],
  controllers: [GatewayController],
})
export class GatewayModule {}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Circular Library Dependencies
If `libs/auth` imports `@app/user` and `libs/user` imports `@app/auth`, TypeScript compiles successfully, but runtime execution crashes with `TypeError: Cannot read properties of undefined` during DI token resolution.
- **Prevention**: Enforce unidirectional layering: Core -> Common -> Domain Libs -> Apps.

### 5.2 Missing Barrel Exports
Omitting newly created providers or interfaces from `libs/<lib-name>/src/index.ts` forces consumers to use deep imports (e.g., `@app/common/services/foo.service`).
- **Remedy**: Always maintain clean barrel exports in `index.ts` and forbid deep path imports via ESLint rules (`no-restricted-imports`).

---

## 6. Performance & Memory Implications

- **Build Time**: Compiling all apps simultaneously can strain memory. Use targeted builds: `nest build gateway` or `nest build order-worker`.
- **Watch Mode Footprint**: Running multiple apps with `nest start <app> --watch` creates independent Node.js processes. For large monorepos, adopt build orchestrators like Turborepo or Nx alongside the Nest CLI.

---

## 7. Anti-Patterns & Common Mistakes

- **Duplicating DTOs Across Services**: Redefining user schemas in both `gateway` and `order-worker` instead of placing them in a shared library `@app/contracts`.
- **Treating Libraries as Standalone Microservices**: Trying to execute `nest start <lib-name>`. Libraries cannot be started directly; they are compiled as components of consumer applications.
- **Hardcoding Relative Imports into Sibling Apps**: Importing `../../apps/gateway/src/...` into `order-worker`. This breaks encapsulation and builds.

---

## 8. Production Checklist & Operational Readiness

- [ ] All cross-project references use configured `@app/*` or `@lib/*` TypeScript path aliases.
- [ ] No relative path imports (`../../libs`) bypass tsconfig path mappings.
- [ ] CI pipeline runs linting, formatting, and unit tests scoped to changed packages.
- [ ] Dockerfiles use multi-stage builds targeted at specific applications (`nest build <app>`).
- [ ] Circular dependencies between libraries are detected and prevented via linter rules.

---

## 9. Migration & Version History

- **NestJS 6.x**: Introduced the official monorepo workspace mode and `nest-cli.json` structure.
- **NestJS 8.x**: Added support for custom webpack configurations and enhanced path alias resolution.
- **NestJS 10.x**: Integrated fast compilation options with SWC in monorepo subprojects.
- **NestJS 11.x**: Improved monorepo schematic generation, Express 5 compatibility, and enhanced Vitest monorepo configuration support.

---

## 10. Verification & Testing

Testing a shared library within a monorepo workspace:

```typescript
// libs/common/src/common.service.spec.ts
import { Test, TestingModule } from '@nestjs/testing';
import { CommonService } from './common.service';

describe('CommonService', () => {
  let service: CommonService;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [CommonService],
    }).compile();

    service = module.get<CommonService>(CommonService);
  });

  it('should be defined', () => {
    expect(service).toBeDefined();
  });

  it('should format UUIDs correctly', () => {
    expect(service.formatId('123')).toBe('ID-123');
  });
});
```

---

## 11. Comprehensive Examples

### Multi-Stage Dockerfile for a Monorepo App

```dockerfile
# syntax=docker/dockerfile:1
FROM node:22-alpine AS builder

WORKDIR /usr/src/app

COPY package*.json ./
COPY tsconfig*.json ./
COPY nest-cli.json ./

RUN npm ci

COPY apps/gateway apps/gateway
COPY libs/ libs/

RUN npm run build gateway

FROM node:22-alpine AS production

WORKDIR /usr/src/app
ENV NODE_ENV=production

COPY package*.json ./
RUN npm ci --only=production

COPY --from=builder /usr/src/app/dist/apps/gateway ./dist

EXPOSE 3000
CMD ["node", "dist/main.js"]
```

---

## 12. Cross-References & Further Reading

- [03-cli-generators.md](./03-cli-generators.md) - CLI command reference
- [04-project-structure.md](./04-project-structure.md) - Project folder conventions
- [09-swc-and-vitest-setup.md](./09-swc-and-vitest-setup.md) - Accelerated monorepo builds with SWC
- [NestJS Monorepo Documentation](https://docs.nestjs.com/cli/monorepo)
