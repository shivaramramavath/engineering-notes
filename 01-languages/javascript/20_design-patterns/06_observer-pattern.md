# Observer Pattern

The observer pattern lets one object (the **subject**) notify many others (**observers**) when something changes, without knowing who they are. Observers subscribe, the subject publishes, and neither side is hard-wired to the other.

It is the basis of DOM events, Node's `EventEmitter`, state stores, and most reactive UI libraries. Understanding the bare pattern makes all of those easier to debug.

**Prerequisites:** [Callbacks](../02_functions/05_callbacks.md), [Closures](../06_closures/01_closures.md), [Map and Set](../09_built-in-objects/06_map-and-set.md)

---

## Minimal Implementation

```js
class Subject {
  #observers = new Set();

  subscribe(fn) {
    this.#observers.add(fn);
    return () => this.#observers.delete(fn); // unsubscribe handle
  }

  notify(data) {
    for (const fn of [...this.#observers]) {
      try {
        fn(data);
      } catch (err) {
        queueMicrotask(() => { throw err; }); // report, but don't block other observers
      }
    }
  }
}
```

```js
const prices = new Subject();

const stop = prices.subscribe((p) => console.log("A saw", p));
prices.subscribe((p) => console.log("B saw", p));

prices.notify(101); // A saw 101, B saw 101
stop();
prices.notify(102); // B saw 102
```

Design choices in these few lines:

- **Return an unsubscribe function.** The caller doesn't need to keep a reference to the original callback.
- **`Set`** gives O(1) removal. The side effect is that subscribing the same function twice registers it once.
- **Iterate over a copy** (`[...this.#observers]`). If an observer unsubscribes itself (or another) during `notify`, mutating the live set mid-iteration would give surprising results.
- **Isolate errors.** One throwing observer shouldn't prevent the rest from running. Rethrowing in a microtask keeps the error visible instead of swallowing it ([Error Propagation](../10_error-handling/04_error-propagation.md)).

---

## A Practical Use: a Tiny Store

```js
function createStore(initial) {
  let state = initial;
  const subject = new Subject();

  return {
    get: () => state,
    set(next) {
      if (Object.is(next, state)) return; // no change, no notification
      state = next;
      subject.notify(state);
    },
    subscribe: (fn) => subject.subscribe(fn),
  };
}

const theme = createStore("light");
const off = theme.subscribe((t) => document.body.dataset.theme = t);
theme.set("dark");
off();
```

Skipping notification when nothing changed prevents redundant work and some infinite loops. Note that state is replaced, not mutated. If you mutate an object in place, `Object.is` can't tell anything changed ([Immutability](../07_functional-programming/02_immutability.md)).

---

## Built-in Observers

You will usually use one of these rather than writing your own.

```js
// DOM: EventTarget
const ctrl = new AbortController();
button.addEventListener("click", onClick, { signal: ctrl.signal });
ctrl.abort(); // removes the listener

// Node: EventEmitter
import { EventEmitter } from "node:events";
const bus = new EventEmitter();
bus.on("ready", () => console.log("ready"));
bus.emit("ready");
```

- Both give you named events, multiple listeners, and removal.
- `EventEmitter` treats the `"error"` event specially: emitting `"error"` with no listener throws.
- Listeners on an `EventEmitter` run **synchronously**, in registration order.

See [Events](../16_nodejs/05_events.md), [DOM Events](../14_dom-and-browser/04_events.md), and [Event Emitter](../23_real-world-patterns/07_event-emitter.md).

---

## Observer vs Pub/Sub

| | Observer | Pub/Sub |
| --- | --- | --- |
| Who knows whom | Observers subscribe directly to the subject | Publishers and subscribers only know a broker/topic |
| Coupling | Low | Lower |
| Typical form | `store.subscribe(fn)` | `bus.on("topic", fn)`, message queues |

Many libraries blur the two. The distinction matters mainly when you ask "can the publisher exist without knowing anything about its consumers?"

---

## Common Mistakes

- **Never unsubscribing.** The subject holds a strong reference to every observer, so long-lived subjects (stores, emitters, `window`) keep observers and everything they close over alive. This is the most common cause of leaks in this pattern ([Memory Leaks](../18_memory-and-garbage-collection/03_memory-leaks.md)). Unsubscribe in cleanup code, or use `AbortSignal`.
- **Mutating the observer list during notification** without copying it.
- **Letting one observer's error stop the rest.**
- **Notification loops.** An observer that updates the subject triggers `notify` again. Guard with a "did it change?" check.
- **Assuming async delivery.** The example above is synchronous. An observer that does heavy work blocks the code calling `notify`. Schedule slow work yourself.
- **Observers depending on call order.** Order is an implementation detail. If order matters, you want an explicit pipeline, not events.

## Debugging

- Log subscribe/unsubscribe counts. A count that only goes up is a leak.
- In Node, `emitter.listenerCount("event")` shows how many listeners exist. Node also prints a warning when more than 10 listeners are added to one event by default, which usually indicates a missing cleanup.
- In Chrome DevTools, selecting an element and checking its Event Listeners panel shows what is attached ([DevTools](../00_setup/04_devtools-and-debugging.md)).

---

## Quick Summary

- Subject keeps a list of observers and calls them on change. Observers don't know each other.
- Return an unsubscribe function, copy the list before iterating, isolate errors.
- Forgotten subscriptions are leaks. Always have a teardown path.
- For real work, prefer `EventTarget`/`EventEmitter` unless you need something they don't give you.

**Next:** [Adapter Pattern](./07_adapter-pattern.md)