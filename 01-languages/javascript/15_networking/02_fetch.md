# Fetch

`fetch()` is the modern, promise-based API for HTTP requests. It is built into browsers, Node.js 18+, Deno, Bun and workers.

```js
const response = await fetch("https://api.example.com/users/42");
const user = await response.json();
```

## The two-step model

1. `await fetch(...)` resolves when **headers arrive** with a `Response`
2. `await response.json()` (or `.text()` etc.) reads the **body**

## What `fetch` rejects on

| Situation | Result |
|-----------|--------|
| Network failure, DNS error, CORS block, aborted request | **rejects** (`TypeError` / `AbortError` / `TimeoutError`) |
| **HTTP 404, 500, ...** | **resolves** normally, with `response.ok === false` |

You must check status yourself:

```js
async function getJson(url, options) {
  const response = await fetch(url, options);
  if (!response.ok) {
    const body = await response.text().catch(() => "");
    throw new HttpError(response.status, response.statusText, body);
  }
  return response.json();
}

class HttpError extends Error {
  constructor(status, statusText, body) {
    super(`HTTP ${status} ${statusText}`);
    this.name = "HttpError";
    this.status = status;
    this.body = body;
  }
}
```

## Response object

| Member | Meaning |
|--------|---------|
| `ok` | status 200 to 299 |
| `status`, `statusText` | e.g. `404`, `"Not Found"` |
| `headers` | `Headers` object (`get`, `has`, `entries`) |
| `url`, `redirected`, `type` | final URL, whether redirected, `"basic"`/`"cors"`/`"opaque"` |
| `body` | `ReadableStream` of the body |
| `bodyUsed` | whether the body has been consumed |
| `json()`, `text()`, `blob()`, `arrayBuffer()`, `formData()`, `bytes()` | read the body (each returns a promise) |
| `clone()` | duplicate to read the body twice |

A body can be read **only once**:

```js
const res = await fetch(url);
const copy = res.clone();
const data = await res.json();
const raw = await copy.text();
```

## Sending data

### GET with query parameters

```js
const url = new URL("https://api.example.com/search");
url.searchParams.set("q", "ada lovelace");
url.searchParams.set("limit", "10");
const res = await fetch(url);                   // accepts a URL object or string
```

### POST JSON

```js
const res = await fetch("/api/users", {
  method: "POST",
  headers: { "Content-Type": "application/json", Accept: "application/json" },
  body: JSON.stringify({ name: "Ada", role: "admin" }),
});
```

### Form data and files

```js
const formData = new FormData(form);            // or build manually
formData.append("avatar", fileInput.files[0]);
await fetch("/api/upload", { method: "POST", body: formData });   // do NOT set Content-Type: the browser adds the multipart boundary
```

### URL-encoded

```js
await fetch("/login", { method: "POST", body: new URLSearchParams({ user: "ada", pass: "x" }) });   // content type set automatically
```

### Other body types

`string`, `Blob`, `File`, `ArrayBuffer`/typed arrays, `FormData`, `URLSearchParams`, `ReadableStream`.

## Options

```js
fetch(url, {
  method: "GET",                       // default
  headers: { Authorization: `Bearer ${token}` },
  body,                                // not allowed for GET/HEAD
  mode: "cors",                        // "cors" (default) | "same-origin" | "no-cors"
  credentials: "same-origin",          // "omit" | "same-origin" (default) | "include"
  cache: "default",                    // "no-store" | "reload" | "no-cache" | "force-cache" | "only-if-cached"
  redirect: "follow",                  // "follow" | "error" | "manual"
  referrerPolicy: "strict-origin-when-cross-origin",
  signal: abortSignal,                 // cancellation and timeouts
  keepalive: false,                    // allow the request to outlive the page (small payloads)
  priority: "auto",                    // "high" | "low" | "auto" (hint, newer browsers)
});
```

| Option | Common use |
|--------|-----------|
| `credentials: "include"` | send cookies to **another origin** (needs matching CORS headers) |
| `cache: "no-store"` | bypass the HTTP cache |
| `keepalive: true` | send analytics when the page unloads (also `navigator.sendBeacon`) |
| `redirect: "manual"` | inspect redirects yourself |

## Cancellation and timeouts

```js
const controller = new AbortController();
const promise = fetch(url, { signal: controller.signal });
controller.abort();                                 // rejects with AbortError

const res = await fetch(url, { signal: AbortSignal.timeout(5000) });                        // TimeoutError after 5 s
const res2 = await fetch(url, { signal: AbortSignal.any([userSignal, AbortSignal.timeout(8000)]) });

try { await fetch(url, { signal }); }
catch (err) {
  if (err.name === "AbortError") return;           // canceled on purpose
  if (err.name === "TimeoutError") return showTimeout();
  throw err;                                        // network failure (TypeError) or other
}
```

Fetch has **no default timeout**: add one. See [Cancellation](../11_asynchronous-javascript/08_cancellation-and-abort.md).

## Parallel and sequential requests

```js
const [user, orders] = await Promise.all([
  fetch("/api/user").then((r) => r.json()),
  fetch("/api/orders").then((r) => r.json()),
]);

const results = await Promise.allSettled(urls.map((u) => fetch(u).then((r) => r.json())));
```

Limit concurrency for large batches (`mapLimit`, see async patterns).

## Reading headers

```js
res.headers.get("content-type");        // case-insensitive
res.headers.get("x-request-id");
[...res.headers].forEach(([k, v]) => console.log(k, v));
```

