# Project 11: Mini State Manager (mini-state-manager)

> A predictable, unidirectional state container implementing the Redux pattern with selector memoization, subscription dispatchers, and composable middleware pipelines.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-11` |
| **Node Baseline** | `>= 22.0.0` (ES2024+) |
| **Complexity** | Intermediate (Rating: 3/5) |
| **Architectural Style** | Unidirectional Data Flow / Redux Pattern |
| **Core Concepts** | Immutable State, Action Dispatch, Pure Reducers, Middleware Pipeline, Selector Subscriptions |

---

## Table of Contents
1. [Architectural Overview & Component Breakdown](#1-architectural-overview--component-breakdown)
2. [Complete Project Source Code](#2-complete-project-source-code)
   - [2.1 Core Store Implementation (`src/store.js`)](#21-core-store-implementation-srcstorejs)
   - [2.2 Middleware & Composable Enhancers (`src/middleware.js`)](#22-middleware--composable-enhancers-srcmiddlewarejs)
   - [2.3 Selector Memoization Engine (`src/selector.js`)](#23-selector-memoization-engine-srcselectorjs)
   - [2.4 Module Entry Point (`src/index.js`)](#24-module-entry-point-srcindexjs)
3. [Build & Runtime Configuration (`package.json`)](#3-build--runtime-configuration-packagejson)
4. [Comprehensive Test Suite (`tests/app.test.js`)](#4-comprehensive-test-suite-testsapptestjs)
5. [Verification & Sample Execution](#5-verification--sample-execution)

---

## 1. Architectural Overview & Component Breakdown

The Mini State Manager enforces predictable mutation rules through unidirectional data flow. The complete application state lives inside a single immutable tree. The state can never be modified directly; transitions occur exclusively by dispatching descriptive plain-object actions. Actions flow through a chain of composable middlewares before reaching the root reducer function, which computes the subsequent state tree.

```
                     ┌───────────────────────────────────┐
                     │          Action Dispatch          │
                     └─────────────────┬─────────────────┘
                                       │
                                       ▼
                     ┌───────────────────────────────────┐
                     │       Middleware Pipeline         │
                     │  (Logging, Async Thunks, Filters) │
                     └─────────────────┬─────────────────┘
                                       │
                                       ▼
                     ┌───────────────────────────────────┐
                     │       Root Reducer Function       │
                     │    (prevState, action) => next    │
                     └─────────────────┬─────────────────┘
                                       │
                                       ▼
                     ┌───────────────────────────────────┐
                     │      Immutable State Store        │
                     └─────────────────┬─────────────────┘
                                       │
                     ┌─────────────────┴─────────────────┐
                     ▼                                   ▼
             Active Subscribers                 Selector Subscriptions
             (Render triggers)                  (Selective re-renders)
```

### Component Breakdown

1. **`store.js`**: Holds the internal state, executes reducer transitions, and manages subscription listeners.
2. **`middleware.js`**: Provides the functional pipeline mechanism (`applyMiddleware`) enabling logging, validation, and asynchronous action dispatch (thunk).
3. **`selector.js`**: Implements memoized selectors (`createSelector`) that prevent costly recalculations if source state branches remain referentially identical.
4. **`index.js`**: Exports the consolidated state management toolkit.

---

## 2. Complete Project Source Code

### 2.1 Core Store Implementation (`src/store.js`)

```javascript
/**
 * Action type sentinel for initial store hydration.
 */
export const ActionTypes = {
  INIT: '@@mini-state/INIT_' + Math.random().toString(36).substring(7)
};

/**
 * Creates a centralized state store.
 *
 * @param {Function} reducer - Pure function calculating next state
 * @param {*} [preloadedState] - Initial seed state
 * @param {Function} [enhancer] - Higher-order store enhancer (e.g. middleware)
 * @returns {object} Store instance with getState, dispatch, subscribe
 */
