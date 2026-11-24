# SWC and Vitest Setup

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 01-getting-started
- **Prerequisites**: [02-first-steps](./02-first-steps.md), [05-development-workflow](./05-development-workflow.md), [00-prerequisites/01-typescript-essentials](../00-prerequisites/01-typescript-essentials.md)

---

## 1. Context & Purpose

As NestJS codebases grow in size and complexity, standard TypeScript compilation (`tsc`) and test runners (`jest` with `ts-jest`) become substantial productivity bottlenecks. Cold build times and test execution can slow development iterations down from milliseconds to tens of seconds.

**SWC** (Speedy Web Compiler, written in Rust) and **Vitest** (modern Vite-powered testing framework) provide order-of-magnitude performance improvements. SWC accelerates development compilation by 10x–20x, while Vitest offers near-instant test start times, native ESM support, and instant watch mode re-execution while maintaining full compatibility with NestJS decorators and dependency injection.

---

## 2. Theoretical Foundations

Standard TypeScript compilation performs two distinct tasks:
1. **Type Checking**: Validates static types against interfaces, generics, and declarations.
2. **Transpilation**: Emits JavaScript code and decorator metadata (`emitDecoratorMetadata`).

SWC decouples transpilation from type checking. Written in Rust, SWC transforms TypeScript classes and decorators in parallel across native threads without performing full type resolution. Type checking is deferred to background processes (`tsc --noEmit`) or pre-commit / CI verification.

Vitest uses native ES modules and worker threads. Unlike Jest—which requires complex polyfills, custom module transformers, and heavy VM context recreation—Vitest leverages native Node ESM resolution and parallel worker pools for blazing-fast unit and integration testing.

---

## 3. Architecture & Internal Mechanics

```
Standard Toolchain (tsc + ts-jest):
TypeScript Source ──> [ tsc AST Parser (Single-threaded JS) ] ──> JS Output (Slow)
Unit Tests        ──> [ ts-jest In-Memory Transpile + VM ]    ──> Test Results (High latency)

Modern Toolchain (SWC + Vitest):
TypeScript Source ──> [ SWC Rust Multi-threaded Engine ]      ──> JS Output (10-20x faster)
Unit Tests        ──> [ Vitest Worker Pool + Native ESM ]      ──> Test Results (Sub-second)
Type Validation   ──> [ tsc --noEmit (Background / CI) ]      ──> Type Correctness
```

### 3.1 The Decorator Metadata Requirement in SWC
NestJS relies fundamentally on `reflect-metadata` and `emitDecoratorMetadata`. SWC must be explicitly configured with `@swc/plugin-transform-decorators-legacy` or the native legacy decorator transform to emit constructor parameter type metadata (`design:paramtypes`). Without this configuration, NestJS DI throws runtime `Cannot resolve dependency` errors.

---

## 4. Concrete Implementation

### 4.1 Configuring SWC with Nest CLI

Install the required SWC packages:
```bash
npm install --save-dev @swc/cli @swc/core
```

Create `.swcrc` in the project root:
```json
{
  "$schema": "https://swc.rs/schema.json",
  "sourceMaps": true,
  "jsc": {
    "parser": {
      "syntax": "typescript",
      "decorators": true,
      "dynamicImport": true
    },
    "transform": {
      "legacyDecorator": true,
      "decoratorMetadata": true
    },
    "target": "es2022",
    "keepClassNames": true,
    "baseUrl": "./"
  },
  "minify": false
}
```

Enable SWC in `nest-cli.json`:
```json
{
  "$schema": "https://json.schemastore.org/nest-cli",
  "collection": "@nestjs/schematics",
  "sourceRoot": "src",
  "compilerOptions": {
    "builder": "swc",
    "typeCheck": true
  }
}
```

Now `nest start --watch` uses SWC for lightning-fast incremental builds.

---

### 4.2 Configuring Vitest for NestJS

Install Vitest and supporting packages:
```bash
npm install --save-dev vitest unplugin-swc @swc/core vite-tsconfig-paths
```

Create `vitest.config.ts`:
```typescript
import { defineConfig } from 'vitest/config';
import swc from 'unplugin-swc';
import tsconfigPaths from 'vite-tsconfig-paths';

export default defineConfig({
  test: {
    globals: true,
    environment: 'node',
    root: './',
    include: ['src/**/*.spec.ts', 'test/**/*.spec.ts'],
    coverage: {
      provider: 'v8',
      reporter: ['text', 'json', 'html'],
    },
  },
  plugins: [
    tsconfigPaths(),
    swc.vite({
      module: { type: 'es6' },
      jsc: {
        transform: {
          legacyDecorator: true,
          decoratorMetadata: true,
        },
        keepClassNames: true,
      },
    }),
  ],
});
```

Update `package.json` test scripts:
```json
{
  "scripts": {
    "test": "vitest run",
    "test:watch": "vitest",
    "test:cov": "vitest run --coverage",
    "typecheck": "tsc --noEmit"
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Missing `decoratorMetadata`
If `decoratorMetadata: true` is missing from `.swcrc` or the Vitest plugin, SWC compiles classes without emitting `design:paramtypes`. NestJS DI fails at runtime with:
```text
Nest can't resolve dependencies of the CatsService (?). Please make sure that the argument CatRepository at index [0] is available in the CatsModule context.
```
**Fix**: Verify both `legacyDecorator: true` and `decoratorMetadata: true` are present.

### 5.2 Stripped Class Names in Production Bundles
If SWC minification mangles class names, providers using class token injection or logger contexts named via `this.constructor.name` display minified strings (e.g., `e`).
**Fix**: Ensure `keepClassNames: true` is configured in `jsc`.

---

## 6. Performance & Memory Implications

- **Transpilation Speed**: SWC compiles a 1,000-file NestJS application in under 400ms, compared to ~6.5 seconds for `tsc`.
- **Vitest Cold Start**: Vitest starts test execution in <100ms compared to Jest's 2–3s node initialization overhead.
- **Memory Consumption**: Vitest shares worker pools and uses native V8 optimizations, consuming 40–60% less RAM during full test suite runs.

---

## 7. Anti-Patterns & Common Mistakes

- **Skipping Type Checking in CI**: Relying solely on SWC without running `tsc --noEmit` in CI. SWC strips invalid types without complaining, allowing type errors into production.
- **Mixing Jest and Vitest Configs**: Leaving Jest setup files and `@types/jest` installed alongside Vitest, causing conflicting global type declarations.
- **Using Native ECMAScript Decorators Instead of Legacy**: NestJS requires legacy Stage 2 decorators with metadata reflection. Setting modern TC39 Stage 3 decorator flags breaks NestJS.

---

## 8. Production Checklist & Operational Readiness

- [ ] `.swcrc` configured with `legacyDecorator: true` and `decoratorMetadata: true`.
- [ ] `nest-cli.json` specifies `"builder": "swc"`.
- [ ] `vitest.config.ts` includes `unplugin-swc` with metadata emission enabled.
- [ ] CI pipeline includes `npm run typecheck` (`tsc --noEmit`) before running `vitest run`.
- [ ] E2E tests configured with Supertest and Vitest runner.

---

## 9. Migration & Version History

- **NestJS 10.x**: Official first-class integration with SWC via CLI builder plugin.
- **NestJS 11.x**: Full compatibility with Vitest 2.x and SWC 1.7+, optimizing modern Node 20/22 ESM runtimes.

---

## 10. Verification & Testing

Sample unit test executing with Vitest:

```typescript
// src/cats/cats.service.spec.ts
import { describe, it, expect, beforeEach } from 'vitest';
import { Test, TestingModule } from '@nestjs/testing';
import { CatsService } from './cats.service';

describe('CatsService (with Vitest)', () => {
  let service: CatsService;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [CatsService],
    }).compile();

    service = module.get<CatsService>(CatsService);
  });

  it('instantiates successfully with decorator metadata intact', () => {
    expect(service).toBeDefined();
    expect(service.findAll()).toEqual([]);
  });
});
```

---

## 11. Comprehensive Examples

### Vitest E2E Test Suite Example

```typescript
// test/app.e2e-spec.ts
import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import { Test, TestingModule } from '@nestjs/testing';
import { INestApplication } from '@nestjs/common';
import * as request from 'supertest';
import { AppModule } from '../src/app.module';

describe('AppController (e2e with Vitest)', () => {
  let app: INestApplication;

  beforeAll(async () => {
    const moduleFixture: TestingModule = await Test.createTestingModule({
      imports: [AppModule],
    }).compile();

    app = moduleFixture.createNestApplication();
    await app.init();
  });

  afterAll(async () => {
    await app.close();
  });

  it('/health (GET)', () => {
    return request(app.getHttpServer())
      .get('/health')
      .expect(200)
      .expect({ status: 'ok' });
  });
});
```

---

## 12. Cross-References & Further Reading

- [02-first-steps.md](./02-first-steps.md) - Project initialization
- [05-development-workflow.md](./05-development-workflow.md) - Tooling & dev server workflow
- [07-production/02-performance/03-benchmarking.md](../07-production/02-performance/03-benchmarking.md) - Profiling compilation and runtime speed
- [SWC Official Documentation](https://swc.rs)
- [Vitest Official Guide](https://vitest.dev)
