# Browser Storage & Offline State

> Modern browser storage spans synchronous key-value stores (`localStorage`, `sessionStorage`), HTTP security cookies, transactional binary databases (`IndexedDB`), the Storage Quota API, and cross-tab communication channels.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [The DOM](./01_dom.md), [Promises](../11_asynchronous-javascript/03_promises.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | WHATWG Web Storage / W3C IndexedDB 3.0 / Storage Standard / Baseline Widely Available |

---

## Table of Contents

1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Client-side data persistence is essential for modern web applications. Users expect offline capability, fast startup via client caching, persistent authentication sessions, and instant state restoration across tab reloads.

Historically, browsers only supported **HTTP Cookies**, which were transmitted across every HTTP request header, severely limited to 4KB of plain text, and vulnerable to cross-site scripting (XSS) and CSRF attacks.

Today, the web platform provides a tiered storage architecture:
1. **Web Storage (`localStorage`, `sessionStorage`)**: Simple, synchronous string key-value storage.
2. **Cookies**: Server-synchronized tokens with cryptographic security flags (`HttpOnly`, `SameSite`, `Secure`).
3. **`IndexedDB`**: A full transactional, asynchronous, indexed NoSQL object database capable of storing gigabytes of structured data, blobs, and typed arrays.
4. **`BroadcastChannel` & Storage Events**: Real-time cross-tab synchronization primitives.

---

## 2. Core Concepts

### Storage Mechanism Comparison Matrix

| Technology | Capacity | Synchronous / Async | Persistence Scope | Accessible via JS? | Transmitted to Server? |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Cookies** | $\approx 4\text{KB}$ per domain | Synchronous | Expiration date or session | Yes (unless `HttpOnly`) | Yes (Every HTTP request) |
| **`sessionStorage`**| $\approx 5\text{MB}$ | Synchronous | Single browser tab lifecycle | Yes | No |
| **`localStorage`** | $\approx 5\text{MB} - 10\text{MB}$ | Synchronous | Persistent across tabs & restarts | Yes | No |
| **`IndexedDB`** | Gigabytes / % of Disk | Asynchronous | Persistent across tabs & restarts | Yes | No |

```text
+--------------------------------------------------------------------------+
|                        Browser Storage Hierarchy                         |
|                                                                          |
|  Small Strings (<5MB)               Structured Data & Large Blobs (>50MB)|
|  +---------------------------+      +---------------------------------+  |
|  | localStorage              |      | IndexedDB                       |  |
|  | sessionStorage            |      | Object Stores, Indexes, Blobs   |  |
|  +---------------------------+      +---------------------------------+  |
|               ▲                                      ▲                   |
|               │                                      │                   |
|  Synchronous / Blocks Thread           Asynchronous / Non-blocking I/O   |
+--------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### IndexedDB Transaction State Machine
`IndexedDB` is strictly transactional. Operations must occur within an explicit transaction lifecycle:

```
 [ Open Database: indexedDB.open('AppDB', 1) ]
                        │
                        ▼
           Does version need upgrade?
                 /             \
           YES  /               \ NO
               v                 v
 [ onupgradeneeded ]     [ onsuccess: Database Ready ]
 (Create Object Stores,          │
  Define Indexes)                v
          │              [ Start Transaction ]
          │              const tx = db.transaction('users', 'readwrite');
          │                      │
          │                      ▼
          │              [ Execute ObjectStore Operations ]
          │              tx.objectStore('users').put({ id: 1, name: 'Ada' });
          │                      │
          │                      ▼
          └────────────> [ Transaction Auto-Commits ]
                         (When microtask / event loop cycle ends)
```

---

## 4. Syntax & API Reference

### 1. `localStorage` & `sessionStorage`

```javascript
// Storage API methods
localStorage.setItem('theme', 'dark');
const theme = localStorage.getItem('theme'); // 'dark' (or null)
localStorage.removeItem('theme');
localStorage.clear();

// Cross-tab synchronization event
window.addEventListener('storage', (event) => {
  console.log(`Key ${event.key} changed from ${event.oldValue} to ${event.newValue}`);
  console.log(`Triggered by window origin: ${event.url}`);
});
```

### 2. HTTP Cookies

```javascript
// Write cookie with modern security attributes
document.cookie = 'user_pref=contrast; max-age=86400; path=/; SameSite=Lax; Secure';

// Read cookies (requires string parsing)
function getCookie(name) {
  const matches = document.cookie.match(new RegExp('(?:^|; )' + name.replace(/([\.$?*|{}\(\)\[\]\\\/\+^])/g, '\\$1') + '=([^;]*)'));
  return matches ? decodeURIComponent(matches[1]) : null;
}
```

### 3. IndexedDB API

```javascript
// 1. Open and Upgrade
const request = indexedDB.open('CommerceDB', 1);

request.onupgradeneeded = (event) => {
  const db = event.target.result;
  // Create object store with keyPath
  const store = db.createObjectStore('products', { keyPath: 'id' });
  // Create index for fast secondary querying
  store.createIndex('by_category', 'category', { unique: false });
};

// 2. Querying inside a Transaction
request.onsuccess = (event) => {
  const db = event.target.result;
  const tx = db.transaction('products', 'readonly');
  const store = tx.objectStore('products');
  const query = store.get('prod_101');

  query.onsuccess = () => console.log('Product:', query.result);
};
```

---

## 5. Practical Examples

### Example 1: Type-Safe JSON `localStorage` Wrapper with Expiration (TTL)
```javascript
class PersistentStorage {
  static set(key, value, ttlSeconds = null) {
    const record = {
      value,
      expiresAt: ttlSeconds ? Date.now() + ttlSeconds * 1000 : null,
    };
    try {
      localStorage.setItem(key, JSON.stringify(record));
    } catch (e) {
      if (e.name === 'QuotaExceededError') {
        console.error('LocalStorage quota exceeded! Evicting cache...');
        localStorage.clear();
      }
    }
  }

  static get(key) {
    const raw = localStorage.getItem(key);
    if (!raw) return null;

    try {
      const record = JSON.parse(raw);
      if (record.expiresAt && Date.now() > record.expiresAt) {
        localStorage.removeItem(key); // Evict expired
        return null;
      }
      return record.value;
    } catch {
      return null;
    }
  }
}
```

### Example 2: Promise-Wrapped IndexedDB Key-Value Store
```javascript
class MiniIndexedDB {
  #dbPromise;

  constructor(dbName = 'KeyValStore', storeName = 'keyval') {
    this.#dbPromise = new Promise((resolve, reject) => {
      const req = indexedDB.open(dbName, 1);
      req.onupgradeneeded = () => req.result.createObjectStore(storeName);
      req.onsuccess = () => resolve(req.result);
      req.onerror = () => reject(req.error);
    });
  }

  async set(key, value) {
    const db = await this.#dbPromise;
    return new Promise((resolve, reject) => {
      const tx = db.transaction('keyval', 'readwrite');
      tx.objectStore('keyval').put(value, key);
      tx.oncomplete = () => resolve();
      tx.onerror = () => reject(tx.error);
    });
  }

  async get(key) {
    const db = await this.#dbPromise;
    return new Promise((resolve, reject) => {
      const tx = db.transaction('keyval', 'readonly');
      const req = tx.objectStore('keyval').get(key);
      req.onsuccess = () => resolve(req.result);
      req.onerror = () => reject(req.error);
    });
  }
}
```

### Example 3: Cross-Tab State Sync via `BroadcastChannel`
```javascript
// Instant cross-tab messaging without localStorage storage event latency
const authChannel = new BroadcastChannel('auth_sync');