export function createStore(reducer, preloadedState, enhancer) {
  if (typeof preloadedState === 'function' && typeof enhancer === 'undefined') {
    enhancer = preloadedState;
    preloadedState = undefined;
  }

  if (typeof enhancer === 'function') {
    return enhancer(createStore)(reducer, preloadedState);
  }

  if (typeof reducer !== 'function') {
    throw new TypeError('Expected the root reducer to be a function.');
  }

  let currentReducer = reducer;
  let currentState = preloadedState;
  let currentListeners = new Set();
  let nextListeners = currentListeners;
  let isDispatching = false;

  function ensureCanMutateNextListeners() {
    if (nextListeners === currentListeners) {
      nextListeners = new Set(currentListeners);
    }
  }

  function getState() {
    if (isDispatching) {
      throw new Error('Cannot call store.getState() while reducer is executing.');
    }
    return currentState;
  }

  function subscribe(listener) {
    if (typeof listener !== 'function') {
      throw new TypeError('Expected the listener to be a function.');
    }
    if (isDispatching) {
      throw new Error('Cannot subscribe while reducer is executing.');
    }

    let isSubscribed = true;
    ensureCanMutateNextListeners();
    nextListeners.add(listener);

    return function unsubscribe() {
      if (!isSubscribed) return;
      if (isDispatching) {
        throw new Error('Cannot unsubscribe while reducer is executing.');
      }
      isSubscribed = false;
      ensureCanMutateNextListeners();
      nextListeners.delete(listener);
      currentListeners = nextListeners;
    };
  }

  function dispatch(action) {
    if (typeof action !== 'object' || action === null || typeof action.type === 'undefined') {
      throw new Error('Actions must be plain objects with a defined "type" property.');
    }
    if (isDispatching) {
      throw new Error('Reducers may not dispatch actions.');
    }

    try {
      isDispatching = true;
      currentState = currentReducer(currentState, action);
    } finally {
      isDispatching = false;
    }

    currentListeners = nextListeners;
    for (const listener of currentListeners) {
      listener();
    }

    return action;
  }

  // Hydrate initial state
  dispatch({ type: ActionTypes.INIT });

  return {
    getState,
    dispatch,
    subscribe
  };
}
```

### 2.2 Middleware & Composable Enhancers (`src/middleware.js`)

```javascript
/**
 * Composes single-argument functions from right to left.
 *
 * @param {...Function} funcs - Functions to compose
 * @returns {Function}
 */
export function compose(...funcs) {
  if (funcs.length === 0) return (arg) => arg;
  if (funcs.length === 1) return funcs[0];
  return funcs.reduce((a, b) => (...args) => a(b(...args)));
}

/**
 * Higher-order store enhancer enabling middleware chains.
 *
 * @param {...Function} middlewares
 * @returns {Function} Store enhancer
 */
export function applyMiddleware(...middlewares) {
  return (createStore) => (reducer, preloadedState) => {
    const store = createStore(reducer, preloadedState);
    let dispatch = () => {
      throw new Error('Dispatching while constructing your middleware is not allowed.');
    };

    const middlewareAPI = {
      getState: store.getState,
      dispatch: (action, ...args) => dispatch(action, ...args)
    };

    const chain = middlewares.map((middleware) => middleware(middlewareAPI));
    dispatch = compose(...chain)(store.dispatch);

    return {
      ...store,
      dispatch
    };
  };
}

/**
 * Built-in async thunk middleware.
 */
export const thunkMiddleware = ({ dispatch, getState }) => (next) => (action) => {
  if (typeof action === 'function') {
    return action(dispatch, getState);
  }
  return next(action);
};
```

### 2.3 Selector Memoization Engine (`src/selector.js`)

```javascript
/**
 * Shallow equality comparison for arrays and objects.
 */
function shallowEqual(a, b) {
  if (Object.is(a, b)) return true;
  if (typeof a !== 'object' || a === null || typeof b !== 'object' || b === null) return false;

  const keysA = Object.keys(a);
  const keysB = Object.keys(b);
  if (keysA.length !== keysB.length) return false;

  for (const key of keysA) {
    if (!Object.prototype.hasOwnProperty.call(b, key) || !Object.is(a[key], b[key])) {
      return false;
    }
  }
  return true;
}

/**
 * Creates a memoized selector to optimize component re-renders.
 *
 * @param {Array<Function>} inputSelectors - Extractor functions
 * @param {Function} resultFunc - Computation function
 * @returns {Function} Memoized selector
 */
