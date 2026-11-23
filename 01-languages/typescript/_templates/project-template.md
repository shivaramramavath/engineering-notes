# Project [XX]: [Project Name] ([slug])

> [One-line executive summary blockquote explaining the architecture, purpose, and design of this TypeScript engineering project.]

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-[XX]` |
| **TypeScript Baseline** | `>= 5.4.0` / TypeScript 7 Native |
| **Tested tsconfig** | `strict: true`, `noUncheckedIndexedAccess: true`, `exactOptionalPropertyTypes: true` |
| **Complexity** | [Intermediate / Advanced] (Rating: X/5) |
| **Architectural Style** | [e.g. Hexagonal / Domain-Driven / Type-Driven Design] |
| **Core Concepts** | [Key TypeScript type-level and runtime patterns] |

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

[Detailed architectural breakdown explaining how the design leverages TypeScript's type system to make invalid states unrepresentable.]

```
       [ Request Input ]
              │
              ▼
    ┌──────────────────┐
    │ Schema / Parser  │ ◄── Validates & narrows untrusted input
    └─────────┬────────┘
              │ (Branded Domain Model)
              ▼
    ┌──────────────────┐
    │  Domain Service  │ ◄── Pure, side-effect-free business logic
    └─────────┬────────┘
              │
              ▼
    ┌──────────────────┐
    │ Storage / Driver │ ◄── Type-safe repository abstraction
    └──────────────────┘
```

### Component Responsibilities
1. **`types.ts`**: Pure type declarations, branded types, discriminated unions, and invariants.
2. **`core.ts`**: Core business domain logic, validation transformers, and interfaces.
3. **`service.ts`**: Application service coordinating persistence and operations.
4. **`index.ts`**: Dependency injection container assembly and public exports.

---

## 2. Complete Project Source Code

### 2.1 Domain Models & Type Invariants (`src/types.ts`)
```typescript
// Complete verified types
```

### 2.2 Core Logic & Adapters (`src/core.ts`)
```typescript
// Complete verified logic
```

### 2.3 Service Implementation (`src/service.ts`)
```typescript
// Complete verified service
```

### 2.4 Application Entry Point (`src/index.ts`)
```typescript
// Complete entry point
```

---

## 3. Build & Runtime Configuration

### 3.1 `package.json`
```json
{
  "name": "project-slug",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "build": "tsc",
    "test": "vitest run"
  }
}
```

### 3.2 `tsconfig.json`
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "exactOptionalPropertyTypes": true,
    "verbatimModuleSyntax": true,
    "skipLibCheck": true
  }
}
```

---

## 4. Comprehensive Test Suite (`tests/app.test.ts`)
```typescript
// Complete verified test suite
```

---

## 5. Verification & Sample Execution
```bash
$ tsc --noEmit
$ vitest run
```
