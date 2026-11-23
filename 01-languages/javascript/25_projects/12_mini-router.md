# Project 12: Mini Client-Side Router (mini-router)

> A lightweight, zero-dependency Single Page Application (SPA) router featuring path-to-regex matching, parameterized routes, query parsing, and asynchronous navigation guards.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-12` |
| **Node Baseline** | `>= 22.0.0` (ES2024+) |
| **Complexity** | Intermediate (Rating: 3/5) |
| **Architectural Style** | Declarative Routing / Event-Driven Controller |
| **Core Concepts** | HTML5 History API, Path-to-RegExp, Parameter Parsing, Navigation Guards, SPA Lifecycle |

---

## Table of Contents
1. [Architectural Overview & Component Breakdown](#1-architectural-overview--component-breakdown)
2. [Complete Project Source Code](#2-complete-project-source-code)
   - [2.1 Route Matcher Engine (`src/matcher.js`)](#21-route-matcher-engine-srcmatcherjs)
   - [2.2 History & Location Manager (`src/history.js`)](#22-history--location-manager-srchistoryjs)
   - [2.3 Router Controller & Navigation Guards (`src/router.js`)](#23-router-controller--navigation-guards-srcrouterjs)
   - [2.4 Entry Point (`src/index.js`)](#24-entry-point-srcindexjs)
3. [Build & Runtime Configuration (`package.json`)](#3-build--runtime-configuration-packagejson)
4. [Comprehensive Test Suite (`tests/app.test.js`)](#4-comprehensive-test-suite-testsapptestjs)
5. [Verification & Sample Execution](#5-verification--sample-execution)

---

## 1. Architectural Overview & Component Breakdown

The Mini Router orchestrates client-side transitions without triggering full browser page refreshes. By intercepting navigation intents and listening to the `popstate` browser event, it updates the view dynamically while maintaining bookmarkable URLs and browser history synchronization.

```
       User Click / router.navigate(url)
                       │
                       ▼
         ┌───────────────────────────┐
         │   Global Route Guards     │  (e.g., Auth, Permission checks)
         │    beforeEach(to, from)   │
         └─────────────┬─────────────┘
                       │ Allowed?
               ┌───────┴───────┐
            Yes│               │ No (Aborted/Redirected)
               ▼               ▼
         ┌───────────┐   ┌───────────┐
         │ PushState │   │ Terminate │
         └─────┬─────┘   └───────────┘
               │
               ▼
         ┌───────────────────────────┐
         │   Regex Pattern Matcher   │  (Matches /users/:id to /users/42)
         └─────────────┬─────────────┘
                       │
                       ▼
         ┌───────────────────────────┐
         │ View Render / Transition  │  (Renders handler with params & query)
         └───────────────────────────┘
```

### Component Breakdown

1. **`matcher.js`**: Transforms Express-style route strings (e.g. `/users/:id/edit`) into compiled Regular Expressions with extracted parameter key mappings.
2. **`history.js`**: Encapsulates browser `window.history` interactions (`pushState`, `replaceState`) or an in-memory stack for headless environments.
3. **`router.js`**: Manages the registered route table, handles navigation pipelines, executes asynchronous guard interceptors, and notifies view observers.
4. **`index.js`**: Exports the unified router package.

---

## 2. Complete Project Source Code

### 2.1 Route Matcher Engine (`src/matcher.js`)

```javascript
/**
 * Compiles a route pattern into a RegExp and parameter list.
 * Supports static paths, params (:id), and wildcards (*).
 *
 * @param {string} path - Route pattern string (e.g. '/users/:id')
 * @returns {{ regex: RegExp, keys: string[] }}
 */
export function compileRoute(path) {
  const keys = [];
  const normalizedPath = path.trim().replace(/\/+$/, '') || '/';

  // Replace :paramName with regex capture group
  const regexPattern = normalizedPath
    .replace(/\/:([a-zA-Z0-9_]+)/g, (_, key) => {
      keys.push(key);
      return '/([^/]+)';
    })
    .replace(/\/\*/g, () => {
      keys.push('wildcard');
      return '/(.*)';
    });

  const regex = new RegExp(`^${regexPattern}$`);
  return { regex, keys };
}

