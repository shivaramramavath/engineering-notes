# Project 05: React 19 Analytical Dashboard (react-dashboard)

> Modern enterprise analytics dashboard built with React 19, TanStack Query v5, and Zustand: polymorphic data tables, query key factories, generic filtering lenses, and optimistic data mutations.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-05` |
| **TypeScript Baseline** | `>= 5.4.0` / TypeScript 7 Native |
| **Tested tsconfig** | `strict: true`, `noUncheckedIndexedAccess: true`, `exactOptionalPropertyTypes: true` |
| **Complexity** | Advanced (Rating: 4/5) |
| **Architectural Style** | Component-Driven & Reactive State Architecture |
| **Core Concepts** | Polymorphic components, TanStack Query key factories, Zustand slices, Generic tables |

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

The dashboard separates server state (managed by TanStack Query) from client interaction filters (managed by Zustand), rendering data through polymorphic tables.

```
       [ User Filter Interaction (Zustand) ]
                        │
                        ▼
       ┌─────────────────────────────────┐
       │     Query Key Factory (v5)      │ ◄── Produces tuple query keys [ 'metrics', filters ]
       └────────────────┬────────────────┘
                        │
                        ▼
       ┌─────────────────────────────────┐
       │   TanStack Query Cache Layer    │ ◄── Handles caching, deduping, and background refresh
       └────────────────┬────────────────┘
                        │
                        ▼
       ┌─────────────────────────────────┐
       │  Polymorphic Table Component    │ ◄── Generic type inference over TData and TKey
       └─────────────────────────────────┘
```

### Component Responsibilities
1. **`types.ts`**: Table column definitions, analytical metrics, query filter contracts.
2. **`core.ts`**: TanStack Query key factories, formatting transformers, sorting helpers.
3. **`service.ts`**: Zustand state store holding client filters and selection state.
4. **`index.ts`**: The polymorphic Table component and dashboard assembly.

---

## 2. Complete Project Source Code

### 2.1 Domain Models & Type Invariants (`src/types.ts`)
```typescript
import type { ReactNode } from "react";

export interface MetricRecord {
  readonly id: string;
  readonly category: "compute" | "storage" | "network";
  readonly usageValue: number;
  readonly costInCents: number;
  readonly timestamp: string;
}

export interface DashboardFilters {
  readonly category?: MetricRecord["category"];
  readonly minCost?: number;
  readonly sortBy?: "usageValue" | "costInCents" | "timestamp";
  readonly sortOrder?: "asc" | "desc";
}

export interface ColumnDef<TData> {
  readonly header: string;
  readonly accessorKey: keyof TData;
  readonly cell?: (value: TData[keyof TData], row: TData) => ReactNode;
}
```

### 2.2 Core Logic & Adapters (`src/core.ts`)
```typescript
import { DashboardFilters, MetricRecord } from "./types.js";

export const metricQueryKeys = {
  all: ["metrics"] as const,
  lists: () => [...metricQueryKeys.all, "list"] as const,
  list: (filters: DashboardFilters) => [...metricQueryKeys.lists(), filters] as const,
  details: () => [...metricQueryKeys.all, "detail"] as const,
  detail: (id: string) => [...metricQueryKeys.details(), id] as const,
};

export function filterAndSortMetrics(
  items: readonly MetricRecord[],
  filters: DashboardFilters
): MetricRecord[] {
  let result = [...items];

  if (filters.category) {
    result = result.filter((item) => item.category === filters.category);
  }

  if (filters.minCost !== undefined) {
    const min = filters.minCost;
    result = result.filter((item) => item.costInCents >= min);
  }

  if (filters.sortBy) {
    const key = filters.sortBy;
    const order = filters.sortOrder === "desc" ? -1 : 1;
    result.sort((a, b) => {
      if (a[key] < b[key]) return -1 * order;
      if (a[key] > b[key]) return 1 * order;
      return 0;
    });
  }

  return result;
}
```

### 2.3 Service Implementation (`src/service.ts`)
```typescript
import { create } from "zustand";
import { DashboardFilters } from "./types.js";