// When user logs out in Tab A:
function logoutUser() {
  clearSession();
  authChannel.postMessage({ type: 'USER_LOGOUT' });
  window.location.href = '/login';
}

// In Tab B, Tab C:
authChannel.onmessage = (event) => {
  if (event.data.type === 'USER_LOGOUT') {
    console.log('Received logout signal from another tab. Logging out...');
    window.location.href = '/login';
  }
};
```

### Example 4: Storage Quota Management with `navigator.storage`
```javascript
async function checkStorageCapacity() {
  if (navigator.storage && navigator.storage.estimate) {
    const { quota, usage } = await navigator.storage.estimate();

    const usageMB = (usage / (1024 * 1024)).toFixed(2);
    const quotaMB = (quota / (1024 * 1024)).toFixed(2);
    const percent = ((usage / quota) * 100).toFixed(1);

    console.log(`Storage: ${usageMB}MB of ${quotaMB}MB used (${percent}%)`);

    // Request persistent storage (prevents automatic browser eviction on low disk)
    if (navigator.storage.persist) {
      const isPersisted = await navigator.storage.persist();
      console.log(`Persistent storage granted: ${isPersisted}`);
    }
  }
}
```

### Example 5: Streaming Large Blobs into IndexedDB
```javascript
async function cacheAudioBlob(audioUrl, trackId) {
  const response = await fetch(audioUrl);
  const blob = await response.blob(); // Binary audio data

  const db = await openAudioDatabase();
  const tx = db.transaction('tracks', 'readwrite');
  tx.objectStore('tracks').put({ id: trackId, data: blob, downloadedAt: Date.now() });

  return new Promise((resolve, reject) => {
    tx.oncomplete = () => resolve();
    tx.onerror = () => reject(tx.error);
  });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`localStorage` Is Synchronous and Blocking**: `localStorage.getItem()` and `.setItem()` execute synchronously on the main thread using synchronous disk I/O. Reading or writing large JSON structures blocks the event loop, dropping frames and delaying user interaction.
2. **`QuotaExceededError`**: Web Storage has a hard limit of $\approx 5\text{MB}$ per origin. When full, `.setItem()` throws a `DOMException: QuotaExceededError`.
3. **`storage` Event Fires ONLY in Other Tabs**: The `window.addEventListener('storage')` event **never fires in the tab that made the mutation**! It only fires in *other* open tabs belonging to the same origin.
4. **Safari Private Browsing Quotas**: In older Safari versions, private browsing set `localStorage` quota to 0 bytes, throwing errors on every write. Modern Safari partitions storage per tab session in private mode.
5. **IndexedDB Auto-Commit**: An IndexedDB transaction automatically commits as soon as the microtask queue empties without any pending requests. You cannot await a non-database Promise (like `fetch()`) inside an IndexedDB transaction, or the transaction will prematurely commit!

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Storing Sensitive Tokens in `localStorage`**| Storing JWT auth tokens in `localStorage` | Use `HttpOnly; Secure; SameSite` cookies | Vulnerable to XSS: any injected script can read `localStorage`. |
| **Awaiting Fetch in IndexedDB Transaction**| `const tx = ...; await fetch(); tx.put()` | Fetch data *before* opening transaction | IndexedDB transactions auto-commit when waiting on non-IDB promises. |
| **Giant JSON in `localStorage`** | Storing 4MB JSON payloads in `localStorage` | Use `IndexedDB` for structured data | Synchronous read/write freezes UI thread. |
| **Assuming `storage` Event Runs Locally**| Expecting current window to receive its own event| Use `BroadcastChannel` or custom events | The HTML spec restricts `storage` events to external tabs. |
| **Unversioned IndexedDB Schemas** | Changing schema without incrementing version | Increment DB version in `indexedDB.open(name, v)` | `onupgradeneeded` only triggers on version increments. |
| **Missing `SameSite` on Cookies** | Writing cookies without `SameSite` attribute | Set `SameSite=Lax` or `Strict` | Leaves application vulnerable to Cross-Site Request Forgery (CSRF). |

---

## 8. Performance Characteristics

- **Read/Write Latency**:
  - `localStorage`: $1\text{ms} - 15\text{ms}$ (synchronous, blocks main thread).
  - `IndexedDB`: $5\text{ms} - 30\text{ms}$ (asynchronous, executed on background LevelDB/SQLite thread).
- **Disk Footprint**: IndexedDB scales to hundreds of gigabytes based on available disk space (typically 60% of free disk in Chromium).

---

## 9. Engine Internals

1. **Chromium LevelDB Engine**: In Chromium, both `localStorage` and `IndexedDB` are backed by **LevelDB** (Google's high-performance key-value storage library written in C++).
2. **Storage Partitioning**: Modern browsers enforce **Third-Party Storage Partitioning**: cookies and storage accessed inside an `<iframe>` are keyed by both the iframe origin *and* the top-level origin, preventing cross-site tracking.

---

## 10. Best Practices & Design Guidelines

1. **Use `HttpOnly` Cookies for Authentication**: Never store sensitive session tokens or passwords in `localStorage`.
2. **Use `IndexedDB` for Offline Caching**: Store application state, offline mutations, and binary media in `IndexedDB`.
3. **Adopt Libraries like `idb`**: Use standard Promise-wrapper libraries (like Jake Archibald's `idb`) to avoid raw callback boilerplate in IndexedDB.
4. **Use `BroadcastChannel` for Cross-Tab Sync**: Synchronize state across multiple open tabs with modern `BroadcastChannel` instead of relying on storage events.

---

## 11. Real-World Case Study

### Offline-First Note-Taking Application with Cloud Sync

A progressive web app (PWA) allows journalists to draft articles offline during flights. Edits must be stored immediately, queued for upload, and synced across open browser tabs.

```javascript
class OfflineNoteManager {
  #db;
  #channel = new BroadcastChannel('notes_sync');

  async init() {
    this.#db = await new Promise((resolve, reject) => {
      const req = indexedDB.open('NotesDatabase', 1);
      req.onupgradeneeded = () => {
        const store = req.result.createObjectStore('notes', { keyPath: 'id' });
        store.createIndex('synced', 'synced', { unique: false });
      };
      req.onsuccess = () => resolve(req.result);
      req.onerror = () => reject(req.error);
    });

    this.#bindSyncChannel();
    this.#checkOnlineAndSync();
  }

  async saveNote(note) {
    note.updatedAt = Date.now();
    note.synced = 0; // 0 = false, 1 = true (IDB indexable)

    const tx = this.#db.transaction('notes', 'readwrite');
    tx.objectStore('notes').put(note);

    await new Promise((resolve, reject) => {
      tx.oncomplete = resolve;
      tx.onerror = reject;
    });

    // Notify other open tabs to re-render note
    this.#channel.postMessage({ type: 'NOTE_SAVED', noteId: note.id });

    if (navigator.onLine) {
      this.#syncNoteToCloud(note);
    }
  }

  async #syncNoteToCloud(note) {
    try {
      const res = await fetch('/api/notes', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(note),
      });

      if (res.ok) {
        const tx = this.#db.transaction('notes', 'readwrite');
        note.synced = 1;
        tx.objectStore('notes').put(note);
      }
    } catch {
      console.warn('Network offline. Note queued for next sync.');
    }
  }

  #bindSyncChannel() {
    this.#channel.onmessage = (event) => {
      if (event.data.type === 'NOTE_SAVED') {
        console.log(`Tab updated note: ${event.data.noteId}. Refreshing UI.`);
        this.renderNotesList();
      }
    };

    window.addEventListener('online', () => this.#checkOnlineAndSync());
  }

  async #checkOnlineAndSync() {
    if (!navigator.onLine) return;

    const tx = this.#db.transaction('notes', 'readonly');
    const index = tx.objectStore('notes').index('synced');
    const pendingNotesReq = index.getAll(0);

    pendingNotesReq.onsuccess = () => {
      for (const note of pendingNotesReq.result) {
        this.#syncNoteToCloud(note);
      }
    };
  }

  renderNotesList() { /* ... updates DOM ... */ }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. Why is storing authentication JWTs in <code>localStorage</code> considered a security vulnerability?</strong></summary>

Any JavaScript executing on the page (including third-party scripts, compromised npm packages, or injected XSS attacks) can read `localStorage.getItem('token')` and exfiltrate it. Storing tokens in `HttpOnly; Secure; SameSite` cookies prevents JavaScript from ever reading the token, mitigating XSS exfiltration risks.
</details>

<details>
<summary><strong>2. Why can't you await a <code>fetch()</code> request inside an IndexedDB transaction?</strong></summary>

IndexedDB transactions automatically commit as soon as the microtask queue empties and no active database requests remain. Because network `fetch()` operations take hundreds of milliseconds to settle, the transaction auto-commits long before the fetch returns, causing subsequent database operations on that transaction to throw an `InactiveTransactionError`.
</details>

<details>
<summary><strong>3. How does the <code>window.addEventListener('storage')</code> event behave across different browser tabs?</strong></summary>

The `storage` event fires exclusively in *other* open tabs belonging to the same origin. It never fires in the tab that actually performed the `localStorage.setItem()` or `removeItem()` call.
</details>

<details>
<summary><strong>4. What does the <code>SameSite</code> cookie attribute do, and what are its three modes?</strong></summary>

`SameSite` controls whether cookies are sent with cross-site requests, protecting against CSRF attacks:
- **`Strict`**: Cookie is never sent in cross-site requests (even when clicking a link to the site).
- **`Lax`**: Cookie is sent on top-level GET navigations (e.g. following a link), but withheld on cross-site POST requests and image/iframe loads (modern default).
- **`None`**: Cookie is sent on all cross-site requests (requires `Secure` attribute).
</details>

<details>
<summary><strong>5. What is the difference between <code>localStorage</code> and <code>sessionStorage</code>?</strong></summary>

`localStorage` persists indefinitely across browser restarts and is shared across all windows and tabs of the same origin. `sessionStorage` is scoped to a single browser tab session; opening the same URL in a new tab creates an isolated `sessionStorage` instance, and data is deleted when the tab is closed.
</details>

<details>
<summary><strong>6. What is the Storage Quota API?</strong></summary>

`navigator.storage.estimate()` returns the total available storage quota and current disk usage for the origin. `navigator.storage.persist()` requests permission from the browser to mark storage as persistent, preventing automatic eviction when system disk space is low.
</details>

<details>
<summary><strong>7. How does <code>BroadcastChannel</code> differ from the <code>storage</code> event for cross-tab communication?</strong></summary>

`BroadcastChannel` is a dedicated messaging primitive designed specifically for cross-tab communication. It transfers arbitrary JavaScript structured objects directly without needing to serialize them to strings or write them to disk, and operates with lower latency.
</details>

<details>
<summary><strong>8. In IndexedDB, when does <code>onupgradeneeded</code> fire?</strong></summary>

It fires when the database is opened with a version number higher than the existing database version on the client, or when the database is created for the first time. Schema modifications (like creating or deleting object stores and indexes) are only permitted inside this event handler.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Type-Safe Session Storage State Hook
Implement a state management wrapper `createSessionState(key, defaultValue)` that reads from `sessionStorage`, validates parsed data, and synchronizes state modifications to `sessionStorage` automatically.

<details>
<summary>View Solution</summary>

```javascript
function createSessionState(key, defaultValue) {
  let currentState = (() => {
    const raw = sessionStorage.getItem(key);
    if (!raw) return defaultValue;
    try {
      return JSON.parse(raw);
    } catch {
      return defaultValue;
    }
  })();

  return {
    get() {
      return currentState;
    },
    set(newState) {
      currentState = newState;
      try {
        sessionStorage.setItem(key, JSON.stringify(newState));
      } catch (e) {
        console.error('SessionStorage set failed:', e);
      }
    },
  };
}
```
</details>

### Challenge 2: Implement a Cookie Parser and Serializer
Write a utility object `CookieJar` with `get(name)`, `set(name, value, options)`, and `delete(name)` supporting `path`, `maxAge`, `sameSite`, and `secure` options.

<details>
<summary>View Solution</summary>

```javascript
const CookieJar = {
  get(name) {
    const cookies = document.cookie ? document.cookie.split('; ') : [];
    for (const cookie of cookies) {
      const [key, ...valParts] = cookie.split('=');
      if (decodeURIComponent(key) === name) {
        return decodeURIComponent(valParts.join('='));
      }
    }
    return null;
  },

  set(name, value, options = {}) {
    let cookieStr = `${encodeURIComponent(name)}=${encodeURIComponent(value)}`;

    if (options.maxAge) cookieStr += `; max-age=${options.maxAge}`;
    if (options.path) cookieStr += `; path=${options.path}`;
    else cookieStr += '; path=/';

    if (options.sameSite) cookieStr += `; SameSite=${options.sameSite}`;
    else cookieStr += '; SameSite=Lax';

    if (options.secure) cookieStr += '; Secure';

    document.cookie = cookieStr;
  },

  delete(name, path = '/') {
    document.cookie = `${encodeURIComponent(name)}=; max-age=0; path=${path}`;
  },
};
```
</details>