/**
 * Parses query strings into key-value parameter records.
 *
 * @param {string} search - URL search query (e.g. '?tab=overview&page=2')
 * @returns {Record<string, string>}
 */
export function parseQuery(search) {
  const query = {};
  if (!search) return query;
  const cleanSearch = search.startsWith('?') ? search.slice(1) : search;
  const pairs = cleanSearch.split('&');

  for (const pair of pairs) {
    if (!pair) continue;
    const [key, val] = pair.split('=');
    query[decodeURIComponent(key)] = val ? decodeURIComponent(val) : '';
  }
  return query;
}
```

### 2.2 History & Location Manager (`src/history.js`)

```javascript
/**
 * Environment-agnostic history manager.
 * Works seamlessly with browser window.history or in-memory stack for tests.
 */
export class HistoryManager {
  constructor(options = {}) {
    this.isBrowser = typeof window !== 'undefined' && typeof window.history !== 'undefined';
    this.memoryStack = [options.initialPath || '/'];
    this.memoryIndex = 0;
    this.listeners = new Set();

    if (this.isBrowser) {
      this.popstateHandler = (e) => this.notify(window.location.pathname + window.location.search);
      window.addEventListener('popstate', this.popstateHandler);
    }
  }

  getCurrentLocation() {
    if (this.isBrowser) {
      return window.location.pathname + window.location.search;
    }
    return this.memoryStack[this.memoryIndex];
  }

  push(url) {
    if (this.isBrowser) {
      window.history.pushState({}, '', url);
      this.notify(url);
    } else {
      this.memoryStack = this.memoryStack.slice(0, this.memoryIndex + 1);
      this.memoryStack.push(url);
      this.memoryIndex++;
      this.notify(url);
    }
  }

  replace(url) {
    if (this.isBrowser) {
      window.history.replaceState({}, '', url);
      this.notify(url);
    } else {
      this.memoryStack[this.memoryIndex] = url;
      this.notify(url);
    }
  }

  listen(fn) {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  notify(url) {
    for (const fn of this.listeners) {
      fn(url);
    }
  }

  destroy() {
    if (this.isBrowser) {
      window.removeEventListener('popstate', this.popstateHandler);
    }
    this.listeners.clear();
  }
}
```

### 2.3 Router Controller & Navigation Guards (`src/router.js`)

```javascript
import { compileRoute, parseQuery } from './matcher.js';
import { HistoryManager } from './history.js';

export class Router {
  constructor(options = {}) {
    this.routes = [];
    this.beforeGuards = [];
    this.currentRoute = null;
    this.history = options.history || new HistoryManager(options);
    this.unlisten = this.history.listen((url) => this._resolve(url));
  }

  addRoute(path, handler, meta = {}) {
    const { regex, keys } = compileRoute(path);
    this.routes.push({ path, regex, keys, handler, meta });
    return this;
  }

  beforeEach(guardFn) {
    this.beforeGuards.push(guardFn);
    return this;
  }

  match(url) {
    const [pathname, search] = url.split('?');
    const normalizedPath = pathname.trim().replace(/\/+$/, '') || '/';
    const query = parseQuery(search);

    for (const route of this.routes) {
      const match = route.regex.exec(normalizedPath);
      if (match) {
        const params = {};
        route.keys.forEach((key, index) => {
          params[key] = decodeURIComponent(match[index + 1]);
        });
        return {
          path: normalizedPath,
          fullUrl: url,
          params,
          query,
          meta: route.meta,
          handler: route.handler
        };
      }
    }

    return null;
  }

  async navigate(url) {
    const targetRoute = this.match(url);
    if (!targetRoute) {
      throw new Error(`[Router] No matching route found for: ${url}`);
    }

    // Run beforeEach navigation guards sequentially
    for (const guard of this.beforeGuards) {
      const result = await guard(targetRoute, this.currentRoute);
      if (result === false) {
        // Abort navigation
        return false;
      }
      if (typeof result === 'string') {
        // Redirect
        return this.navigate(result);
      }
    }

    this.history.push(url);
    return true;
  }

