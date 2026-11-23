# Project [XX]: [Project Name] (`[slug]`)

> [One-line summary of project architecture, core capabilities, and real-world JavaScript engineering concepts implemented.]

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-[xx]-[slug]` |
| **Node Baseline** | Node.js v22 LTS / v24 |
| **Complexity** | [Intermediate / Advanced / Production-Grade] |
| **Architectural Style** | [Modular Clean Architecture / Event-Driven / Reactive / Microkernel] |
| **Core Concepts** | [Event Loop, Streams, Prototypes, WeakMap, Proxy, Web Workers, IndexedDB, etc.] |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture
[Detailed architectural rationale and interaction between components...]

```
+-------------------------------------------------------------+
|                      System Architecture                    |
|                                                             |
|  [ Client / Entrypoint ] ---> [ Router / Dispatcher ]      |
|                                       |                     |
|                                       v                     |
|                               [ Core Service ]              |
|                                /            \               |
|                               v              v              |
|                     [ Domain Models ]   [ Store / Buffer ]  |
+-------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Domain Models (`src/models/`):** Encapsulated data entities, immutable state records, and validation invariants.
- **Service & Engine (`src/services/`):** Core business logic, state machines, and algorithmic pipelines.
- **Infrastructure / Storage (`src/infra/`):** Persistence (IndexedDB/LocalStorage/Filesystem), network I/O, or event emitters.
- **CLI / Entrypoint Driver (`src/index.js`):** Standalone harness demonstrating execution and lifecycle management.

---

## 2. Complete Project Source Code

### Component 1: Domain Entities (`src/models/entity.js`)
```javascript
// Complete domain entities and validations
```

### Component 2: Service Engine (`src/services/engine.js`)
```javascript
// Complete business logic implementation
```

### Component 3: Storage & Infrastructure (`src/infra/store.js`)
```javascript
// Complete persistence or adapter implementation
```

### Component 4: Application Harness (`src/index.js`)
```javascript
// Complete runnable entry point
```

---

## 3. Build & Runtime Configuration

### `package.json`
```json
{
  "name": "proj-[slug]",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "start": "node src/index.js",
    "test": "node --test tests/*.test.js"
  }
}
```

---

## 4. Comprehensive Test Suite

### Unit & Integration Tests (`tests/app.test.js`)
```javascript
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';

describe('Project Test Suite', () => {
  test('core workflow executes accurately', async () => {
    // Complete verified tests
  });
});
```

---

## 5. Verification & Sample Execution

### Verification Log
```
$ node --test
▶ Project Test Suite
  ✔ core workflow executes accurately (12.4ms)
ℹ tests 1
ℹ pass 1
ℹ fail 0
```
"""
,Overwrite:true,Path: