# Module 00: Node.js Module Systems — ESM & CommonJS for NestJS

> Understanding CommonJS (CJS) vs ECMAScript Modules (ESM) in the NestJS ecosystem, configuring `NodeNext` module resolution in TypeScript 5.7+, managing dynamic `import()`, and leveraging Node.js 22 LTS `require(esm)`.

| Metadata | Details |
| :--- | :--- |
| **NestJS Version** | NestJS 11+ |
| **Target Runtime** | Node.js >= 20.0.0 (Node.js 22 LTS recommended) |
| **Module Resolution** | `NodeNext` / `Bundler` |
| **Module Systems** | Dual: CommonJS runtime default with pure ESM compatibility |
| **TypeScript Config** | `"module": "NodeNext"`, `"moduleResolution": "NodeNext"` |

---

## 1. Concept Overview (Why before How)

For years, NestJS defaulted strictly to CommonJS (`"module": "commonjs"`) because early TypeScript reflection (`emitDecoratorMetadata`) and tooling chains (Webpack, Jest, ts-node) relied on synchronous `require()` calls to evaluate class constructors before instantiating the Dependency Injection container.

However, the modern JavaScript and npm ecosystem has transitioned decisively toward pure ECMAScript Modules (ESM). Major libraries (including modern versions of `node-fetch`, `chalk`, `p-limit`, and `@octokit/*`) ship exclusively as ESM. If a NestJS application is configured in legacy CommonJS mode, attempting to `import` or `require` a pure-ESM package throws `ERR_REQUIRE_ESM`.

In NestJS 11 on Node.js 22 LTS, developers have two architectural pathways:
1. **Native ESM Mode**: Configure `"type": "module"` in `package.json` with `"moduleResolution": "NodeNext"` in `tsconfig.json`.
2. **Modern CJS with `require(esm)`**: Run CommonJS on Node.js 22.12+ / 24+, where Node natively allows synchronous `require()` of synchronous ESM modules without throwing `ERR_REQUIRE_ESM`.

---

## 2. Architecture & Data Flow Diagram

```
+----------------------------------------------------------------------------------------------------+
|                                    Module Resolution in NestJS                                     |
|                                                                                                    |
|  [ tsconfig.json: "module": "NodeNext" ] ──> [ TypeScript Compiler / SWC ]                         |
|                                                      │                                             |
|                                                      ▼ Emits .js with relative extensions          |
|  [ Inbound Request ] ──> [ Bootstrap: NestFactory.create(AppModule) ]                              |
|                                     │                                                              |
|                                     ├──> Mode A: Native ESM ("type": "module")                     |
|                                     │    - Asynchronous 3-phase loading (Parse, Instantiate, Eval) |
|                                     │    - Top-Level await enabled                                 |
|                                     │    - Mandatory relative extensions: import './app.js'        |
|                                     │                                                              |
|                                     └──> Mode B: CommonJS with Node 22 require(esm)                |
|                                          - Synchronous execution                                   |
|                                          - require('pure-esm-pkg') permitted if no top-level await|
+----------------------------------------------------------------------------------------------------+
```

---

## 3. Core Mechanics & Deep Dive

### 3.1 CommonJS vs Native ESM Mechanics

```typescript
// CommonJS (Legacy NestJS default)
const { Module } = require('@nestjs/common');
module.exports = { AppModule };

// ECMAScript Modules (Modern standard)
import { Module } from '@nestjs/common';
export class AppModule {}
```

### 3.2 The Extension Mandate in Native ESM
When running native ESM in Node.js, relative imports **must explicitly include the `.js` file extension**, even when writing TypeScript source files:
```typescript
// Correct in ESM:
import { UsersService } from './users.service.js';

// Error in ESM (throws ERR_MODULE_NOT_FOUND):
import { UsersService } from './users.service';
```

### 3.3 Dynamic Imports as a Bridge
When running in CommonJS mode but consuming a pure ESM library, dynamic `import()` is used inside asynchronous factory providers:
```typescript
export const GeoipProvider = {
  provide: 'GEOIP_CLIENT',
  useFactory: async () => {
    // Dynamic import works across both CJS and ESM boundaries
    const { default: maxmind } = await import('maxmind');
    return maxmind;
  },
};
```

