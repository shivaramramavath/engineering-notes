# Browser Storage

Browsers offer several places to keep data on the client. They differ in **size, lifetime, API style, and who can read them** (JavaScript only, or also the server).

| Storage | Capacity (typical) | Lifetime | API | Sent to server? | Best for |
|---------|-------------------|----------|-----|-----------------|----------|
| `localStorage` | ~5 MB per origin | until cleared | sync, strings | no | small preferences |
| `sessionStorage` | ~5 MB per origin | per tab session | sync, strings | no | per-tab state, wizards |
| Cookies | ~4 KB each, ~50 per domain | session or `Expires`/`Max-Age` | string API | **yes, on every matching request** | server sessions, auth |
| IndexedDB | large (percent of disk, GBs possible) | until cleared | async, structured data | no | offline data, caches, files |
| Cache API | large | until cleared | async, Request/Response | no | service worker caches |
| Origin Private File System (OPFS) | large | until cleared | async file handles | no | files, databases (SQLite WASM) |
| In-memory variables | RAM | until page unload | n/a | no | transient state |

All storage is **per origin** (scheme + host + port). Third-party iframes get **partitioned** storage in modern browsers.

## localStorage and sessionStorage

```js
localStorage.setItem("theme", "dark");
localStorage.getItem("theme");          // "dark" (or null if missing)
localStorage.removeItem("theme");
localStorage.clear();
localStorage.length;
localStorage.key(0);

localStorage.theme = "dark";            // property syntax works, but prefer methods (avoids key clashes like "length")
```

- Values are **strings only**: numbers and booleans are converted
- Operations are **synchronous** and block the main thread: keep data small
- `sessionStorage` has the same API but is scoped to one **tab** and cleared when it closes (survives reloads)

### Storing objects

```js
const save = (key, value) => localStorage.setItem(key, JSON.stringify(value));
const load = (key, fallback = null) => {
  try {
    const raw = localStorage.getItem(key);
    return raw === null ? fallback : JSON.parse(raw);
  } catch {
    return fallback;                     // corrupted or unavailable
  }
};

save("prefs", { theme: "dark", fontSize: 16 });
load("prefs", {});
```

`JSON` loses `Date`, `Map`, `Set`, `undefined`, functions, `BigInt`: convert them yourself or use IndexedDB.

### Quota and availability errors

```js
try {
  localStorage.setItem("big", hugeString);
} catch (err) {
  if (err.name === "QuotaExceededError") cleanUp();
}
```

Storage can be **unavailable** (blocked cookies, some private modes, sandboxed iframes: `SecurityError`). Wrap access in `try/catch` and fall back to memory.

### The `storage` event (cross-tab sync)

```js
window.addEventListener("storage", (e) => {
  // fires in OTHER tabs/windows of the same origin, not in the tab that made the change
  console.log(e.key, e.oldValue, e.newValue, e.storageArea === localStorage);
  if (e.key === "theme") applyTheme(e.newValue);
});
```

For richer messaging use `BroadcastChannel`:

```js
const channel = new BroadcastChannel("app");
channel.postMessage({ type: "logout" });
channel.onmessage = (e) => { if (e.data.type === "logout") signOut(); };
```

### Rules of thumb

| Do | Do not |
|----|--------|
| Store non-sensitive preferences and small caches | Store passwords, auth tokens, personal data |
| Namespace and version keys (`app:v2:prefs`) | Assume keys are unique across apps on the same origin |
| Validate what you read (it can be edited by users/extensions) | Trust stored data |
| Handle quota/unavailable errors | Store big blobs (use IndexedDB) |

**Security:** anything in web storage is readable by any script on your origin, so an **XSS** vulnerability exposes it. Do not keep long-lived tokens there; prefer `HttpOnly` cookies.

## Cookies

Cookies are small key/value strings that the **browser automatically sends** with matching requests. Servers set them with the `Set-Cookie` header; JavaScript can access non-`HttpOnly` ones via `document.cookie`.

```js
document.cookie = "theme=dark; Max-Age=31536000; Path=/; SameSite=Lax; Secure";

document.cookie;                         // "theme=dark; lang=en"  (name=value pairs only, no attributes)

const getCookie = (name) =>
  document.cookie.split("; ").find((row) => row.startsWith(`${name}=`))?.split("=")[1];

document.cookie = "theme=; Max-Age=0; Path=/";   // delete (must match Path/Domain)
```

Encode values with `encodeURIComponent` when they may contain `;`, `,` or spaces.

### Attributes

| Attribute | Meaning |
|-----------|---------|
| `Expires=<date>` / `Max-Age=<seconds>` | persistent; without them it is a **session cookie** |
| `Path=/` | URL path scope |
| `Domain=example.com` | also send to subdomains (omit to restrict to the exact host) |
| `Secure` | only over HTTPS |
| `HttpOnly` | **not readable by JavaScript** (set by the server): protects against XSS theft |
| `SameSite=Strict` / `Lax` / `None` | when cookies are sent on cross-site requests (`None` requires `Secure`); `Lax` is the default in modern browsers |
| `Partitioned` | CHIPS: per-top-level-site storage for third-party cookies |
| Prefixes `__Secure-`, `__Host-` | enforce `Secure` (and `Path=/`, no `Domain`) |

### Typical auth cookie

```http
Set-Cookie: __Host-session=abc123; Path=/; Secure; HttpOnly; SameSite=Lax; Max-Age=86400
```

### Cookie vs web storage

| | Cookie | localStorage |
|---|--------|--------------|
| Sent automatically with requests | yes (overhead!) | no |
| Readable by server | yes | no |
| Can be `HttpOnly` | yes | no |
| Size | ~4 KB | ~5 MB |
| Expiry | built-in | manual |

Cookies on every request add bandwidth and latency: keep them tiny, and serve static assets from a cookieless domain/path where relevant.

### Cookie Store API (async, newer)

```js
await cookieStore.set({ name: "theme", value: "dark", path: "/", sameSite: "lax" });
const cookie = await cookieStore.get("theme");
cookieStore.addEventListener("change", (e) => console.log(e.changed, e.deleted));
```

Check browser support before relying on it.

### Privacy and law

Cookies used for tracking typically require **consent** (GDPR/ePrivacy and similar). Third-party cookies are restricted or blocked in many browsers.

## IndexedDB

A transactional **NoSQL database** in the browser: stores structured-cloneable values (objects, arrays, `Date`, `Map`, `Set`, `Blob`, `File`, `ArrayBuffer`), supports indexes, key ranges and large datasets. The raw API is event-based and verbose.

```js
const request = indexedDB.open("app-db", 1);                // name, version

request.onupgradeneeded = (event) => {                      // create/upgrade schema
  const db = event.target.result;
  const store = db.createObjectStore("todos", { keyPath: "id", autoIncrement: true });
  store.createIndex("byDone", "done");
  store.createIndex("byTitle", "title", { unique: false });
};

request.onsuccess = (event) => {
  const db = event.target.result;
  const tx = db.transaction("todos", "readwrite");
  const store = tx.objectStore("todos");

  store.add({ title: "Write docs", done: false });
  store.put({ id: 1, title: "Updated", done: true });       // insert or replace
  store.get(1).onsuccess = (e) => console.log(e.target.result);
  store.delete(2);

  tx.oncomplete = () => console.log("committed");
  tx.onerror = () => console.error(tx.error);
};
request.onerror = () => console.error(request.error);
request.onblocked = () => console.warn("close other tabs using the old version");
```

### Concepts

| Concept | Meaning |
|---------|---------|
| **Database** | named, versioned container |
| **Object store** | like a table; records keyed by a key path or auto-increment |
| **Index** | secondary lookup on a property |
| **Transaction** | `readonly` / `readwrite`; auto-commits when no pending requests remain |
| **Cursor** | iterate records |
| **Key range** | `IDBKeyRange.bound(a, b)`, `lowerBound`, `upperBound`, `only` |
| **Version** | schema changes happen only in `onupgradeneeded` |

### Use a promise wrapper

The `idb` library (≈1 KB) wraps IndexedDB with promises:

```js
import { openDB } from "idb";

const db = await openDB("app-db", 1, {
  upgrade(db) {
    const store = db.createObjectStore("todos", { keyPath: "id", autoIncrement: true });
    store.createIndex("byDone", "done");
  },
});

await db.add("todos", { title: "Write docs", done: false });
await db.put("todos", { id: 1, title: "Updated", done: true });
const one = await db.get("todos", 1);
const all = await db.getAll("todos");
const done = await db.getAllFromIndex("todos", "byDone", true);
await db.delete("todos", 1);

const tx = db.transaction("todos", "readwrite");        // multiple operations atomically
await Promise.all([tx.store.add({ title: "A" }), tx.store.add({ title: "B" }), tx.done]);
```

Other options: **Dexie.js** (richer query API), **localForage** (localStorage-like API on IndexedDB).

### Transaction pitfalls

- Do not `await` unrelated async work (like `fetch`) **inside** a transaction: it auto-commits when the event loop has no pending requests, and later operations throw `TransactionInactiveError`
- Keep transactions short
- Handle `blocked` and `versionchange` events so tabs can upgrade cleanly:

