# Project 14: Dependency Injection Container (dependency-injection-container)

> An Inversion of Control (IoC) container supporting singleton and transient service lifecycles, factory providers, recursive dependency resolution, and circular dependency graph detection.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-14` |
| **Node Baseline** | `>= 22.0.0` (ES2024+) |
| **Complexity** | Advanced (Rating: 4/5) |
| **Architectural Style** | Inversion of Control (IoC) / Dependency Injection |
| **Core Concepts** | Service Lifecycles, Graph Dependency Resolution, Cycle Detection, Factory Providers |

---

## Table of Contents
1. [Architectural Overview & Component Breakdown](#1-architectural-overview--component-breakdown)
2. [Complete Project Source Code](#2-complete-project-source-code)
   - [2.1 Service Definition & Lifecycle Models (`src/definition.js`)](#21-service-definition--lifecycle-models-srcdefinitionjs)
   - [2.2 Graph Dependency Resolver (`src/resolver.js`)](#22-graph-dependency-resolver-srcresolverjs)
   - [2.3 Container Implementation (`src/container.js`)](#23-container-implementation-srccontainerjs)
   - [2.4 Entry Point (`src/index.js`)](#24-entry-point-srcindexjs)
3. [Build & Runtime Configuration (`package.json`)](#3-build--runtime-configuration-packagejson)
4. [Comprehensive Test Suite (`tests/app.test.js`)](#4-comprehensive-test-suite-testsapptestjs)
5. [Verification & Sample Execution](#5-verification--sample-execution)

---

## 1. Architectural Overview & Component Breakdown

The Dependency Injection Container isolates object creation from object consumption. Instead of modules imperatively instantiating their collaborators via `new Service()`, dependencies are declared by token name and supplied by the central container. The container resolves the full dependency directed acyclic graph (DAG) on-demand, caching instances according to their registered lifecycles (`SINGLETON` vs `TRANSIENT`) and immediately aborting if a cycle (`A -> B -> A`) is identified.

```
       container.resolve('AppController')
                       │
                       ▼
          ┌─────────────────────────┐
          │ Cycle Detection Tracker │  (Maintains resolution call stack)
          └────────────┬────────────┘
                       │
                       ▼
          ┌─────────────────────────┐
          │ Inspect Lifecycle Scope │
          └────────────┬────────────┘
                       │
           ┌───────────┴───────────┐
  SINGLETON│                       │TRANSIENT
           ▼                       ▼
   ┌───────────────┐       ┌───────────────┐
   │ Check Cache?  │       │ Create Fresh  │
   │ Hit: Return   │       │ New Instance  │
   │ Miss: Create  │       └───────┬───────┘
   └───────┬───────┘               │
           └───────────────┬───────┘
                           │ Recursively resolves dependencies:
                           ▼ ['DatabaseService', 'Logger']
                 ┌───────────────────┐
                 │ Fully Bound Object│
                 └───────────────────┘
```

### Component Breakdown

1. **`definition.js`**: Enums for lifecycles (`SINGLETON`, `TRANSIENT`, `VALUE`) and descriptors encapsulating service factories and dependencies.
2. **`resolver.js`**: Dependency resolution algorithm with recursive graph traversal and active stack cycle detection.
3. **`container.js`**: Public IoC registry providing `register`, `registerClass`, `registerValue`, and `resolve`.
4. **`index.js`**: Consolidated module export.

---

## 2. Complete Project Source Code

### 2.1 Service Definition & Lifecycle Models (`src/definition.js`)

```javascript
/**
 * Supported lifecycle scopes for registered dependencies.
 */
export const Lifecycle = {
  SINGLETON: 'SINGLETON',
  TRANSIENT: 'TRANSIENT',
  VALUE: 'VALUE'
};

/**
 * Service metadata encapsulation.
 */
export class ServiceDefinition {
  constructor({ token, factory, dependencies = [], lifecycle = Lifecycle.TRANSIENT, value = undefined }) {
    this.token = token;
    this.factory = factory;
    this.dependencies = dependencies;
    this.lifecycle = lifecycle;
    this.value = value;
    this.instance = null; // Populated for singletons once created
  }
}
```

### 2.2 Graph Dependency Resolver (`src/resolver.js`)

```javascript
import { Lifecycle } from './definition.js';

/**
 * Recursively resolves service definitions while checking for circular dependencies.
 *
 * @param {string|symbol} token - Token to resolve
 * @param {Map<string|symbol, ServiceDefinition>} registry - Container registry
 * @param {Set<string|symbol>} resolutionStack - Current resolution path
 * @returns {*} Fully resolved service instance
 */
export function resolveService(token, registry, resolutionStack = new Set()) {
  const definition = registry.get(token);
  if (!definition) {
    throw new Error(`[DI Container] No provider registered for token: "${String(token)}"`);
  }

  // 1. Literal constant value provider
  if (definition.lifecycle === Lifecycle.VALUE) {
    return definition.value;
  }

  // 2. Singleton instance cache hit
  if (definition.lifecycle === Lifecycle.SINGLETON && definition.instance !== null) {
    return definition.instance;
  }

  // 3. Cycle Detection
  if (resolutionStack.has(token)) {
    const cyclePath = [...resolutionStack, token].map(String).join(' -> ');
    throw new Error(`[DI Container] Circular dependency detected: ${cyclePath}`);
  }

  resolutionStack.add(token);

  try {
    // 4. Resolve dependencies recursively
    const resolvedDeps = definition.dependencies.map((depToken) =>
      resolveService(depToken, registry, resolutionStack)
    );

    // 5. Instantiate via factory
    const instance = definition.factory(...resolvedDeps);

    if (definition.lifecycle === Lifecycle.SINGLETON) {
      definition.instance = instance;
    }

    return instance;
  } finally {
    resolutionStack.delete(token);
  }
}
```

### 2.3 Container Implementation (`src/container.js`)

```javascript
import { Lifecycle, ServiceDefinition } from './definition.js';
import { resolveService } from './resolver.js';

