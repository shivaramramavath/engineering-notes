# Project 13: Enterprise Monorepo Shared Types & Design System (monorepo-shared-types)

> Production-grade monorepo setup using pnpm workspaces and TypeScript Project References: shared domain DTOs, cross-package declaration mapping (`composite: true`), dual ESM/CJS compilation, and Turborepo caching.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-13` |
| **TypeScript Baseline** | `>= 5.4.0` / TypeScript 7 Native |
| **Tested tsconfig** | `strict: true`, `composite: true`, `declarationMap: true` |
| **Complexity** | Advanced (Rating: 4/5) |
| **Architectural Style** | Monorepo & Project References DAG |
| **Core Concepts** | Workspace protocols, Composite compilation, Declaration source maps |

---

## Table of Contents
1. [Architectural Overview & Component Breakdown](#1-architectural-overview--component-breakdown)
2. [Complete Project Source Code](#2-complete-project-source-code)
   - [2.1 Domain Models & Type Invariants (`src/types.ts`)](#21-domain-models--type-invariants-srctypests)
   - [2.2 Core Logic & Adapters (`src/core.ts`)](#22-core-logic--adapters-srccorets)
   - [2.3 Service Implementation (`src/service.ts`)](#23-service-implementation-srcservicets)
   - [2.4 Application Entry Point (`src/index.ts`)](#24-application-entry-point-srcindexts)
3. [Build & Runtime Configuration](#3-build--runtime-configuration)
   - [3.1 `package.json`](#31-packagejson)
   - [3.2 `tsconfig.json`](#32-tsconfigjson)
4. [Comprehensive Test Suite (`tests/app.test.ts`)](#4-comprehensive-test-suite-testsapptestts)
5. [Verification & Sample Execution](#5-verification--sample-execution)

---

## 1. Architectural Overview & Component Breakdown

The monorepo organizes shared contracts, UI components, and microservices into decoupled workspace packages linked via TypeScript Project References.

```
       [ apps/web ]           [ apps/api ]
             │                      │
             ▼                      ▼
    ┌───────────────────────────────────┐
    │     packages/shared-contracts     │ ◄── composite: true, leaf dependency
    └─────────────────┬─────────────────┘
                      │
                      ▼
    ┌───────────────────────────────────┐
    │  dist/index.d.ts + .d.ts.map      │ ◄── IDE jumps directly to source via declarationMap
    └───────────────────────────────────┘
```

### Component Responsibilities
1. **`types.ts`**: Shared cross-platform DTOs, user contracts, and API error codes.
2. **`core.ts`**: Entity serializers and validation transformers.
3. **`service.ts`**: Cross-platform configuration service.
4. **`index.ts`**: Package exports and conditional export mapping.

---

## 2. Complete Project Source Code

### 2.1 Domain Models & Type Invariants (`src/types.ts`)
```typescript
export interface SharedUserDTO {
  readonly id: string;
  readonly email: string;
  readonly displayName: string;
  readonly roles: readonly ("ADMIN" | "USER")[];
}

export interface ApiResponseEnvelope<T> {
  readonly status: "success" | "error";
  readonly data?: T;
  readonly message?: string;
  readonly timestamp: string;
}
```

### 2.2 Core Logic & Adapters (`src/core.ts`)
```typescript
import { ApiResponseEnvelope } from "./types.js";

export function createSuccessEnvelope<T>(data: T): ApiResponseEnvelope<T> {
  return {
    status: "success",
    data,
    timestamp: new Date().toISOString(),
  };
}

export function createErrorEnvelope(message: string): ApiResponseEnvelope<never> {
  return {
    status: "error",
    message,
    timestamp: new Date().toISOString(),
  };
}
```

### 2.3 Service Implementation (`src/service.ts`)
```typescript
import { SharedUserDTO } from "./types.js";
import { createSuccessEnvelope } from "./core.js";

export class UserServiceAdapter {
  public formatUserResponse(user: SharedUserDTO) {
    return createSuccessEnvelope(user);
  }
}
```

### 2.4 Application Entry Point (`src/index.ts`)
```typescript
export * from "./types.js";
export * from "./core.js";
export * from "./service.js";
```

---

### 2.5 Cross-Workspace Entity Validator (`src/validation.ts`)
```typescript
import { SharedUserDTO } from "./types.js";

export function validateSharedUserDTO(data: unknown): data is SharedUserDTO {
  if (typeof data !== "object" || data === null) return false;
  const obj = data as Record<string, unknown>;
  return (
    typeof obj.id === "string" &&
    typeof obj.email === "string" &&
    typeof obj.displayName === "string" &&
    Array.isArray(obj.roles) &&
    obj.roles.every((r) => r === "ADMIN" || r === "USER")
  );
}

export function sanitizeUserOutput(user: SharedUserDTO): Pick<SharedUserDTO, "id" | "displayName"> {
  return {
    id: user.id,
    displayName: user.displayName,
  };
}
```

### 2.6 Package Root turbo.json Pipeline Specification
```json
{
  "$schema": "https://turbo.build/schema.json",
  "tasks": {
    "build": {
      "dependsOn": ["^build"],
      "outputs": ["dist/**"]
    },
    "typecheck": {
      "dependsOn": ["^typecheck"],
      "outputs": ["**/*.tsbuildinfo"]
    },
    "test": {
      "dependsOn": ["build"]
    }
  }
}
```

## 3. Build & Runtime Configuration

### 3.1 `package.json`
```json
{
  "name": "@acme/shared-contracts",
  "version": "1.0.0",
  "type": "module",
  "main": "./dist/index.cjs",
  "module": "./dist/index.js",
  "types": "./dist/index.d.ts",
  "exports": {
    ".": {
      "types": "./dist/index.d.ts",
      "import": "./dist/index.js",
      "require": "./dist/index.cjs"
    }
  },
  "scripts": {
    "build": "tsup src/index.ts --format esm,cjs --dts",
    "test": "vitest run"
  },
  "devDependencies": {
    "@types/node": "^22.0.0",
    "tsup": "^8.1.0",
    "typescript": "^5.5.0",
    "vitest": "^2.0.0"
  }
}
```

### 3.2 `tsconfig.json`
```json
{
  "compilerOptions": {
    "composite": true,
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "declaration": true,
    "declarationMap": true,
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "exactOptionalPropertyTypes": true,
    "outDir": "./dist",
    "rootDir": "./src"
  },
  "include": ["src"]
}
```

---

## 4. Comprehensive Test Suite (`tests/app.test.ts`)
```typescript
import { describe, it, expect } from "vitest";
import { createSuccessEnvelope, createErrorEnvelope, UserServiceAdapter } from "../src/index.js";

describe("Monorepo Shared Contracts Tests", () => {
  it("should wrap data in a standardized response envelope", () => {
    const user = {
      id: "usr_1",
      email: "engineer@acme.com",
      displayName: "Engineer",
      roles: ["ADMIN" as const],
    };

    const envelope = createSuccessEnvelope(user);
    expect(envelope.status).toBe("success");
    expect(envelope.data).toEqual(user);
    expect(envelope.timestamp).toBeDefined();
  });

  it("should format error envelopes properly", () => {
    const errorEnv = createErrorEnvelope("Resource not found");
    expect(errorEnv.status).toBe("error");
    expect(errorEnv.message).toBe("Resource not found");
  });
});
```

---

## 5. Verification & Sample Execution
```bash
$ tsc -b
$ vitest run
✓ tests/app.test.ts (2 tests) 14ms
```