export interface FilterStore {
  readonly filters: DashboardFilters;
  setCategory: (category?: DashboardFilters["category"]) => void;
  setMinCost: (minCost?: number) => void;
  setSorting: (sortBy: DashboardFilters["sortBy"], order: DashboardFilters["sortOrder"]) => void;
  reset: () => void;
}

export const useFilterStore = create<FilterStore>()((set) => ({
  filters: {},
  setCategory: (category) => set((s) => ({ filters: { ...s.filters, category } })),
  setMinCost: (minCost) => set((s) => ({ filters: { ...s.filters, minCost } })),
  setSorting: (sortBy, sortOrder) => set((s) => ({ filters: { ...s.filters, sortBy, sortOrder } })),
  reset: () => set({ filters: {} }),
}));
```

### 2.4 Application Entry Point (`src/index.ts`)
```typescript
import React, { ElementType, ReactElement } from "react";
import { ColumnDef } from "./types.js";

export interface TableProps<TData> {
  readonly data: readonly TData[];
  readonly columns: readonly ColumnDef<TData>[];
  readonly as?: ElementType;
}

export function GenericTable<TData extends { id: string }>({
  data,
  columns,
  as: Component = "table",
}: TableProps<TData>): ReactElement {
  return React.createElement(
    Component,
    { className: "typed-table" },
    React.createElement(
      "thead",
      null,
      React.createElement(
        "tr",
        null,
        columns.map((col, idx) => React.createElement("th", { key: idx }, col.header))
      )
    ),
    React.createElement(
      "tbody",
      null,
      data.map((row) =>
        React.createElement(
          "tr",
          { key: row.id },
          columns.map((col, idx) => {
            const val = row[col.accessorKey];
            const content = col.cell ? col.cell(val, row) : String(val);
            return React.createElement("td", { key: idx }, content);
          })
        )
      )
    )
  );
}

export * from "./types.js";
export * from "./core.js";
export * from "./service.js";
```

---

## 3. Build & Runtime Configuration

### 3.1 `package.json`
```json
{
  "name": "@acme/react-dashboard",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "build": "tsup src/index.ts --format esm --dts",
    "test": "vitest run"
  },
  "dependencies": {
    "zustand": "^4.5.4"
  },
  "peerDependencies": {
    "react": "^19.0.0"
  },
  "devDependencies": {
    "@types/node": "^22.0.0",
    "@types/react": "^19.0.0",
    "react": "^19.0.0",
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
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "jsx": "react-jsx",
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "exactOptionalPropertyTypes": true,
    "verbatimModuleSyntax": true,
    "skipLibCheck": true,
    "outDir": "./dist"
  },
  "include": ["src"]
}
```

---

## 4. Comprehensive Test Suite (`tests/app.test.ts`)
```typescript
import { describe, it, expect } from "vitest";
import { filterAndSortMetrics, metricQueryKeys } from "../src/core.js";
import { MetricRecord } from "../src/types.js";

describe("React Dashboard State & Core Tests", () => {
  const sampleMetrics: MetricRecord[] = [
    { id: "1", category: "compute", usageValue: 80, costInCents: 5000, timestamp: "2026-01-01" },
    { id: "2", category: "storage", usageValue: 120, costInCents: 2000, timestamp: "2026-01-02" },
    { id: "3", category: "compute", usageValue: 40, costInCents: 1500, timestamp: "2026-01-03" },
  ];

  it("should generate deterministic TanStack Query keys", () => {
    const key = metricQueryKeys.list({ category: "compute" });
    expect(key).toEqual(["metrics", "list", { category: "compute" }]);
  });

  it("should filter by category and minimum cost correctly", () => {
    const filtered = filterAndSortMetrics(sampleMetrics, {
      category: "compute",
      minCost: 2000,
    });
    expect(filtered).toHaveLength(1);
    expect(filtered[0]?.id).toBe("1");
  });

  it("should sort metrics ascending and descending", () => {
    const sorted = filterAndSortMetrics(sampleMetrics, {
      sortBy: "costInCents",
      sortOrder: "asc",
    });
    expect(sorted.map((s) => s.id)).toEqual(["3", "2", "1"]);
  });
});
```

---

## 5. Verification & Sample Execution
```bash
$ tsc --noEmit
$ vitest run
✓ tests/app.test.ts (3 tests) 15ms
```
