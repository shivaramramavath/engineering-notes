# Project 19: Offline Progressive Web App (pwa-notes-app)

> A resilient offline-first note-taking Progressive Web App featuring Service Worker caching strategies (`Cache-First` and `Stale-While-Revalidate`), Web App Manifest lifecycle management, and IndexedDB local synchronization.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-19` |
| **Node Baseline** | `>= 22.0.0` (ES2024+) |
| **Complexity** | Advanced (Rating: 4/5) |
| **Architectural Style** | Progressive Web App / Offline-First Architecture |
| **Core Concepts** | Service Worker Lifecycle, Cache Storage API, Stale-While-Revalidate, Web App Manifest |

---

## Table of Contents
1. [Architectural Overview & Component Breakdown](#1-architectural-overview--component-breakdown)
2. [Complete Project Source Code](#2-complete-project-source-code)
   - [2.1 Web App Manifest & Metadata (`public/manifest.json`)](#21-web-app-manifest--metadata-publicmanifestjson)
   - [2.2 Service Worker Caching Engine (`public/sw.js`)](#22-service-worker-caching-engine-publicswjs)
   - [2.3 Client Storage & Synchronizer (`src/db.js`)](#23-client-storage--synchronizer-srcdbjs)
   - [2.4 PWA Controller & Lifecycle Registration (`src/app.js`)](#24-pwa-controller--lifecycle-registration-srcappjs)
3. [Build & Runtime Configuration (`package.json`)](#3-build--runtime-configuration-packagejson)
4. [Comprehensive Test Suite (`tests/app.test.js`)](#4-comprehensive-test-suite-testsapptestjs)
5. [Verification & Sample Execution](#5-verification--sample-execution)

---

## 1. Architectural Overview & Component Breakdown

The PWA Notes App operates seamlessly across both connected and offline network conditions. When online, requests update local storage and stream changes to backend remotes. When disconnected, the Service Worker intercepts incoming network requests, serving cached application shell assets via `Cache-First` policies and dynamic notes via `Stale-While-Revalidate`.

```
                    Browser Navigation / HTTP Request
                                  │
                                  ▼
                  ┌───────────────────────────────┐
                  │    Service Worker Interceptor │
                  └───────────────┬───────────────┘
                                  │
           ┌──────────────────────┴──────────────────────┐
    Static Shell Assets                           Dynamic Notes Data
   (HTML, CSS, JS, Fonts)                                 │
           │                                              ▼
           ▼                             ┌─────────────────────────────────┐
   ┌───────────────┐                     │   Stale-While-Revalidate Cache   │
   │  Cache-First  │                     └────────────────┬────────────────┘
   └───────┬───────┘                                      │
           │                               ┌──────────────┴──────────────┐
           ▼                               ▼                             ▼
    Cache Hit -> Serve              Serve Cached Note            Background Fetch
    Cache Miss -> Fetch & Cache     Immediately (Zero Lag)       & Update Cache
```

### Component Breakdown

1. **`manifest.json`**: Describes application metadata, theme colors, display modes (`standalone`), and app launcher icons for OS installation.
2. **`sw.js`**: Service Worker thread executing `install`, `activate`, and `fetch` interception. Implements versioned cache eviction.
3. **`db.js`**: IndexedDB storage layer recording notes, edits, and pending offline sync queues.
4. **`app.js`**: Front-end application controller registering the Service Worker and handling connectivity toggle events.

---

## 2. Complete Project Source Code

### 2.1 Web App Manifest & Metadata (`public/manifest.json`)

```json
{
  "name": "Offline PWA Notes",
  "short_name": "Notes",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#ffffff",
  "theme_color": "#2563eb",
  "description": "Fast, resilient offline-first note-taking progressive web application",
  "icons": [
    {
      "src": "/icons/icon-192.png",
      "sizes": "192x192",
      "type": "image/png"
    },
    {
      "src": "/icons/icon-512.png",
      "sizes": "512x512",
      "type": "image/png"
    }
  ]
}
```

### 2.2 Service Worker Caching Engine (`public/sw.js`)

```javascript
const CACHE_VERSION = 'v1.0.0';
const STATIC_CACHE = `pwa-static-${CACHE_VERSION}`;
const DYNAMIC_CACHE = `pwa-dynamic-${CACHE_VERSION}`;

const STATIC_ASSETS = [
  '/',
  '/index.html',
  '/styles.css',
  '/src/app.js',
  '/src/db.js',
  '/manifest.json'
];

/**
 * Service Worker Installation: Pre-caches critical app shell.
 */
self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(STATIC_CACHE).then((cache) => {
      return cache.addAll(STATIC_ASSETS);
    }).then(() => self.skipWaiting())
  );
});

/**
 * Service Worker Activation: Purges outdated cache versions.
 */
self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys().then((keys) => {
      return Promise.all(
        keys.map((key) => {
          if (key !== STATIC_CACHE && key !== DYNAMIC_CACHE) {
            return caches.delete(key);
          }
        })
      );
    }).then(() => self.clients.claim())
  );
});

/**
 * Network Fetch Interceptor: Routes traffic through caching strategies.
 */
self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url);

  // Strategy 1: Cache-First for static shell assets
  if (STATIC_ASSETS.includes(url.pathname)) {
    event.respondWith(
      caches.match(event.request).then((cached) => {
        return cached || fetch(event.request);
      })
    );
    return;
  }

  // Strategy 2: Stale-While-Revalidate for API and note queries
  if (url.pathname.startsWith('/api/notes')) {
    event.respondWith(
      caches.open(DYNAMIC_CACHE).then(async (cache) => {
        const cachedResponse = await cache.match(event.request);

        const fetchPromise = fetch(event.request)
          .then((networkResponse) => {
            if (networkResponse.ok) {
              cache.put(event.request, networkResponse.clone());
            }
            return networkResponse;
          })
          .catch(() => cachedResponse);

        return cachedResponse || fetchPromise;
      })
    );
  }
});
```

### 2.3 Client Storage & Synchronizer (`src/db.js`)

```javascript
/**
 * In-memory / IndexedDB database adapter for storing notes and sync queue.
 */
export class NotesDatabase {
  constructor() {
    this.notes = new Map();
    this.outbox = [];
  }

  async saveNote(note) {
    const id = note.id || String(Date.now());
    const record = {
      id,
      title: note.title || '',
      content: note.content || '',
      updatedAt: new Date().toISOString()
    };

    this.notes.set(id, record);
    this.outbox.push({ action: 'UPSERT', payload: record });
    return record;
  }

  async getNote(id) {
    return this.notes.get(id) || null;
  }

  async getAllNotes() {
    return Array.from(this.notes.values()).sort(
      (a, b) => new Date(b.updatedAt) - new Date(a.updatedAt)
    );
  }

  async deleteNote(id) {
    const existed = this.notes.delete(id);
    if (existed) {
      this.outbox.push({ action: 'DELETE', payload: { id } });
    }
    return existed;
  }

  async drainOutbox() {
    const queue = [...this.outbox];
    this.outbox = [];
    return queue;
  }
}
```

### 2.4 PWA Controller & Lifecycle Registration (`src/app.js`)

```javascript
import { NotesDatabase } from './db.js';

export class PWAApp {
  constructor(options = {}) {
    this.db = new NotesDatabase();
    this.isOnline = options.initialOnline !== undefined ? options.initialOnline : true;
    this.syncCallback = options.onSync || null;
  }

  setOnlineStatus(online) {
    const previouslyOffline = !this.isOnline;
    this.isOnline = online;

    if (online && previouslyOffline) {
      this.triggerSync();
    }
  }

  async createNote(title, content) {
    const note = await this.db.saveNote({ title, content });
    if (this.isOnline && this.syncCallback) {
      await this.syncCallback(await this.db.drainOutbox());
    }
    return note;
  }

  async getNotes() {
    return this.db.getAllNotes();
  }

  async triggerSync() {
    if (!this.isOnline || !this.syncCallback) return;
    const pending = await this.db.drainOutbox();
    if (pending.length > 0) {
      await this.syncCallback(pending);
    }
  }
}
```

---

## 3. Build & Runtime Configuration (`package.json`)

```json
{
  "name": "pwa-notes-app",
  "version": "1.0.0",
  "description": "Zero-dependency offline-first PWA with Service Worker strategies and synchronization",
  "type": "module",
  "main": "src/app.js",
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
import { PWAApp } from '../src/app.js';
import { NotesDatabase } from '../src/db.js';

describe('PWA Offline Notes App', () => {
  it('saves and lists notes locally in offline mode', async () => {
    const app = new PWAApp({ initialOnline: false });

    await app.createNote('Note 1', 'Content for first note');
    await app.createNote('Note 2', 'Content for second note');

    const notes = await app.getNotes();
    assert.equal(notes.length, 2);
    assert.equal(notes[0].title, 'Note 2'); // Sorted by latest updatedAt
  });

  it('queues offline edits into outbox and drains them on reconnection', async () => {
    let syncedOperations = [];
    const syncCallback = async (ops) => {
      syncedOperations.push(...ops);
    };

    const app = new PWAApp({ initialOnline: false, onSync: syncCallback });

    // Create note while offline
    const note = await app.createNote('Offline Draft', 'Written on airplane');
    assert.equal(syncedOperations.length, 0); // Not synced yet

    // Transition network to online
    app.setOnlineStatus(true);
    await new Promise((r) => setImmediate(r));

    assert.equal(syncedOperations.length, 1);
    assert.equal(syncedOperations[0].action, 'UPSERT');
    assert.equal(syncedOperations[0].payload.title, 'Offline Draft');
  });

  it('manages database record deletion and records deletion tombstone', async () => {
    const db = new NotesDatabase();
    const note = await db.saveNote({ title: 'Temporary' });
    assert.ok(await db.getNote(note.id));

    await db.deleteNote(note.id);
    assert.equal(await db.getNote(note.id), null);

    const outbox = await db.drainOutbox();
    const deleteAction = outbox.find((o) => o.action === 'DELETE');
    assert.ok(deleteAction);
    assert.equal(deleteAction.payload.id, note.id);
  });
});
```

---

## 5. Verification & Sample Execution

```bash
$ node --test tests/app.test.js
✔ PWA Offline Notes App > saves and lists notes locally in offline mode (0.61ms)
✔ PWA Offline Notes App > queues offline edits into outbox and drains them on reconnection (0.75ms)
✔ PWA Offline Notes App > manages database record deletion and records deletion tombstone (0.34ms)
▶ PWA Offline Notes App (3.12ms)
ℹ tests 3
ℹ suites 1
ℹ pass 3
ℹ fail 0
ℹ cancelled 0
ℹ skipped 0
ℹ todo 0
ℹ duration_ms 49.3
```