  async _resolve(url) {
    const targetRoute = this.match(url);
    if (targetRoute) {
      this.currentRoute = targetRoute;
      await targetRoute.handler(targetRoute);
    }
  }

  start() {
    return this._resolve(this.history.getCurrentLocation());
  }

  stop() {
    if (this.unlisten) this.unlisten();
    this.history.destroy();
  }
}
```

### 2.4 Entry Point (`src/index.js`)

```javascript
import { Router } from './router.js';
import { HistoryManager } from './history.js';
import { compileRoute, parseQuery } from './matcher.js';

export { Router, HistoryManager, compileRoute, parseQuery };
```

---

## 3. Build & Runtime Configuration (`package.json`)

```json
{
  "name": "mini-router",
  "version": "1.0.0",
  "description": "Zero-dependency SPA client-side router with route matching and async guards",
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
import { Router, HistoryManager, parseQuery } from '../src/index.js';

describe('Mini Client-Side Router', () => {
  it('parses URL query strings accurately', () => {
    const query = parseQuery('?search=javascript&page=3&sort=desc');
    assert.deepEqual(query, {
      search: 'javascript',
      page: '3',
      sort: 'desc'
    });
  });

  it('matches static and parameterized routes', () => {
    const router = new Router();
    router.addRoute('/users/:userId/posts/:postId', () => {});

    const matched = router.match('/users/42/posts/108?format=json');
    assert.ok(matched);
    assert.equal(matched.params.userId, '42');
    assert.equal(matched.params.postId, '108');
    assert.equal(matched.query.format, 'json');
  });

  it('executes route handler upon navigation', async () => {
    const router = new Router();
    let invokedWith = null;

    router.addRoute('/dashboard', (context) => {
      invokedWith = context;
    });

    await router.navigate('/dashboard');
    assert.ok(invokedWith);
    assert.equal(invokedWith.path, '/dashboard');
  });

  it('enforces navigation guards and supports redirection', async () => {
    const router = new Router();
    let userLoggedIn = false;
    let loginRendered = false;
    let secretRendered = false;

    router.addRoute('/login', () => { loginRendered = true; });
    router.addRoute('/secret', () => { secretRendered = true; }, { requiresAuth: true });

    router.beforeEach((to) => {
      if (to.meta.requiresAuth && !userLoggedIn) {
        return '/login'; // Redirect to login
      }
      return true;
    });

    await router.navigate('/secret');
    assert.equal(secretRendered, false);
    assert.equal(loginRendered, true);

    // Now log in and navigate again
    userLoggedIn = true;
    await router.navigate('/secret');
    assert.equal(secretRendered, true);
  });

  it('allows guards to abort navigation completely by returning false', async () => {
    const router = new Router();
    let pageVisited = false;

    router.addRoute('/locked', () => { pageVisited = true; });
    router.beforeEach(() => false); // Block all transitions

    const result = await router.navigate('/locked');
    assert.equal(result, false);
    assert.equal(pageVisited, false);
  });
});
```

---

## 5. Verification & Sample Execution

```bash
$ node --test tests/app.test.js
✔ Mini Client-Side Router > parses URL query strings accurately (0.41ms)
✔ Mini Client-Side Router > matches static and parameterized routes (0.35ms)
✔ Mini Client-Side Router > executes route handler upon navigation (0.44ms)
✔ Mini Client-Side Router > enforces navigation guards and supports redirection (0.58ms)
✔ Mini Client-Side Router > allows guards to abort navigation completely by returning false (0.29ms)
▶ Mini Client-Side Router (3.11ms)
ℹ tests 5
ℹ suites 1
ℹ pass 5
ℹ fail 0
ℹ cancelled 0
ℹ skipped 0
ℹ todo 0
ℹ duration_ms 55.4
```