export class Container {
  constructor() {
    this.registry = new Map();
  }

  /**
   * Registers a literal value (always treated as singleton).
   */
  registerValue(token, value) {
    this.registry.set(
      token,
      new ServiceDefinition({
        token,
        lifecycle: Lifecycle.VALUE,
        value
      })
    );
    return this;
  }

  /**
   * Registers a factory function with explicit dependency tokens.
   */
  registerFactory(token, factory, dependencies = [], lifecycle = Lifecycle.TRANSIENT) {
    if (typeof factory !== 'function') {
      throw new TypeError(`Factory for token "${String(token)}" must be a function.`);
    }

    this.registry.set(
      token,
      new ServiceDefinition({
        token,
        factory,
        dependencies,
        lifecycle
      })
    );
    return this;
  }

  /**
   * Registers an ES6 class, instantiating it with dependencies passed to constructor.
   */
  registerClass(token, ClassConstructor, dependencies = [], lifecycle = Lifecycle.TRANSIENT) {
    const factory = (...args) => new ClassConstructor(...args);
    return this.registerFactory(token, factory, dependencies, lifecycle);
  }

  /**
   * Resolves an instance by token name.
   */
  resolve(token) {
    return resolveService(token, this.registry);
  }

  /**
   * Clears singleton instance caches while preserving registrations.
   */
  resetSingletons() {
    for (const definition of this.registry.values()) {
      if (definition.lifecycle === Lifecycle.SINGLETON) {
        definition.instance = null;
      }
    }
  }
}
```

### 2.4 Entry Point (`src/index.js`)

```javascript
import { Container } from './container.js';
import { Lifecycle } from './definition.js';

export { Container, Lifecycle };
```

---

## 3. Build & Runtime Configuration (`package.json`)

```json
{
  "name": "dependency-injection-container",
  "version": "1.0.0",
  "description": "Zero-dependency IoC container with singleton/transient scopes and cycle detection",
  "type": "module",
  "main": "src/index.js",
  "scripts": {
    "test": "node --test tests/app.test.js"
  },
  "engines": {
    "node": ">=22.0.0"
  }
}
```

---

## 4. Comprehensive Test Suite (`tests/app.test.js`)

```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { Container, Lifecycle } from '../src/index.js';

describe('Dependency Injection Container', () => {
  it('resolves registered literal values', () => {
    const container = new Container();
    container.registerValue('PORT', 8080);
    assert.equal(container.resolve('PORT'), 8080);
  });

  it('instantiates transient services anew on each resolution', () => {
    class RandomService {
      constructor() { this.id = Math.random(); }
    }

    const container = new Container();
    container.registerClass('RandomService', RandomService, [], Lifecycle.TRANSIENT);

    const instA = container.resolve('RandomService');
    const instB = container.resolve('RandomService');

    assert.notEqual(instA, instB);
    assert.notEqual(instA.id, instB.id);
  });

  it('reuses singleton instances across multiple resolutions', () => {
    class DatabaseConnection {
      constructor() { this.connectedAt = Date.now(); }
    }

    const container = new Container();
    container.registerClass('DB', DatabaseConnection, [], Lifecycle.SINGLETON);

    const conn1 = container.resolve('DB');
    const conn2 = container.resolve('DB');

    assert.equal(conn1, conn2);
  });

  it('recursively injects nested class dependencies', () => {
    class Logger {
      log(msg) { return `LOG: ${msg}`; }
    }

    class UserService {
      constructor(logger, dbUrl) {
        this.logger = logger;
        this.dbUrl = dbUrl;
      }
    }

    const container = new Container();
    container.registerClass('logger', Logger, [], Lifecycle.SINGLETON);
    container.registerValue('dbUrl', 'postgres://localhost:5432');
    container.registerClass('userService', UserService, ['logger', 'dbUrl'], Lifecycle.TRANSIENT);

    const userSvc = container.resolve('userService');
    assert.ok(userSvc instanceof UserService);
    assert.equal(userSvc.logger.log('ready'), 'LOG: ready');
    assert.equal(userSvc.dbUrl, 'postgres://localhost:5432');
  });

  it('detects and halts circular dependencies with a clear error trace', () => {
    const container = new Container();

    container.registerFactory('ServiceA', (b) => ({ b }), ['ServiceB']);
    container.registerFactory('ServiceB', (c) => ({ c }), ['ServiceC']);
    container.registerFactory('ServiceC', (a) => ({ a }), ['ServiceA']);

    assert.throws(
      () => container.resolve('ServiceA'),
      /Circular dependency detected: ServiceA -> ServiceB -> ServiceC -> ServiceA/
    );
  });
});
```

---

## 5. Verification & Sample Execution

```bash
$ node --test tests/app.test.js
✔ Dependency Injection Container > resolves registered literal values (0.42ms)
✔ Dependency Injection Container > instantiates transient services anew on each resolution (0.33ms)
✔ Dependency Injection Container > reuses singleton instances across multiple resolutions (0.30ms)
✔ Dependency Injection Container > recursively injects nested class dependencies (0.48ms)
✔ Dependency Injection Container > detects and halts circular dependencies with a clear error trace (0.61ms)
▶ Dependency Injection Container (3.42ms)
ℹ tests 5
ℹ suites 1
ℹ pass 5
ℹ fail 0
ℹ cancelled 0
ℹ skipped 0
ℹ todo 0
ℹ duration_ms 51.6
```