---

## 4. Complete Code Examples (5 Levels)

### 4.1 Basic Example: `tsconfig.json` for Native ESM
```json
{
  "compilerOptions": {
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "target": "ES2023",
    "emitDecoratorMetadata": true,
    "experimentalDecorators": true,
    "sourceMap": true,
    "outDir": "./dist",
    "strict": true,
    "skipLibCheck": true
  },
  "include": ["src/**/*"]
}
```

### 4.2 Real-World Production Example: Pure ESM Service Consumer
```typescript
import { Injectable, OnModuleInit } from '@nestjs/common';

@Injectable()
export class MarkdownService implements OnModuleInit {
  private markedInstance: any;

  async onModuleInit() {
    // Dynamically loading pure-ESM marked library safely
    const { marked } = await import('marked');
    this.markedInstance = marked;
  }

  parse(content: string): string {
    return this.markedInstance.parse(content);
  }
}
```

### 4.3 Edge Case: Resolving Directory Paths in ESM
In CommonJS, `__dirname` and `__filename` are injected globals. In native ESM, they do not exist:
```typescript
import { Injectable } from '@nestjs/common';
import { join } from 'node:path';

@Injectable()
export class TemplateService {
  getTemplatePath(name: string): string {
    // Modern Node.js 20.11+ / 22+ standard in ESM:
    const baseDir = import.meta.dirname;
    return join(baseDir, 'templates', `${name}.hbs`);
  }
}
```

### 4.4 Failure Case & Debugging: The `ERR_REQUIRE_ESM` Crash
```typescript
// Problem: Calling require() or static CJS import on a pure ESM package:
// const pLimit = require('p-limit');
// Crash output:
// Error [ERR_REQUIRE_ESM]: require() of ES Module /node_modules/p-limit/index.js from app.js not supported.

// Fix: Use dynamic import or upgrade runtime to Node.js 22 LTS (22.12+)
export async function getLimiter(concurrency: number) {
  const { default: pLimit } = await import('p-limit');
  return pLimit(concurrency);
}
```

### 4.5 Idiomatic Production Recipe: Dual-Module Compatible Configuration
```typescript
import { Module } from '@nestjs/common';
import { ConfigModule } from '@nestjs/config';
import { UsersController } from './users.controller.js';
import { UsersService } from './users.service.js';

@Module({
  imports: [ConfigModule.forRoot({ isGlobal: true })],
  controllers: [UsersController],
  providers: [UsersService],
  exports: [UsersService],
})
export class UsersModule {}
```

---

## 5. Framework Internals (Under the Hood)
When Nest boots via `NestFactory.create(AppModule)`:
1. V8 parses the module graph.
2. In CommonJS, `require()` executes synchronously; V8 caches modules in `require.cache`.
3. In native ESM, V8 evaluates modules in three separate phases:
   - **Construction**: Finding and downloading all dependencies recursively.
   - **Instantiation**: Allocating memory spaces for all exported bindings without running code.
   - **Evaluation**: Running top-level code and filling the allocated memory spaces.
4. Because NestJS relies on decorators (`@Module`, `@Injectable`), the decorator functions run during the **Evaluation phase**, registering metadata into the global `Reflect` registry before `NestFactory.create` finishes.

---

## 6. Common Pitfalls & Mistakes Table

| Mistake | Why It Happens | Idiomatic Fix |
| :--- | :--- | :--- |
| **1. Missing `.js` Extension** | Omitting extension in native ESM imports. | Always write `import { x } from './x.js'` in ESM TypeScript. |
| **2. Using `__dirname` in ESM** | `__dirname` is a CommonJS parameter, not an ESM global. | Use `import.meta.dirname` (Node 20.11+) or `fileURLToPath(import.meta.url)`. |
| **3. `ERR_REQUIRE_ESM` on npm package**| Requiring pure-ESM package from CJS Nest app. | Use asynchronous dynamic `import()` or run Node 22.12+. |
| **4. Incompatible Jest Config** | Jest historically struggles with native ESM and TS decorators. | Use Vitest (`vitest`) or SWC (`@swc/jest`) configured for ESM. |
| **5. Broken Type Exports** | Forgetting `"exports"` field in library `package.json`. | Provide explicit `"import"` and `"require"` condition blocks in `exports`. |
| **6. Top-Level Await Crash in CJS** | Attempting `require(esm)` on an ESM module that has top-level `await`. | Remove top-level `await` or consume via dynamic `await import()`. |

---

## 7. Performance & Memory Impact
- **Startup Speed**: Native ESM with SWC boots ~25% faster than legacy ts-node/CJS on large graphs.
- **Tree-Shaking**: Native ESM allows production bundlers (ESBuild, Rollup) to statically eliminate unused service methods and dead code, reducing Docker image artifact sizes.

---

## 8. Security Considerations
- **Prototype Pollution via CJS exports**: Mutating `module.exports` can pollute shared runtime objects. Native ESM live bindings are immutable exports, preventing prototype tampering.
- **Dependency Confusion**: Verify package integrity via `npm audit` and lockfile version pins (`package-lock.json`).

---

## 9. Testing Strategies (Unit & E2E)

```typescript
import { describe, it, expect } from 'vitest';
import { MarkdownService } from './markdown.service.js';

describe('MarkdownService ESM Dynamic Loading', () => {
  it('initializes and parses markdown', async () => {
    const service = new MarkdownService();
    await service.onModuleInit();
    const html = service.parse('# Title');
    expect(html).toContain('<h1>Title</h1>');
  });
});
```

---

## 10. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: Why did NestJS historically prefer CommonJS over ESM?</strong></summary>

**Answer:**  
NestJS relies heavily on TypeScript decorator reflection (`reflect-metadata`). In CommonJS, `require()` executes synchronously, allowing decorators to synchronously register classes into the metadata registry before the application finishes loading. Native ESM requires asynchronous module resolution and enforces strict static analysis, which initially caused tooling incompatibilities with Jest and older TypeScript compilers.
</details>

<details>
<summary><strong>Q2: How does Node.js 22 LTS resolve the ESM/CJS dilemma with `require(esm)`?</strong></summary>

**Answer:**  
Starting in Node.js 22.12.0 and Node.js 23+, synchronous `require(esm)` is enabled by default. If a CommonJS NestJS project requires a pure-ESM npm package, Node.js will load it synchronously without throwing `ERR_REQUIRE_ESM`, provided the target package does not use top-level `await`.
</details>

<details>
<summary><strong>Q3: What happens if an ESM module contains top-level `await` and you attempt to `require()` it?</strong></summary>

**Answer:**  
Node.js throws `ERR_REQUIRE_ASYNC_MODULE`. Because `require()` is fundamentally synchronous and cannot return an unresolved Promise, modules with top-level `await` can only be loaded via asynchronous dynamic `await import()`.
</details>

<details>
<summary><strong>Q4: Why must you write `.js` in relative imports when authoring TypeScript for NodeNext ESM?</strong></summary>

**Answer:**  
Under `NodeNext` module resolution, TypeScript outputs standard ECMAScript code and does not modify import specifiers. The ECMAScript specification and Node.js ESM loader mandate explicit file extensions for file resolution. Therefore, TypeScript enforces writing `./file.js` in your `.ts` source files.
</details>

<details>
<summary><strong>Q5: How do you mock pure-ESM packages in NestJS unit tests?</strong></summary>

**Answer:**  
Use modern test runners like Vitest with `vi.mock('./module.js')` or Node.js native test runner `node:test` with module loader customization hooks (`module.register`). Legacy Jest requires `--experimental-vm-modules` to support ESM mocking.
</details>

<details>
<summary><strong>Q6: What is the difference between `import.meta.url` and `__filename`?</strong></summary>

**Answer:**  
`__filename` is a string file path injected into the CommonJS module wrapper function by Node.js. `import.meta.url` is a standard ECMAScript file URL string (e.g. `file:///app/src/main.js`). It can be converted to an OS path using `node:url` `fileURLToPath(import.meta.url)`.
</details>

<details>
<summary><strong>Q7: Can a NestJS controller have top-level await in native ESM mode?</strong></summary>

**Answer:**  
Yes, in native ESM (`"type": "module"`), top-level await is permitted at the module root (for example, to establish a connection or load a configuration before bootstrapping). However, best practice in NestJS is to place asynchronous initialization inside lifecycle hooks (`OnModuleInit`).
</details>

<details>
<summary><strong>Q8: How does SWC handle TypeScript decorators compared to tsc?</strong></summary>

**Answer:**  
SWC supports legacy TypeScript decorators and parameter reflection via its `@swc/core` configuration (`jsc.transform.legacyDecorator: true` and `jsc.transform.decoratorMetadata: true`), compiling files up to 20x faster than `tsc`.
</details>

---

## 11. Hands-on Exercises

<details>
<summary><strong>Exercise 1 (Easy): Replace `__dirname` with standard ESM syntax</strong></summary>

**Prompt:** Rewrite a path join using Node.js 22 ESM standard without CommonJS globals.  
**Solution:**
```typescript
import { join } from 'node:path';
const configPath = join(import.meta.dirname, 'config.json');
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Dynamic Import Factory</strong></summary>

**Prompt:** Create an asynchronous factory provider that dynamically imports a pure ESM library.  
**Solution:**
```typescript
export const NanoIdProvider = {
  provide: 'NANOID',
  useFactory: async () => {
    const { nanoid } = await import('nanoid');
    return nanoid;
  },
};
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Configure tsconfig for NodeNext</strong></summary>

**Prompt:** Write the minimal `compilerOptions` required for NestJS 11 with NodeNext ESM.  
**Solution:**
```json
{
  "compilerOptions": {
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "experimentalDecorators": true,
    "emitDecoratorMetadata": true,
    "target": "ES2023"
  }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Conditional Import Loader</strong></summary>

**Prompt:** Implement a service helper that catches `ERR_REQUIRE_ESM` and falls back to dynamic import.  
**Solution:**
```typescript
export async function safeLoadPackage(name: string) {
  try {
    return require(name);
  } catch (err: any) {
    if (err.code === 'ERR_REQUIRE_ESM') {
      return await import(name);
    }
    throw err;
  }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Custom Module Loader with `import.meta.resolve`</strong></summary>

**Prompt:** Use modern `import.meta.resolve` in Node.js 22 to check if an optional plugin package is installed before bootstrapping.  
**Solution:**
```typescript
export async function isPluginAvailable(pkgName: string): Promise<boolean> {
  try {
    await import.meta.resolve(pkgName);
    return true;
  } catch {
    return false;
  }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): ESM Vitest Setup for NestJS</strong></summary>

**Prompt:** Configure a `vitest.config.ts` supporting NestJS metadata reflection and ESM imports.  
**Solution:**
```typescript
import { defineConfig } from 'vitest/config';
import unpluginSwc from 'unplugin-swc';

export default defineConfig({
  plugins: [
    unpluginSwc.vite({
      jsc: {
        transform: {
          legacyDecorator: true,
          decoratorMetadata: true,
        },
      },
    }),
  ],
  test: {
    globals: true,
    environment: 'node',
  },
});
```
</details>

---

## 12. Version Differences & Migration Notes (Nest 10 vs Nest 11)
- **Node.js 18 Drop**: NestJS 11 drops Node 18, requiring Node >= 20.0.0.
- **Node 22 `require(esm)`**: Node 22.12+ removes friction between CJS Nest apps and pure-ESM npm libraries.
- **Import Extensions**: When migrating to pure ESM in Nest 11, codemods must add `.js` extensions across all relative imports.

---

## 13. Further Reading & References
- [Node.js Official ESM Documentation](https://nodejs.org/api/esm.html)
- [TypeScript NodeNext Module Resolution](https://www.typescriptlang.org/docs/handbook/modules/reference.html#node16-nodenext)
- [NestJS Package Guidelines](https://docs.nestjs.com/)
