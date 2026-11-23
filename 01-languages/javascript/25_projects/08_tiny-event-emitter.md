# Project 08: Tiny Universal EventEmitter (`tiny-event-emitter`)

> A lightweight, zero-dependency Universal EventEmitter library for Node.js and browsers, engineered with copy-on-write event loops, once-listeners, wildcard pattern matching, and async pipelines.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-08-tiny-event-emitter` |
| **Node Baseline** | Node.js v20+ LTS / Browser Evergreen |
| **Complexity** | Intermediate |
| **Architectural Style** | Publish-Subscribe Micro-Library |
| **Core Concepts** | Event-Driven Architecture, Copy-on-Write Dispatch, Wildcard Matching, Memory Leaks |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture
The library provides an ultra-fast event dispatch engine. It uses copy-on-write iteration to protect against mutation bugs while invoking listeners, and incorporates wildcard topic routing.

```
+-------------------------------------------------------------------------+
|                    Tiny EventEmitter Architecture                       |
|                                                                         |
|  [ Publisher ] ──► emit('order.created', data)                          |
|                           │                                             |
|                           ├──► [ Exact Match: 'order.created' ]         |
|                           │                                             |
|                           ├──► [ Wildcard Pattern: 'order.*' ]          |
|                           │                                             |
|                           └──► [ Global Wildcard: '*' ]                 |
|                                         │                               |
|                                         ▼                               |
|                    (Copy-on-Write Snapshot Execution)                   |
|                                         │                               |
|            ┌────────────────────────────┼────────────────────────────┐  |
|            ▼                            ▼                            ▼  |
|     [ Subscriber 1 ]             [ Subscriber 2 ]             [ Subscriber 3 ]
+-------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Core EventEmitter (`src/emitter.js`):** Implements subscription maps, copy-on-write dispatch loops, and leak detection thresholds.
- **Wildcard Matcher (`src/wildcard.js`):** Resolves wildcard topics (`namespace.*`).
- **Application Harness (`src/index.js`):** Exports the public module interface.

---

## 2. Complete Project Source Code

### Component 1: Wildcard Matcher (`src/wildcard.js`)
```javascript
export function matchWildcard(pattern, eventName) {
  if (pattern === '*' || pattern === eventName) return true;

  if (pattern.endsWith('.*')) {
    const prefix = pattern.slice(0, -2);
    return eventName.startsWith(prefix + '.');
  }

  return false;
}
```

### Component 2: Core EventEmitter Engine (`src/emitter.js`)
```javascript
import { matchWildcard } from './wildcard.js';

export class TinyEventEmitter {
  constructor(maxListeners = 10) {
    this._events = new Map();
    this._maxListeners = maxListeners;
  }

  on(event, listener) {
    if (typeof listener !== 'function') throw new TypeError('Listener must be a function');

    let list = this._events.get(event);
    if (!list) {
      list = [];
      this._events.set(event, list);
    }

    list.push(listener);

    if (this._maxListeners > 0 && list.length > this._maxListeners) {
      console.warn(`[MaxListenersExceeded] Event '${event}' exceeded ${this._maxListeners} listeners.`);
    }

    return () => this.off(event, listener); // Returns unbind function
  }

  once(event, listener) {
    const unbind = this.on(event, (...args) => {
      unbind();
      listener.apply(this, args);
    });
    unbind.original = listener;
    return unbind;
  }

  off(event, listener) {
    const list = this._events.get(event);
    if (!list) return this;

    const idx = list.findIndex(l => l === listener || l.original === listener);
    if (idx !== -1) {
      list.splice(idx, 1);
      if (list.length === 0) this._events.delete(event);
    }

    return this;
  }

  emit(event, ...args) {
    let delivered = false;

    // 1. Direct and wildcard dispatches
    for (const [pattern, listeners] of this._events.entries()) {
      if (matchWildcard(pattern, event)) {
        // Copy-on-write snapshot
        const snapshot = [...listeners];
        for (const listener of snapshot) {
          listener.apply(this, args);
        }
        if (snapshot.length > 0) delivered = true;
      }
    }

    return delivered;
  }

  async emitAsync(event, ...args) {
    const promises = [];

    for (const [pattern, listeners] of this._events.entries()) {
      if (matchWildcard(pattern, event)) {
        const snapshot = [...listeners];
        for (const listener of snapshot) {
          promises.push(Promise.resolve().then(() => listener.apply(this, args)));
        }
      }
    }

    return Promise.all(promises);
  }

  listenerCount(event) {
    return this._events.get(event)?.length || 0;
  }

  removeAllListeners(event) {
    if (event) {
      this._events.delete(event);
    } else {
      this._events.clear();
    }
  }
}
```

### Component 3: Module Interface (`src/index.js`)
```javascript
export { TinyEventEmitter } from './emitter.js';
export { matchWildcard } from './wildcard.js';
```

---

## 3. Build & Runtime Configuration

### `package.json`
```json
{
  "name": "proj-tiny-event-emitter",
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
import { TinyEventEmitter } from '../src/emitter.js';

describe('TinyEventEmitter Test Suite', () => {
  test('on and emit deliver messages with arguments', () => {
    const ee = new TinyEventEmitter();
    let received = null;

    ee.on('greet', (name) => {
      received = name;
    });

    ee.emit('greet', 'Antigravity');
    assert.equal(received, 'Antigravity');
  });

  test('once listener fires only once and self-deregisters', () => {
    const ee = new TinyEventEmitter();
    let count = 0;

    ee.once('tick', () => {
      count++;
    });

    ee.emit('tick');
    ee.emit('tick');
    assert.equal(count, 1);
    assert.equal(ee.listenerCount('tick'), 0);
  });

  test('Wildcard matching triggers namespace listeners', () => {
    const ee = new TinyEventEmitter();
    const actions = [];

    ee.on('auth.*', (status) => {
      actions.push(status);
    });

    ee.emit('auth.login', 'SUCCESS');
    ee.emit('auth.logout', 'SUCCESS');
    ee.emit('payment.success', 'IGNORED');

    assert.deepEqual(actions, ['SUCCESS', 'SUCCESS']);
  });

  test('emitAsync resolves after all asynchronous listeners complete', async () => {
    const ee = new TinyEventEmitter();
    let step = 0;

    ee.on('async-event', async () => {
      await new Promise(r => setTimeout(r, 20));
      step = 42;
    });

    await ee.emitAsync('async-event');
    assert.equal(step, 42);
  });
});
```

---

## 5. Verification & Sample Execution

### Verification Log
```
$ node --test tests/app.test.js
▶ TinyEventEmitter Test Suite
  ✔ on and emit deliver messages with arguments (1.8ms)
  ✔ once listener fires only once and self-deregisters (1.2ms)
  ✔ Wildcard matching triggers namespace listeners (1.4ms)
  ✔ emitAsync resolves after all asynchronous listeners complete (23.5ms)
✔ TinyEventEmitter Test Suite (30.1ms)

ℹ tests 4
ℹ pass 4
ℹ fail 0
ℹ duration_ms 44.8
```