```js
db.onversionchange = () => { db.close(); alert("App updated: please reload"); };
```

### When to use IndexedDB

Offline-first apps, large datasets, cached API responses, drafts, files/Blobs, PWAs. Use it from **workers** too (it is available there, unlike `localStorage`).

## Cache API (with service workers)

Stores `Request` → `Response` pairs, ideal for offline assets and API caching.

```js
const cache = await caches.open("v1");
await cache.addAll(["/", "/app.css", "/app.js"]);
const hit = await caches.match(request);
await cache.put(request, response.clone());
await caches.delete("v0");
```

Usually used inside a service worker's `fetch` handler (cache-first, network-first, stale-while-revalidate strategies). Libraries: **Workbox**.

## Origin Private File System (OPFS)

A sandboxed, fast file system per origin.

```js
const root = await navigator.storage.getDirectory();
const handle = await root.getFileHandle("data.bin", { create: true });
const writable = await handle.createWritable();
await writable.write(new Uint8Array([1, 2, 3]));
await writable.close();
const file = await handle.getFile();
```

Used by SQLite-in-WASM and heavy file workloads. (The separate File System Access API with user-visible files is Chromium-centric: check support.)

## Storage quotas and persistence

```js
const { usage, quota } = await navigator.storage.estimate();
console.log(`${(usage / 1e6).toFixed(1)} MB of ${(quota / 1e6).toFixed(0)} MB`);

const persisted = await navigator.storage.persist();    // ask the browser not to evict (may prompt or auto-decide)
await navigator.storage.persisted();
```

By default, origin storage is **best-effort**: the browser may evict it under storage pressure (and Safari clears script-writable storage after ~7 days without interaction in some cases). Treat client storage as a **cache** of data that can be rebuilt from the server unless you request persistence and handle loss.

## Choosing storage

| Need | Choose |
|------|--------|
| Tiny preference (theme, language) | `localStorage` (or a cookie if the server needs it) |
| Per-tab temporary state | `sessionStorage` |
| Authentication session | **`HttpOnly` `Secure` `SameSite` cookie** set by the server |
| Structured/offline data, large or binary | IndexedDB |
| Cached responses, offline assets | Cache API + service worker |
| Files, SQLite | OPFS |
| Sync state across tabs | `storage` event or `BroadcastChannel` |
| Anything sensitive | do not store on the client (or encrypt with care, keys cannot be hidden from your own origin scripts) |

## Security checklist

- Assume **XSS** can read all JS-accessible storage
- Keep tokens in `HttpOnly` cookies; use short lifetimes and rotation
- Never store passwords, full card data, or secrets
- Validate and sanitize anything read back
- Use `Secure`, `SameSite`, and `__Host-` prefix for cookies
- Clear storage on logout (`localStorage.clear()`, caches, IndexedDB `deleteDatabase`)
- Avoid fingerprinting-style storage tricks; respect privacy settings and consent rules

```js
async function clearClientData() {
  localStorage.clear(); sessionStorage.clear();
  for (const { name } of await indexedDB.databases?.() ?? []) indexedDB.deleteDatabase(name);
  for (const key of await caches.keys()) await caches.delete(key);
}
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Putting tokens in `localStorage` | XSS steals them | `HttpOnly` cookies |
| Storing objects without `JSON.stringify` | `"[object Object]"` | Serialize/parse with error handling |
| Large data in `localStorage` | Blocks the main thread, 5 MB cap | IndexedDB |
| Assuming storage always works | Quota, private mode, blocked cookies | `try/catch` + fallbacks |
| Forgetting versions/migrations | Old data breaks new code | Versioned keys, IndexedDB upgrades |
| Big or many cookies | Slower requests | Small cookies, scope with `Path`/`Domain` |
| Expecting the `storage` event in the same tab | Fires only in other tabs | Call your handler directly too |
| Doing async work inside IndexedDB transactions | `TransactionInactiveError` | Keep transactions synchronous-ish and short |
| Treating client storage as permanent | Eviction, user clearing | Server as source of truth, request persistence |
| Not deleting data on logout | Privacy leak on shared devices | Clear storage and caches |

## Key takeaways

- Web storage is simple but synchronous, string-only and XSS-exposed; use it for small non-sensitive preferences
- Cookies travel to the server: use `HttpOnly`, `Secure`, `SameSite` for sessions
- IndexedDB (via `idb`/Dexie) is the tool for large, structured, offline data
- Treat client storage as evictable and handle errors and versioning

**Next:** [Web APIs](./09_web-apis.md)