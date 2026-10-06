# Event Emitter

An event emitter lets one part of a program announce "something happened" without knowing who cares. Other parts subscribe with a callback. It's the publish/subscribe pattern at its smallest, and it's everywhere: Node streams, HTTP servers, and your own domain events (`order:paid`, `user:logged-out`).

## Prerequisites

- [Callbacks](../02_functions/05_callbacks.md)
- [Classes](../05_this-and-oop/05_classes.md)
- [Observer pattern](../20_design-patterns/06_observer-pattern.md) (the design-pattern view)
- [Node events](../16_nodejs/05_events.md) (the built-in implementation)

---

## Why Use One

```text
            ┌──────────────┐  emit('saved', doc)
  Editor ──►│   emitter    │──► autosave indicator
            └──────────────┘──► analytics
                            ──► search indexer
```

The editor doesn't import or know about those three consumers. Adding a fourth requires no change to the editor. That decoupling is the point. The cost: control flow becomes **implicit**, so it's harder to trace who reacts to what. Use emitters for genuine one-to-many notification, not as a replacement for ordinary function calls.

---

## A Minimal Implementation

```js
class Emitter {
  #handlers = new Map();   // event name → Set of listeners

  on(event, fn) {
    if (!this.#handlers.has(event)) this.#handlers.set(event, new Set());
    this.#handlers.get(event).add(fn);
    return () => this.off(event, fn);          // convenient unsubscribe function
  }

  off(event, fn) {
    this.#handlers.get(event)?.delete(fn);
  }

  once(event, fn) {
    const unsubscribe = this.on(event, (...args) => {
      unsubscribe();
      fn(...args);
    });
    return unsubscribe;
  }

  emit(event, ...args) {
    const listeners = this.#handlers.get(event);
    if (!listeners) return false;
    for (const fn of [...listeners]) fn(...args);   // iterate a copy
    return true;
  }
}

const bus = new Emitter();
const stop = bus.on('saved', (doc) => console.log('saved', doc.id));
bus.emit('saved', { id: 1 });
stop();                                        // unsubscribe
```

Design decisions worth understanding:

- **Iterate over a copy** in `emit`. A listener may call `off` (as `once` does) while the loop runs; mutating the live `Set` mid-iteration can skip or repeat listeners.
- A `Set` means the same function registered twice only runs once. Node's emitter allows duplicates; pick deliberately.
- `on` **returns an unsubscribe function**, which is much harder to get wrong than keeping a reference to the exact handler.
- `emit` here is **synchronous**: listeners run, in registration order, before `emit` returns. This is also how Node's `EventEmitter` behaves.
- Limitation of this `once`: you can't remove it via `off(event, originalFn)`, only via the returned function.

### Isolating listener errors

As written, a throwing listener stops the remaining ones and the exception propagates to the emitter's caller. Decide the policy:

```js
emit(event, ...args) {
  for (const fn of [...(this.#handlers.get(event) ?? [])]) {
    try { fn(...args); }
    catch (err) { queueMicrotask(() => { throw err; }); }   // report, but don't break other listeners
  }
}
```

---

## Node's `EventEmitter`

In Node, prefer the built-in:

```js
import { EventEmitter, once } from 'node:events';

class Job extends EventEmitter {
  run() {
    this.emit('start');
    // ... work ...
    this.emit('done', { ok: true });
  }
}

const job = new Job();
job.on('done', (result) => console.log(result));
job.run();

// Await the next occurrence of an event as a promise
const [result] = await once(job, 'done');
```

Behaviors that matter in practice:

| Behavior | Detail |
|---|---|
| **`'error'` is special** | Emitting `'error'` with **no listener** throws the error (and can crash the process). Always attach an `'error'` listener on emitters that can fail (streams, sockets, your own). |
| **Max listeners warning** | By default, more than 10 listeners for one event on one emitter prints a `MaxListenersExceededWarning`. It's a leak detector, so investigate before raising it with `setMaxListeners`. |
| **Synchronous emit** | Listeners run immediately, in order. Async listeners aren't awaited, and a rejected promise inside one is an unhandled rejection unless handled (Node has a `captureRejections` option). |
| **`once`/`prependListener`** | `emitter.once` auto-removes after the first call; `prependListener` runs first. |
| **`events.once(emitter, name)`** | Promise for the next event; rejects if `'error'` is emitted first. |