export function createSelector(inputSelectors, resultFunc) {
  let lastArgs = null;
  let lastResult = null;

  return function (state) {
    const currentArgs = inputSelectors.map((fn) => fn(state));

    if (
      lastArgs !== null &&
      lastArgs.length === currentArgs.length &&
      lastArgs.every((val, idx) => Object.is(val, currentArgs[idx]))
    ) {
      return lastResult;
    }

    lastArgs = currentArgs;
    lastResult = resultFunc(...currentArgs);
    return lastResult;
  };
}
```

### 2.4 Module Entry Point (`src/index.js`)

```javascript
import { createStore, ActionTypes } from './store.js';
import { applyMiddleware, compose, thunkMiddleware } from './middleware.js';
import { createSelector } from './selector.js';

export {
  createStore,
  ActionTypes,
  applyMiddleware,
  compose,
  thunkMiddleware,
  createSelector
};
```

---

## 3. Build & Runtime Configuration (`package.json`)

```json
{
  "name": "mini-state-manager",
  "version": "1.0.0",
  "description": "Zero-dependency predictable state container with memoization and middleware pipelines",
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
import {
  createStore,
  applyMiddleware,
  thunkMiddleware,
  createSelector
} from '../src/index.js';

describe('Mini State Manager', () => {
  function counterReducer(state = { count: 0, items: [] }, action) {
    switch (action.type) {
      case 'INCREMENT':
        return { ...state, count: state.count + 1 };
      case 'DECREMENT':
        return { ...state, count: state.count - 1 };
      case 'ADD_ITEM':
        return { ...state, items: [...state.items, action.payload] };
      default:
        return state;
    }
  }

  it('hydrates initial state and handles synchronous dispatches', () => {
    const store = createStore(counterReducer);
    assert.deepEqual(store.getState(), { count: 0, items: [] });

    store.dispatch({ type: 'INCREMENT' });
    assert.equal(store.getState().count, 1);

    store.dispatch({ type: 'DECREMENT' });
    assert.equal(store.getState().count, 0);
  });

  it('notifies subscribers upon state change and supports unsubscription', () => {
    const store = createStore(counterReducer);
    let callCount = 0;

    const unsubscribe = store.subscribe(() => {
      callCount++;
    });

    store.dispatch({ type: 'INCREMENT' });
    store.dispatch({ type: 'INCREMENT' });
    assert.equal(callCount, 2);

    unsubscribe();
    store.dispatch({ type: 'INCREMENT' });
    assert.equal(callCount, 2); // Unsubscribed, does not fire
  });

  it('supports asynchronous actions via thunk middleware', async () => {
    const store = createStore(counterReducer, applyMiddleware(thunkMiddleware));

    const asyncIncrement = (amount) => (dispatch) => {
      return new Promise((resolve) => {
        setTimeout(() => {
          dispatch({ type: 'ADD_ITEM', payload: `item-${amount}` });
          resolve();
        }, 10);
      });
    };

    await store.dispatch(asyncIncrement(42));
    assert.deepEqual(store.getState().items, ['item-42']);
  });

  it('memoizes complex selectors and recalculates only when inputs change', () => {
    let computations = 0;
    const selectItems = (state) => state.items;
    const selectTotalLength = createSelector([selectItems], (items) => {
      computations++;
      return items.join(',').length;
    });

    const state1 = { count: 1, items: ['apple', 'banana'] };
    const res1 = selectTotalLength(state1);
    assert.equal(computations, 1);

    // Call again with same items reference
    const res2 = selectTotalLength({ count: 2, items: state1.items });
    assert.equal(res1, res2);
    assert.equal(computations, 1); // Memoized cache hit

    // Change items
    const res3 = selectTotalLength({ count: 2, items: ['orange'] });
    assert.equal(computations, 2); // Recomputed
    assert.equal(res3, 6);
  });
});
```

---

## 5. Verification & Sample Execution

```bash
$ node --test tests/app.test.js
✔ Mini State Manager > hydrates initial state and handles synchronous dispatches (0.81ms)
✔ Mini State Manager > notifies subscribers upon state change and supports unsubscription (0.42ms)
✔ Mini State Manager > supports asynchronous actions via thunk middleware (12.45ms)
✔ Mini State Manager > memoizes complex selectors and recalculates only when inputs change (0.49ms)
▶ Mini State Manager (15.22ms)
ℹ tests 4
ℹ suites 1
ℹ pass 4
ℹ fail 0
ℹ cancelled 0
ℹ skipped 0
ℹ todo 0
ℹ duration_ms 68.3
```
