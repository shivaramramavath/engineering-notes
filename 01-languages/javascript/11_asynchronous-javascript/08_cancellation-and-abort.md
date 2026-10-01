# Cancellation and Abort

Promises cannot be canceled, but the **operations behind them can** if they cooperate. The standard mechanism is **`AbortController`** / **`AbortSignal`**, available in browsers, Node, Deno and Bun.

```
controller.abort()  ──signal──►  fetch / timers / streams / your code stops and rejects
```

## The basics

```js
const controller = new AbortController();
const { signal } = controller;

const promise = fetch("/api/slow", { signal });

controller.abort();                              // cancel the request
try { await promise; }
catch (err) {
  if (err.name === "AbortError") console.log("canceled");
}
```

| Object | Purpose |
|--------|---------|
| `AbortController` | creates a signal and holds `abort(reason?)` |
| `AbortSignal` | read-only handle passed to operations: `aborted`, `reason`, `onabort`, `throwIfAborted()`, `addEventListener("abort", ...)` |

A signal can only be aborted **once**; create a new controller for each cancelable operation.

## Abort reasons

```js
controller.abort();                              // reason: DOMException "AbortError"
controller.abort(new Error("user navigated"));   // custom reason
controller.abort("timeout");                     // any value

signal.aborted;                                  // true
signal.reason;                                   // the reason
```

`fetch` rejects with `signal.reason` (an `AbortError` by default).

## Built-in helpers

```js
AbortSignal.timeout(5000);                       // aborts with TimeoutError after 5 s
AbortSignal.abort();                             // already-aborted signal
AbortSignal.any([userSignal, AbortSignal.timeout(5000)]);   // aborts when ANY input aborts
```

```js
const res = await fetch(url, {
  signal: AbortSignal.any([controller.signal, AbortSignal.timeout(8000)]),
});
```

Check `err.name`: `"AbortError"` (manual) vs `"TimeoutError"` (from `AbortSignal.timeout`).

## APIs that accept signals

| API | Usage |
|-----|-------|
| `fetch` | `fetch(url, { signal })` |
| `addEventListener` | `{ signal }` removes the listener on abort |
| Node `fs/promises` | `readFile(path, { signal })` |
| Node `timers/promises` | `setTimeout(ms, value, { signal })` |
| Node `events.on` / `once` | `{ signal }` |
| Node `child_process`, `http`, streams | `{ signal }` |
| Web streams | `pipeTo(dest, { signal })` |
| `Worker` / ReadableStream sources | custom handling |

```js
// one signal removes many listeners
const ac = new AbortController();
window.addEventListener("resize", onResize, { signal: ac.signal });
window.addEventListener("scroll", onScroll, { signal: ac.signal });
document.addEventListener("keydown", onKey, { signal: ac.signal });
ac.abort();                                      // all three removed
```

## Making your own functions cancelable

```js
async function processAll(items, { signal } = {}) {
  for (const item of items) {
    signal?.throwIfAborted();                    // throws signal.reason if aborted
    await processOne(item, { signal });          // pass it down
  }
}
```

Abortable sleep:

```js
function sleep(ms, signal) {
  return new Promise((resolve, reject) => {
    if (signal?.aborted) return reject(signal.reason);
    const id = setTimeout(() => { signal?.removeEventListener("abort", onAbort); resolve(); }, ms);
    const onAbort = () => { clearTimeout(id); reject(signal.reason); };
    signal?.addEventListener("abort", onAbort, { once: true });
  });
}
```

Wrap a non-cancelable promise so the **caller** stops waiting (the work itself continues):

```js
function abortable(promise, signal) {
  if (!signal) return promise;
  return new Promise((resolve, reject) => {
    if (signal.aborted) return reject(signal.reason);
    const onAbort = () => reject(signal.reason);
    signal.addEventListener("abort", onAbort, { once: true });
    promise.then(resolve, reject).finally(() => signal.removeEventListener("abort", onAbort));
  });
}
```

## "Latest wins" (search-as-you-type)

Cancel the previous request when a new one starts, so stale results never overwrite fresh ones.

```js
let current;

async function search(query) {
  current?.abort();
  current = new AbortController();
  try {
    const res = await fetch(`/search?q=${encodeURIComponent(query)}`, { signal: current.signal });
    render(await res.json());
  } catch (err) {
    if (err.name !== "AbortError") showError(err);
  }
}
```

Even without a signal-aware API, guard with a request id:

```js
let latest = 0;
async function load(id) {
  const mine = ++latest;
  const data = await fetchData(id);
  if (mine !== latest) return;                   // a newer call superseded this one
  render(data);
}
```

## Cleanup patterns

### UI components (React example)

```js
useEffect(() => {
  const ac = new AbortController();
  load({ signal: ac.signal }).then(setData).catch((e) => { if (e.name !== "AbortError") setError(e); });
  return () => ac.abort();                       // cancel on unmount or dependency change
}, [id]);
```

### Timeouts that actually stop work

```js
// race: loser keeps running (bad)
await Promise.race([work(), timeout(3000)]);

// signal: work stops (good)
await work({ signal: AbortSignal.timeout(3000) });
```

### Server request cancellation

```js
app.get("/report", async (req, res) => {
  const ac = new AbortController();
  req.on("close", () => ac.abort());             // client disconnected
  const data = await buildReport({ signal: ac.signal });
  res.json(data);
});
```

## Cancel a group

```js
const ac = new AbortController();
const tasks = urls.map((u) => fetch(u, { signal: ac.signal }));
const first = await Promise.any(tasks);
ac.abort();                                      // cancel the slower ones
```

## Cancel async iteration and streams

```js
const ac = new AbortController();
for await (const msg of on(emitter, "message", { signal: ac.signal })) { ... }   // abort ends the loop with an AbortError

const reader = res.body.getReader();
await reader.cancel();                           // cancel a web stream
```

## Cancellation errors are not failures

```js
try { await op({ signal }); }
catch (err) {
  if (signal.aborted) return;                    // intended: do not log as an error
  throw err;
}
```

## Cooperative cancellation limits

- Abort is a **request**: the operation must check the signal
- Already-completed side effects are **not undone**: design for idempotence or compensation
- CPU-bound loops need explicit `throwIfAborted()` checks (or run in a worker you can `terminate()`)
- Abort listeners can leak if added without removal: use `{ once: true }` and remove them when done

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Using `Promise.race` for timeouts only | Work keeps running | `AbortSignal.timeout` |
| Reusing an aborted controller | Immediately aborted forever | New controller per operation |
| Reporting `AbortError` as a failure | Noisy errors | Check `signal.aborted` / `err.name` |
| Not passing the signal down the call chain | Inner work ignores abort | Thread `signal` through |
| Leaking `abort` listeners | Memory leaks | `{ once: true }`, remove on settle |
| Assuming abort undoes side effects | Partial writes remain | Idempotency, transactions |
| Awaiting stale responses | Out-of-order UI updates | Abort previous, or compare request ids |
| No cleanup on unmount/navigation | Wasted work, state updates after teardown | Abort in cleanup |

## Key takeaways

- Cancel with `AbortController`: pass `signal` into APIs and your own functions
- `AbortSignal.timeout()` and `AbortSignal.any()` compose timeouts and manual cancel
- Treat aborts as expected control flow, not errors to report
- Cancellation is cooperative and does not roll back completed work

**Next:** [Async Patterns](./09_async-patterns.md)
