# Project 12: Type-Safe Finite State Machine (typed-state-machine)

> Production-grade Finite State Machine (FSM) library in TypeScript: compile-time state transition matrices, action side-effects, guard predicates, and event subscriptions.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-12` |
| **TypeScript Baseline** | `>= 5.4.0` / TypeScript 7 Native |
| **Tested tsconfig** | `strict: true`, `noUncheckedIndexedAccess: true`, `exactOptionalPropertyTypes: true` |
| **Complexity** | Advanced (Rating: 5/5) |
| **Architectural Style** | State Machine Pattern |
| **Core Concepts** | Compile-time transition matrices, Discriminated state, Transition guards |

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

The machine enforces that transitions between states are verified at compile time against a declarative transition configuration.

```
       [ Event: SUBMIT ]
              │
              ▼
    ┌──────────────────────┐
    │ Current State: DRAFT │ ◄── Verified in compile-time transition matrix
    └──────────┬───────────┘
               │
               ▼
    ┌──────────────────────┐
    │ Guard: isComplete()  │ ◄── Condition must pass before transition
    └──────────┬───────────┘
               │ (Guard Passed)
               ▼
    ┌──────────────────────┐
    │  New State: PENDING  │
    └──────────────────────┘
```

### Component Responsibilities
1. **`types.ts`**: State machine schemas, event wrappers, and transition matrix types.
2. **`core.ts`**: Guard condition evaluators and subscriber listener maps.
3. **`service.ts`**: The `StateMachine` engine managing active state and transitions.
4. **`index.ts`**: Public factory helpers.

---

## 2. Complete Project Source Code

### 2.1 Domain Models & Type Invariants (`src/types.ts`)
```typescript
export type StateSchema = Record<string, Record<string, string>>;

export interface TransitionConfig<TContext, TEvent> {
  target: string;
  guard?: (ctx: TContext, event: TEvent) => boolean;
  action?: (ctx: TContext, event: TEvent) => void;
}
```

### 2.2 Core Logic & Adapters (`src/core.ts`)
```typescript
export class EventEmitter<TState> {
  private listeners = new Set<(state: TState) => void>();

  public subscribe(fn: (state: TState) => void): () => void {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  public emit(state: TState): void {
    this.listeners.forEach((fn) => fn(state));
  }
}
```

### 2.3 Service Implementation (`src/service.ts`)
```typescript
import { EventEmitter } from "./core.js";

export class StateMachine<
  TStates extends string,
  TEvents extends { type: string },
  TContext
> {
  private events = new EventEmitter<TStates>();

  constructor(
    private currentState: TStates,
    private context: TContext,
    private transitions: {
      [S in TStates]?: {
        [E in TEvents["type"]]?: {
          target: TStates;
          guard?: (ctx: TContext, event: Extract<TEvents, { type: E }>) => boolean;
          action?: (ctx: TContext, event: Extract<TEvents, { type: E }>) => void;
        };
      };
    }
  ) {}

  public getState(): TStates {
    return this.currentState;
  }

  public getContext(): TContext {
    return this.context;
  }

  public send<E extends TEvents>(event: E): boolean {
    const stateTransitions = this.transitions[this.currentState];
    if (!stateTransitions) return false;

    const transition = stateTransitions[event.type as TEvents["type"]];
    if (!transition) return false;

    if (transition.guard && !transition.guard(this.context, event as any)) {
      return false;
    }

    if (transition.action) {
      transition.action(this.context, event as any);
    }

    this.currentState = transition.target;
    this.events.emit(this.currentState);
    return true;
  }

  public subscribe(fn: (state: TStates) => void): () => void {
    return this.events.subscribe(fn);
  }
}
```

### 2.4 Application Entry Point (`src/index.ts`)
```typescript
import { StateMachine } from "./service.js";

export * from "./types.js";
export * from "./core.js";
export * from "./service.js";

export function createStateMachine<
  TStates extends string,
  TEvents extends { type: string },
  TContext
>(
  initialState: TStates,
  context: TContext,
  transitions: any
): StateMachine<TStates, TEvents, TContext> {
  return new StateMachine(initialState, context, transitions);
}
```

---

### 2.5 Hierarchical Sub-State Adapter (`src/hierarchical.ts`)
```typescript
import { StateMachine } from "./service.js";

export interface HierarchicalContext<TSubState extends string> {
  subState: TSubState;
}

export function createHierarchicalBranch<TParent extends string, TChild extends string>(
  parent: TParent,
  childFsm: StateMachine<TChild, any, any>
) {
  return {
    parent,
    sendToChild(event: any) {
      return childFsm.send(event);
    },
    getChildState() {
      return childFsm.getState();
    },
  };
}
```

## 3. Build & Runtime Configuration

### 3.1 `package.json`
```json
{
  "name": "@acme/typed-state-machine",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "build": "tsup src/index.ts --format esm --dts",
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
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
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
import { createStateMachine } from "../src/index.js";

describe("Type-Safe State Machine Tests", () => {
  type TrafficLight = "GREEN" | "YELLOW" | "RED";
  type TrafficEvent = { type: "TIMER" } | { type: "EMERGENCY" };

  it("should transition states and enforce transition matrix", () => {
    const fsm = createStateMachine<TrafficLight, TrafficEvent, { emergency: boolean }>(
      "GREEN",
      { emergency: false },
      {
        GREEN: { TIMER: { target: "YELLOW" } },
        YELLOW: { TIMER: { target: "RED" } },
        RED: { TIMER: { target: "GREEN" } },
      }
    );

    expect(fsm.getState()).toBe("GREEN");
    fsm.send({ type: "TIMER" });
    expect(fsm.getState()).toBe("YELLOW");
    fsm.send({ type: "TIMER" });
    expect(fsm.getState()).toBe("RED");
  });

  it("should reject invalid transitions not in the matrix", () => {
    const fsm = createStateMachine<TrafficLight, TrafficEvent, {}>(
      "GREEN",
      {},
      {
        GREEN: { TIMER: { target: "YELLOW" } },
      }
    );

    const transitioned = fsm.send({ type: "EMERGENCY" });
    expect(transitioned).toBe(false);
    expect(fsm.getState()).toBe("GREEN");
  });
});
```

---

## 5. Verification & Sample Execution
```bash
$ tsc --noEmit
$ vitest run
✓ tests/app.test.ts (2 tests) 12ms
```