See [Node events](../16_nodejs/05_events.md) for the full API.

In browsers, the equivalent is `EventTarget` / DOM events ([Events](../14_dom-and-browser/04_events.md)), and `new EventTarget()` can be used as a lightweight emitter.

---

## Practical Usage

**Domain events in an app:**

```js
const events = new Emitter();

// payments module
events.emit('order:paid', { orderId: 42, total: 1999 });

// elsewhere, independent modules react
events.on('order:paid', sendReceiptEmail);
events.on('order:paid', updateInventory);
```

**Wrapping something callback-ish into events**, e.g. progress reporting:

```js
function download(url) {
  const emitter = new EventEmitter();
  (async () => {
    try {
      for await (const chunk of stream(url)) emitter.emit('progress', chunk.length);
      emitter.emit('done');
    } catch (err) {
      emitter.emit('error', err);
    }
  })();
  return emitter;
}
```

**When *not* to use an emitter:** if there is exactly one consumer and you need its result or want clear error propagation, a plain callback, promise, or async iterator is simpler and more traceable. A promise represents **one** future value; an emitter represents **many** occurrences over time.

---

## Common Mistakes

| Mistake | Fix |
|---|---|
| Adding listeners and never removing them (components, per-request handlers) → memory leak, duplicate handling | Return/call unsubscribe in cleanup; watch for max-listener warnings |
| Registering a listener inside another handler repeatedly | Use `once` or register once outside |
| No `'error'` listener in Node | Add one; otherwise errors throw |
| Assuming `emit` is async | It's synchronous: a slow listener blocks the emitter's caller |
| Awaiting inside listeners and expecting `emit` to wait | It doesn't; use `Promise.all` over handlers in a custom `emitAsync` if you need that |
| Emitting before listeners are attached (event is lost) | Attach first, or use a promise/state for "already happened" |
| Typo'd event names (`'sved'`) fail silently | Centralize names in constants (or use typed emitters in TypeScript) |
| Mutating the listener list during `emit` | Iterate over a copy |

### Debugging

- `emitter.listenerCount('event')` and `emitter.eventNames()` (Node) show what's registered, handy for leak hunting.
- Log in a wildcard-style wrapper during development to trace emission order.
- A growing listener count over time (per request, per component mount) is the classic leak signature. See [Memory leaks](../18_memory-and-garbage-collection/03_memory-leaks.md).

---

## Testing

```js
import { vi, it, expect } from 'vitest';

it('calls listeners in order with the payload', () => {
  const bus = new Emitter();
  const calls = [];
  bus.on('x', (v) => calls.push(['a', v]));
  bus.on('x', (v) => calls.push(['b', v]));
  bus.emit('x', 1);
  expect(calls).toEqual([['a', 1], ['b', 1]]);
});

it('once fires a single time', () => {
  const bus = new Emitter();
  const fn = vi.fn();
  bus.once('x', fn);
  bus.emit('x'); bus.emit('x');
  expect(fn).toHaveBeenCalledTimes(1);
});

it('unsubscribe stops delivery', () => {
  const bus = new Emitter();
  const fn = vi.fn();
  const off = bus.on('x', fn);
  off();
  bus.emit('x');
  expect(fn).not.toHaveBeenCalled();
});
```

---

## Quick Summary

- An emitter decouples producers from consumers: `on`, `off`, `once`, `emit`.
- Make `on` return an **unsubscribe** function; iterate over a **copy** when emitting.
- Node's `EventEmitter`: synchronous `emit`, special `'error'` event, max-listener leak warning, `events.once()` for promises.
- Guard against leaks: always remove listeners when their owner goes away.
- Don't use emitters when a promise or direct call expresses the flow more clearly.

**Next:** [Caching](./08_caching.md)