In browsers, cross-origin responses expose only **CORS-safelisted** headers unless the server sends `Access-Control-Expose-Headers`.

## Streaming responses

```js
const res = await fetch("/api/big-export");
const reader = res.body.getReader();
const decoder = new TextDecoder();

let received = 0;
const total = Number(res.headers.get("content-length")) || 0;

while (true) {
  const { done, value } = await reader.read();       // value is a Uint8Array
  if (done) break;
  received += value.length;
  onProgress(total ? received / total : null);
  process(decoder.decode(value, { stream: true }));
}
```

Async iteration (modern runtimes):

```js
for await (const chunk of res.body) handle(chunk);
```

NDJSON streaming:

```js
const lines = res.body.pipeThrough(new TextDecoderStream()).pipeThrough(splitLines());   // custom TransformStream
```

## Download a file

```js
const res = await fetch("/report.pdf");
const blob = await res.blob();
const url = URL.createObjectURL(blob);
const a = Object.assign(document.createElement("a"), { href: url, download: "report.pdf" });
a.click();
URL.revokeObjectURL(url);
```

## Upload progress

`fetch` does not report **upload** progress in a widely supported way (streaming request bodies need HTTP/2 and `duplex: "half"`, limited support). Use `XMLHttpRequest` when you need progress bars:

```js
const xhr = new XMLHttpRequest();
xhr.open("POST", "/upload");
xhr.upload.onprogress = (e) => e.lengthComputable && setProgress(e.loaded / e.total);
xhr.onload = () => done(xhr.status);
xhr.send(formData);
```

## Fetch in Node.js

`fetch`, `Headers`, `Request`, `Response`, `FormData`, `AbortController` are global in Node 18+ (powered by undici).

```js
const res = await fetch("https://api.example.com/data", { signal: AbortSignal.timeout(10_000) });
```

Differences from the browser: no CORS, no cookie jar (manage cookies yourself), no CSP, and extras via `undici` (agents, connection pools, `ProxyAgent`).

## Fetch vs XMLHttpRequest vs libraries

| | `fetch` | XHR | axios / ky / ofetch |
|---|---------|-----|---------------------|
| Promise-based | yes | no (events) | yes |
| Streaming responses | yes | no | partly |
| Upload progress | limited | **yes** | via XHR adapter |
| Throws on HTTP errors | **no** | no | axios yes, ky yes |
| JSON helpers | manual | manual | built in |
| Interceptors / retries | DIY | DIY | built in |
| Timeout | via `AbortSignal` | `timeout` | built in |
| Dependency | none | none | small library |

A thin wrapper around `fetch` (see patterns) is often all you need.

## A small, solid helper

```js
async function api(path, { method = "GET", json, signal, headers, timeoutMs = 10_000, ...rest } = {}) {
  const response = await fetch(`/api${path}`, {
    method,
    headers: { Accept: "application/json", ...(json !== undefined && { "Content-Type": "application/json" }), ...headers },
    body: json !== undefined ? JSON.stringify(json) : undefined,
    signal: signal ? AbortSignal.any([signal, AbortSignal.timeout(timeoutMs)]) : AbortSignal.timeout(timeoutMs),
    credentials: "same-origin",
    ...rest,
  });

  if (response.status === 204) return null;
  const isJson = response.headers.get("content-type")?.includes("json");
  const payload = isJson ? await response.json() : await response.text();

  if (!response.ok) throw Object.assign(new Error(payload?.message ?? `HTTP ${response.status}`), { status: response.status, payload });
  return payload;
}

const user = await api("/users/42");
await api("/users", { method: "POST", json: { name: "Ada" } });
```

## Debugging fetch

- DevTools **Network** panel: status, headers, timing, payload, preview, initiator
- "Failed to fetch" / `TypeError` usually means network, DNS, mixed content, or **CORS**: check the Console for the real reason
- Replay with **Copy as fetch / cURL** from the Network panel
- Log `response.status`, `response.url`, and `await response.clone().text()` for unexpected bodies

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Assuming `fetch` rejects on 404/500 | Errors treated as data | Check `response.ok` |
| Calling `response.json()` on an empty or non-JSON body | `SyntaxError` | Check `204` / `Content-Type` first |
| Reading the body twice | `TypeError: body used already` | `clone()` or store the result |
| Setting `Content-Type` for `FormData` | Missing boundary breaks uploads | Let the browser set it |
| No timeout | Hung requests | `AbortSignal.timeout` |
| Not sending credentials cross-origin | Cookies missing | `credentials: "include"` (+ CORS config) |
| Building URLs with string concatenation | Encoding bugs | `URL` / `URLSearchParams` |
| Ignoring aborts as errors | Noisy logs | Handle `AbortError` explicitly |
| Sequential awaits for independent calls | Slow | `Promise.all` |
| Unvalidated JSON | Runtime errors later | Validate with a schema |
| Using `no-cors` expecting data | Opaque response, unreadable | Fix CORS on the server |

## Key takeaways

- `fetch` resolves on any HTTP response and rejects only for network failures and aborts: check `response.ok`
- Read the body once with the right method (`json`, `text`, `blob`, ...), `clone()` for multiple reads
- Add timeouts and cancellation with `AbortSignal`
- Let the browser set `Content-Type` for `FormData`, and build URLs with `URL`

**Next:** [Headers and CORS](./03_headers-and-cors.md)
